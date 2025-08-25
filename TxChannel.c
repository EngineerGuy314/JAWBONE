

#include "defines.h"

static TxChannelContext *spTX = NULL;

/**
ISR initiated by timer alarm interupt each WSPR bit period. 
 */
static void RAM (TxChannelISR)(void)
{
    RfGenStruct *RfGen = spTX->_p_oscillator;

    uint8_t byte;
    const int n2send = TxChannelPop(spTX, &byte);
    if(n2send)
    {
			// sets frequency after doing modulation with the value in "byte"
			si5351aSetFrequency((uint64_t)((uint64_t)(((float)spTX->_u32_dialfreqhz + (float)byte * 12000.f / 8192.f)*100)));			
    }

    spTX->_tm_future_call += spTX->_bit_period_us; //next alarm calculation

EXIT:
    hw_clear_bits(&timer_hw->intr, 1U<<spTX->_timer_alarm_num); //clear interupt flag
    timer_hw->alarm[spTX->_timer_alarm_num] = (uint32_t)spTX->_tm_future_call; //set next alarm
}

/// @brief Initializes a TxChannel context. Starts timer alarm ISR.
/// @param bit_period_us Period of data bits, BPS speed = 1e6/bit_period_us.
/// @param timer_alarm_num Pico-specific hardware timer resource id.
/// @param pDCO Ptr to oscillator.
/// @return the Context.
TxChannelContext *TxChannelInit(const uint32_t bit_period_us, uint8_t timer_alarm_num,RfGenStruct *RfGen)
{

    TxChannelContext *p = calloc(1, sizeof(TxChannelContext));

    p->_bit_period_us = bit_period_us;
    p->_timer_alarm_num = timer_alarm_num;
	p->_p_oscillator = RfGen;
    spTX = p;

    hw_set_bits(&timer_hw->inte, 1U << p->_timer_alarm_num);
    irq_set_exclusive_handler(TIMER_IRQ_0, TxChannelISR);
    irq_set_priority(TIMER_IRQ_0, 0x00);
    irq_set_enabled(TIMER_IRQ_0, true);

    p->_tm_future_call = timer_hw->timerawl + 20000LL;
    timer_hw->alarm[p->_timer_alarm_num] = (uint32_t)p->_tm_future_call;

    return p;
}

/// @brief Gets a count of remaining bytes to send.
/// @param pctx Context.
/// @return A count of bytes.
uint8_t TxChannelPending(TxChannelContext *pctx)
{
    return 256L + (int)pctx->_ix_input - (int)pctx->_ix_output;
}

/// @brief Push a number of bytes to the transmission buffer.
/// @param pctx Context.
/// @param psrc Ptr to buffer to send.
/// @param n A count of bytes to send.
/// @return A count of bytes has been sent (might be lower than n).
int TxChannelPush(TxChannelContext *pctx, uint8_t *psrc, int n)
{
    uint8_t *pdst = pctx->_pbyte_buffer;
    while(n-- && pctx->_ix_input != pctx->_ix_output)
    {
        pdst[pctx->_ix_input++] = *psrc++;
    }

    return n;
}

/// @brief Retrieves a next byte from FIFO.
/// @param pctx Context.
/// @param pdst Ptr to write a byte.
/// @return 1 if a byte has been retrived, or 0.
int TxChannelPop(TxChannelContext *pctx, uint8_t *pdst)
{
    if(pctx->_ix_input != pctx->_ix_output)
    {
        *pdst = pctx->_pbyte_buffer[pctx->_ix_output++];
        return 1;
    }

    return 0;  // no more bytes to transmit
}

/// @brief Clears transmission buffer completely by setting write & read indexes to 0.
/// @param pctx Context.
void TxChannelClear(TxChannelContext *pctx)
{
    pctx->_ix_input = pctx->_ix_output = 0;
}

/*int32_t PioDCOGetFreqShiftMilliHertz(const PioDco *pdco, uint64_t u64_desired_frq_millihz)
{
    if(!pdco->_pGPStime)
    {
        return 0U;
    }

    static int64_t i64_last_correction = 0;
    const int64_t dt = pdco->_pGPStime->_time_data._i32_freq_shift_ppb; // Parts per billion.    //Used here
    if(dt)
    {
        i64_last_correction = dt;
    }

    int32_t i32ret_millis;
    if(i64_last_correction)
    {
        int64_t i64corr_coeff = (u64_desired_frq_millihz + 500000LL) / 1000000LL;
        i32ret_millis = (i64_last_correction * i64corr_coeff + 50000LL) / 1000000LL;

        return i32ret_millis;
    }

    return 0U;
}*/
