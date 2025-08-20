
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "hardware/clocks.h"
#include "pico/stdlib.h"
#include "defines.h"

#define BUFFER_SIZE 256

static char logBuffer[BUFFER_SIZE] = {0};



void StampPrintf(const char* pformat, ...)
{
    static uint32_t sTick = 0;
    if(!sTick)
    {
        stdio_init_all();
    }

    uint64_t tm_us = to_us_since_boot(get_absolute_time());
    
    const uint32_t tm_day = (uint32_t)(tm_us / 86400000000ULL);
    tm_us -= (uint64_t)tm_day * 86400000000ULL;

    const uint32_t tm_hour = (uint32_t)(tm_us / 3600000000ULL);
    tm_us -= (uint64_t)tm_hour * 3600000000ULL;

    const uint32_t tm_min = (uint32_t)(tm_us / 60000000ULL);
    tm_us -= (uint64_t)tm_min * 60000000ULL;
    
    const uint32_t tm_sec = (uint32_t)(tm_us / 1000000ULL);
    tm_us -= (uint64_t)tm_sec * 1000000ULL;

    char timestamp[64];  //let's create timestamp
    snprintf(timestamp, sizeof(timestamp), "%02lud%02lu:%02lu:%02lu.%06llu [%04lu] ", tm_day, tm_hour, tm_min, tm_sec, tm_us, sTick++);

    va_list argptr;
    va_start(argptr, pformat);
    char message[BUFFER_SIZE];
    vsnprintf(message, sizeof(message), pformat, argptr); //let's format the message 
    va_end(argptr);
    strncat(logBuffer, timestamp, BUFFER_SIZE - strlen(logBuffer) - 1);
    strncat(logBuffer, message, BUFFER_SIZE - strlen(logBuffer) - 1);
    strncat(logBuffer, "\n", BUFFER_SIZE - strlen(logBuffer) - 1);
    
}

/// @brief Outputs the content of the log buffer to stdio (UART and/or USB)
/// @brief Direct output to UART is very slow so we will do it in CPU idle times
/// @brief and not in time critical functions
void DoLogPrint()
{
    if (logBuffer[0] != '\0')
    {
        printf("%s", logBuffer);
        logBuffer[0] = '\0';  // Clear the buffer

    }

}