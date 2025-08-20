
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "defines.h"



typedef struct
{
    uint8_t _u8_is_solution_active;             /* A navigation solution is valid. */
	uint8_t sat_count;
    char _u8_last_digit_minutes;                /* First digit of the minutes. Really, this is the only thing needed to sequence messages. */
    char _u8_last_digit_hour;  
	char _full_time_string[7];
	uint32_t hour;
	uint32_t minute;
    int64_t _i64_lat_100k, _i64_lon_100k;       /* The lat, lon, degrees, multiplied by 1e5. */
    uint32_t _u32_nmea_gprmc_count;             /* The count of $GPRMC sentences received */
    uint8_t _ix_last;                           /* An index of last write to sliding window. */
    int64_t _i32_freq_shift_ppb;                /* Calcd frequency shift, parts per billion. */
	
} GPStimeData;

typedef struct
{
    int _uart_id;
    int _uart_baudrate;
    int _pps_gpio;

    GPStimeData _time_data;

    uint8_t _pbytebuff[256];  
    uint8_t _u8_ixw;
    uint8_t _is_sentence_ready;
    int32_t _i32_error_count;
    float _altitude;   //altitude in metesr
	uint8_t user_setup_menu_active;
	uint8_t forced_XMIT_on;
	int8_t temp_in_Celsius;
	int8_t verbosity;

} GPStimeContext;

GPStimeContext *GPStimeInit(int uart_id, int uart_baud, int pps_gpio, uint32_t clock_speed);
void GPStimeDestroy(GPStimeContext **pp);
int parse_GPS_data(GPStimeContext *pg);
void RAM (GPStimePPScallback)(uint gpio, uint32_t events);
void RAM (GPStimeUartRxIsr)();
void GPStimeDump(const GPStimeData *pd);

inline uint64_t GetUptime64(void)
{
    const uint32_t lo = timer_hw->timelr;
    const uint32_t hi = timer_hw->timehr;

    return ((uint64_t)hi << 32U) | lo;
}

inline uint32_t GetTime32(void)
{
    return timer_hw->timelr;
}

inline uint32_t PicoU64timeToSeconds(uint64_t u64tm)
{
    return u64tm / 1000000U;    // No rounding deliberately!
}

inline uint32_t DecimalStr2ToNumber(const char *p)
{
    return 10U * (p[0] - '0') + (p[1] - '0');
}

inline void PRN32(uint32_t *val)
{ 
    *val ^= *val << 13;
    *val ^= *val >> 17;
    *val ^= *val << 5;
}