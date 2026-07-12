#pragma once
#include <stdint.h>

// Binary GPS log record — 32 bytes, packed, XOR checksum over bytes [0..sizeof-2].
// Flash layout:  sector 65 = metadata,  sectors 66-320 = records.
// Capacity: 255 sectors × 128 records/sector = 32,640 records.

#define GPSLOG_RECORD_BYTES  32u
#define GPSLOG_TOTAL_SLOTS   (255u * (4096u / GPSLOG_RECORD_BYTES))   // 32 640

typedef struct __attribute__((packed)) {
    int32_t  lat_1e5;       //  4  latitude  × 1e5 (signed, decimal degrees)
    int32_t  lon_1e5;       //  4  longitude × 1e5 (signed, decimal degrees)
    uint16_t altitude_m;    //  2  altitude + 1000 m offset (range -1000..64535 m)
    uint16_t ttf_s;         //  2  time-to-first-fix, seconds
    uint8_t  hour;          //  1  GPS UTC hour
    uint8_t  minute;        //  1  GPS UTC minute
    uint8_t  second;        //  1  GPS UTC second
    uint8_t  day;           //  1  GPS UTC day   (from RMC)
    uint8_t  month;         //  1  GPS UTC month (from RMC)
    uint8_t  year;          //  1  GPS UTC year, 2-digit (from RMC, e.g. 25 = 2025)
    uint8_t  sats;          //  1  satellites used in fix
    uint8_t  sats_in_view;  //  1  satellites in view (same as sats; GSV not parsed)
    int8_t   temp_c;        //  1  temperature, °C
    uint8_t  volt_10ths;    //  1  battery voltage × 10  (e.g. 33 = 3.3 V)
    uint8_t  flags;         //  1  bit 0 = GPS fix valid
    uint8_t  checksum;      //  1  XOR of bytes 0 .. sizeof(record)-2
    uint8_t  _pad[8];       //  8  reserved, zeroed
} GpsLogRecord;             // = 32 bytes

_Static_assert(sizeof(GpsLogRecord) == GPSLOG_RECORD_BYTES, "GpsLogRecord must be 32 bytes");

// mode 1 = circular (evict oldest sector when full)
// mode 2 = stop when full
void gpslog_stats(uint32_t *used_out, uint32_t *total_out);
int  gpslog_append(const GpsLogRecord *r, int mode);
void gpslog_dump_csv(void);
void gpslog_wipe(void);
