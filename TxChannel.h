
#ifndef BITEMITTER_H_
#define BITEMITTER_H_

#include <stdint.h>
#include <stdlib.h>
#include "hardware/clocks.h"
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "GPStime.h"




typedef struct
{
    //enum PioDcoMode _mode;      /* Running mode. */

 //   PIO _pio;                   /* Worker PIO on this DCO. */
  //  int _gpio;                  /* Pico' GPIO for DCO output. */

 //   pio_sm_config _pio_sm;      /* Worker PIO parameter. */
  //  int _ism;                   /* Index of state maschine. */
   // int _offset;                /* Worker PIO u-program offset. */

    int32_t _frq_cycles_per_pi; /* CPU CLK cycles per PI. */

    uint32_t _ui32_pioreg[8];   /* Shift register to PIO. */

    uint32_t _clkfreq_hz;       /* CPU CLK freq, Hz. */

    GPStimeContext *_pGPStime;  /* Ptr to GPS time context. */

    uint32_t _ui32_frq_hz;      /* Working freq, Hz. */
    int32_t _ui32_frq_millihz;  /* Working freq additive shift, mHz. */
    int _is_enabled;

} PioDco;


typedef struct
{
    uint64_t _tm_future_call;
    uint32_t _bit_period_us;

    uint8_t _timer_alarm_num;

    uint8_t _ix_input, _ix_output;
    uint8_t _pbyte_buffer[256];

    PioDco *_p_oscillator;
    uint32_t _u32_dialfreqhz;
    int _i_tx_gpio;

} TxChannelContext;

TxChannelContext *TxChannelInit(const uint32_t bit_period_us, 
                                uint8_t timer_alarm_num, PioDco *pDCO);

uint8_t TxChannelPending(TxChannelContext *pctx);
int TxChannelPush(TxChannelContext *pctx, uint8_t *psrc, int n);
int TxChannelPop(TxChannelContext *pctx, uint8_t *pdst);
void TxChannelClear(TxChannelContext *pctx);
int32_t PioDCOGetFreqShiftMilliHertz(const PioDco *pdco, uint64_t u64_desired_frq_millihz);

#endif
