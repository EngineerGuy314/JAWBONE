#include "defines.h"
#include "gpslog.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include <stddef.h>
#include <string.h>
#include <stdio.h>

#define GPSLOG_MAGIC         0x4C4F4701u   // 'LOG\x01'

// Flash byte offsets from start of flash (NOT from XIP_BASE).
// Add XIP_BASE only when reading via the memory map.
#define META_FLASH_OFF       (65u * FLASH_SECTOR_SIZE)   // one sector for metadata
#define DATA_FLASH_OFF       (66u * FLASH_SECTOR_SIZE)   // 255 sectors for records
#define DATA_SECTORS         255u
#define RECORDS_PER_SECTOR   (FLASH_SECTOR_SIZE / GPSLOG_RECORD_BYTES)   // 128

// Metadata block — padded to 256 bytes (one flash page) so write is page-aligned.
typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint32_t head;    // oldest valid record index (0-based, < GPSLOG_TOTAL_SLOTS)
    uint32_t tail;    // next-write slot index
    uint32_t count;   // records currently stored
    uint8_t  _pad[240];
} GpsLogMeta;

_Static_assert(sizeof(GpsLogMeta) == 256, "GpsLogMeta must be one flash page");

// Separate static RAM buffers so neither sits on the stack (4 KB each).
static uint8_t s_meta_buf[FLASH_SECTOR_SIZE];
static uint8_t s_data_buf[FLASH_SECTOR_SIZE];

// ── helpers ──────────────────────────────────────────────────────────────────

static uint8_t compute_checksum(const GpsLogRecord *r) {
    const uint8_t *b = (const uint8_t *)r;
    uint8_t cs = 0;
    for (size_t i = 0; i < offsetof(GpsLogRecord, checksum); i++)
        cs ^= b[i];
    return cs;
}

static void meta_read(GpsLogMeta *m) {
    memcpy(m, (const void *)(XIP_BASE + META_FLASH_OFF), sizeof(*m));
}

static bool meta_valid(const GpsLogMeta *m) {
    return m->magic == GPSLOG_MAGIC
        && m->head  <  GPSLOG_TOTAL_SLOTS
        && m->tail  <  GPSLOG_TOTAL_SLOTS
        && m->count <= GPSLOG_TOTAL_SLOTS;
}

static void meta_write(const GpsLogMeta *m) {
    memset(s_meta_buf, 0xFF, sizeof(s_meta_buf));
    memcpy(s_meta_buf, m, sizeof(*m));
    uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(META_FLASH_OFF, FLASH_SECTOR_SIZE);
    flash_range_program(META_FLASH_OFF, s_meta_buf, FLASH_SECTOR_SIZE);
    restore_interrupts(ints);
}

static void slot_read(uint32_t slot, GpsLogRecord *r) {
    uint32_t off = DATA_FLASH_OFF + slot * GPSLOG_RECORD_BYTES;
    memcpy(r, (const void *)(XIP_BASE + off), sizeof(*r));
}

// Read-modify-erase-write the sector that holds `slot`.
static void slot_write(uint32_t slot, const GpsLogRecord *r) {
    uint32_t sec     = slot / RECORDS_PER_SECTOR;
    uint32_t in_sec  = slot % RECORDS_PER_SECTOR;
    uint32_t sec_off = DATA_FLASH_OFF + sec * FLASH_SECTOR_SIZE;

    memcpy(s_data_buf, (const void *)(XIP_BASE + sec_off), FLASH_SECTOR_SIZE);
    memcpy(s_data_buf + in_sec * GPSLOG_RECORD_BYTES, r, GPSLOG_RECORD_BYTES);

    uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(sec_off, FLASH_SECTOR_SIZE);
    flash_range_program(sec_off, s_data_buf, FLASH_SECTOR_SIZE);
    restore_interrupts(ints);
}

static void data_sector_erase(uint32_t sec_idx) {
    uint32_t sec_off = DATA_FLASH_OFF + sec_idx * FLASH_SECTOR_SIZE;
    uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(sec_off, FLASH_SECTOR_SIZE);
    restore_interrupts(ints);
}

// ── public API ────────────────────────────────────────────────────────────────

void gpslog_stats(uint32_t *used_out, uint32_t *total_out) {
    GpsLogMeta m;
    meta_read(&m);
    *total_out = GPSLOG_TOTAL_SLOTS;
    *used_out  = meta_valid(&m) ? m.count : 0;
}

// Returns 0 on success, -1 if log is full and mode==2 (stop).
int gpslog_append(const GpsLogRecord *r_in, int mode) {
    GpsLogMeta m;
    meta_read(&m);
    if (!meta_valid(&m)) {
        memset(&m, 0xFF, sizeof(m));
        m.magic = GPSLOG_MAGIC;
        m.head  = 0;
        m.tail  = 0;
        m.count = 0;
    }

    if (m.count >= GPSLOG_TOTAL_SLOTS) {
        if (mode == 2) {
            printf("GPS log full (mode 2: stop-when-full). Record not stored.\n");
            return -1;
        }
        // Mode 1 circular: erase the sector containing head, advance head past it.
        uint32_t head_sec    = m.head / RECORDS_PER_SECTOR;
        uint32_t head_in_sec = m.head % RECORDS_PER_SECTOR;
        uint32_t evicted     = RECORDS_PER_SECTOR - head_in_sec;
        data_sector_erase(head_sec);
        m.head  = ((head_sec + 1) * RECORDS_PER_SECTOR) % GPSLOG_TOTAL_SLOTS;
        m.count = (m.count >= evicted) ? m.count - evicted : 0;
    }

    GpsLogRecord r = *r_in;
    r.checksum = compute_checksum(&r);
    slot_write(m.tail, &r);

    m.tail  = (m.tail + 1) % GPSLOG_TOTAL_SLOTS;
    m.count++;
    meta_write(&m);

    uint32_t free_slots = GPSLOG_TOTAL_SLOTS - m.count;
    printf("GPS logged (%lu rec. / %.0f kB used, %lu rec. / %.0f kB avail.)\n",
           (unsigned long)m.count,
           (double)m.count     * GPSLOG_RECORD_BYTES / 1024.0,
           (unsigned long)free_slots,
           (double)free_slots  * GPSLOG_RECORD_BYTES / 1024.0);
    return 0;
}

void gpslog_dump_csv(void) {
    GpsLogMeta m;
    meta_read(&m);
    if (!meta_valid(&m) || m.count == 0) {
        printf("GPS log: no records to dump.\n");
        return;
    }
    printf("seq,lat,lon,altitude_m,date,time,sats,sats_in_view,"
           "temp_c,volts,ttf_s,fix_ok\n");

    uint32_t slot = m.head;
    for (uint32_t i = 0; i < m.count; i++) {
        GpsLogRecord r;
        slot_read(slot, &r);
        uint8_t cs = compute_checksum(&r);
        if (cs != r.checksum) {
            printf("# slot %lu: checksum error (stored=0x%02X computed=0x%02X)\n",
                   (unsigned long)slot, r.checksum, cs);
        } else {
            printf("%lu,%.5f,%.5f,%d,20%02u-%02u-%02u,%02u:%02u:%02u,%u,%u,%d,%.1f,%u,%u\n",
                   (unsigned long)i,
                   (double)r.lat_1e5  * 1e-7,
                   (double)r.lon_1e5  * 1e-7,
                   (int)r.altitude_m - 1000,
                   r.year, r.month, r.day,
                   r.hour, r.minute, r.second,
                   r.sats,
                   r.sats_in_view,
                   (int)r.temp_c,
                   r.volt_10ths * 0.1,
                   (unsigned)r.ttf_s,
                   r.flags & 1u);
        }
        slot = (slot + 1) % GPSLOG_TOTAL_SLOTS;
    }
    printf("# %lu records total\n", (unsigned long)m.count);
}

void gpslog_wipe(void) {
    // Erase metadata sector and all data sectors in one call.
    printf("Wiping GPS log (%u sectors)...\n", DATA_SECTORS + 1u);
    uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(META_FLASH_OFF, (DATA_SECTORS + 1u) * FLASH_SECTOR_SIZE);
    restore_interrupts(ints);
    printf("GPS log wiped.\n");
}
