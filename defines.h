///////////////////////////////////////////////////////////////////////////////
//
//  EngineerGuy314 
//  https://github.com/EngineerGuy314/pico-WSPRer
//
///////////////////////////////////////////////////////////////////////////////


//////////////// Hardware related defines ////////////////////////////
//#define PLL_SYS_MHZ 270UL//115UL // This sets CPU speed. Roman originally had 270UL. 
      // After improvement od RfGenStruct we are now on 115 MHz (for 20m band) :-)     

// Serial data from GPS module wired to uart1, GPIO9. 

		/* pin definitions  */
#define GPS_ENABLE_PIN 11       /* 11 on JAWBBONE  GPS_ENABLE pin, inverse logic */  
#define VFO_ENABLE_PIN 18       /* 18 on JAWBONE VFO synthesizer ENABLE pin, inverse logic */  
#define ONEWIRE_bus_pin_pcb 27 

#define LED_PIN  25 /* 25 for pi pico, 13 for Waveshare rp2040-zero  */
#define FLASH_TARGET_OFFSET (256 * 1024) //leaves 256k of space for the program
#define CONFIG_LOCATOR4 "AA22AB"       	       //gets overwritten by gps data anyway       

//////////////// Other defines ////////////////////////////
#define FALSE 0                                     /* Something is false. */
#define TRUE 1                                       /* Something is true. */
#define BAD 0                                         /* Something is bad. */
#define GOOD 1                                       /* Something is good. */
#define INVALID 0                                 /* Something is invalid. */
#define VALID 1                                     /* Something is valid. */
#define NO 0                                          /* The answer is no. */
#define YES 1                                        /* The answer is yes. */
#define OFF 0                                       /* Turn something off. */
#define ON 1                                         /* Turn something on. */
#define ZERO 0                                 /* Something in zero state. */

#define RAM __not_in_flash_func         /* Place time-critical func in RAM */
#define RAM_A __not_in_flash("A")        /* Place time-critical var in RAM */

// ANSI escape codes for color
#define RED "\x1b[91m"
#define BRIGHT "\x1b[97m"
#define NORMAL "\x1b[37m"
#define GREEN "\x1b[32m"
#define YELLOW "\x1b[33m"
#define BLUE "\x1b[34m"
#define RESET "\x1b[0m"
#define CLEAR_SCREEN "\x1b[2J"
#define CURSOR_HOME "\x1b[H"
#define UNDERLINE_ON "\033[4m"
#define UNDERLINE_OFF "\033[24m"
#define BOLD_ON "\033[1m"   
#define BOLD_OFF "\033[0m"

/* A macros for arithmetic right shifts, with casting of the argument. */
#define iSAR32(arg, rcount) (((int32_t)(arg)) >> (rcount))
#define iSAR64(arg, rcount) (((int64_t)(arg)) >> (rcount))

#define min(a,b) ((a)<(b)?(a):(b))
#define max(a,b) ((a)>(b)?(a):(b))

  /* A macros for fast fixed point arithmetics calculations */
#define iMUL32ASM(a,b) __mul_instruction((int32_t)(a), (int32_t)(b))
#define iSquare32ASM(x) (iMUL32ASM((x), (x)))
#define ABS(x) ((x) > 0 ? (x) : -(x))
#define INVERSE(x) ((x) = -(x))
#define asizeof(a) (sizeof (a) / sizeof ((a)[0]))

#define SECOND 1                                                  /* Time. */
#define MINUTE (60 * SECOND)
#define HOUR (60 * MINUTE)

#define kHz 1000UL                                                /* Freq. */
#define MHz 1000000UL

/* WSPR constants definitions */
#define WSPR_FREQ_STEP_MILHZ    2930UL     /* FSK freq.bin (*2 this time). */
#define WSPR_MAX_GPS_DISCONNECT_TM  \
        (6 * HOUR)                      /* How long is active without GPS. */
			
#define WSPR_SYMBOL_COUNT   162
#define WSPR_BIT_COUNT      162
#define VALID_DBM_SIZE      28
#define tempU (tempC*(9.0f/5.0f))+32

#define SI5351_ADDR 0x60  //for the MS5351 
#define CLK_CNTRL_reg_16 16 // Register definitions
#define MultiSynth_frac_denom_reg_26 26
#define MS_Div_0_42 42
#define MS_Div_1_50 50
#define MS_Div_2_58 58
#define PLL_RESET_177 177
#define SI_R_DIV_1 0b00000000 // R-division ratio definitions
#define SI_R_DIV_128 0b01110000
#define SI_CLK_SRC_PLL_A 0b00000000
#define Si5351I2CAddress 0x60
#define synth_xtal_freq 26000000


///////////// INCLUDES //////////////////////
#pragma once

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"   
#include <stdint.h>
#include <string.h>
#include <ctype.h>	
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "pico/stdlib.h"
#include "hardware/uart.h"
#include <stdint.h>
#include <stdlib.h>
#include "hardware/clocks.h"
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/structs/pll.h"
#include "hardware/pll.h"
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include "hardware/i2c.h"

		
////////////    Data type definition /////////////////////////////



//gps time

typedef struct
{
    uint8_t _u8_is_solution_active;             /* A navigation solution is valid. */

	uint8_t sat_count;
    char _u8_last_digit_minutes;                /* First digit of the minutes. Really, this is the only thing needed to sequence messages. */
    uint8_t _seconds;               
    char _u8_last_digit_hour;  
	char _full_time_string[7];
	uint32_t hour;
	uint32_t knots;
	uint32_t minute;
    int64_t _i64_lat_100k, _i64_lon_100k;       /* The lat, lon, degrees, multiplied by 1e5. */
	
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
    int8_t Optional_Debug;
    uint32_t message_count;   //#valid GPS serial messages          		/* number of text bursts from GPS mod. */


} GPStimeContext;

//TX Chanel
typedef struct
{

    int32_t _frq_cycles_per_pi; /* CPU CLK cycles per PI. */
    uint32_t _ui32_pioreg[8];   /* Shift register to PIO. */
    uint32_t _clkfreq_hz;       /* CPU CLK freq, Hz. */
    GPStimeContext *_pGPStime;  /* Ptr to GPS time context. */
    uint32_t _ui32_frq_hz;      /* Working freq, Hz. */
    int32_t _ui32_frq_millihz;  /* Working freq additive shift, mHz. */
    int _is_enabled;
} RfGenStruct;

typedef struct
{
    uint64_t _tm_future_call;
    uint32_t _bit_period_us;
    uint8_t _timer_alarm_num;
    uint8_t _ix_input, _ix_output;
    uint8_t _pbyte_buffer[256];
    RfGenStruct *_p_oscillator;
    uint32_t _u32_dialfreqhz;
    int _i_tx_gpio;

} TxChannelContext;

//WSPR BEacon

typedef struct
{
 
	uint8_t force_xmit_for_testing;
    uint8_t led_mode;
	uint8_t suffix;
    char id13[3];
    int8_t temp_in_Celsius;
	int8_t verbosity;
    int8_t Optional_Debug;	
    double voltage;
    double voltage_at_idle;
    double voltage_at_xmit;
	uint32_t TELEN1_val1;
	uint32_t TELEN1_val2;
	uint32_t TELEN2_val1;
	uint32_t TELEN2_val2;
	uint32_t minutes_since_boot;
	uint32_t seconds_for_lock;
	uint32_t max_sats_seen_today;
	uint8_t low_power_mode;

} WSPRbeaconSchedule;


typedef struct
{
	uint32_t value;
	uint32_t range;
} v_and_r;

typedef struct
{
    uint8_t _pu8_callsign[12];
    uint8_t _pu8_locator[7];
    uint8_t _u8_txpower;
    uint8_t _pu8_outbuf[256];
    TxChannelContext *_pTX;
    WSPRbeaconSchedule _txSched;
	char telem_callsign[7];
	char telem_4_char_loc[5];
	uint8_t telem_power;	
	char telem_chars[8];
	v_and_r telem_vals_and_ranges[5][10];    //slot and param number
	uint64_t Big64;
	uint8_t grid7;
	uint8_t grid8;
	uint8_t grid9;
	uint8_t grid10;
	
} WSPRbeaconContext;

////////////    function definition /////////////////////////////

//main.c
void read_NVRAM(void);
void write_NVRAM(void);
int check_data_validity(void);
void check_data_validity_and_set_defaults(void);
void user_interface(void);
void show_values(void);
void convertToUpperCase(char *str);
void handle_LED(int led_state);
void InitPicoPins(void);
void display_intro(void);
void I2C_init(void);
void I2C_read(void);
void show_TELEN_msg(void); 
void process_TELEN_data(void); 
void onewire_read(void);
void dallas_setup(void);
void datalog_special_functions(void);
void datalog_loop(void);
void reboot_now(void);
void go_to_sleep(void);
void write_to_next_avail_flash(char *text);
void process_chan_num(void);


//logutils
void StampPrintf(const char* pformat, ...);
void DoLogPrint();


//protos
void InitPicoHW(void);
void Core1Entry(void);

//maidenhead

char* get_mh(double lat, double lon, int size);
char* complete_mh(char* locator);
double mh2lon(char* locator);
double mh2lat(char* locator);

//utilitieas

void get_user_input(const char *prompt, char *input_variable, int max_length);

//nhash

uint32_t nhash_( const void *, int *, uint32_t *);

//WSPR Utility

void wspr_encode(const char * call, const char * loc, const int8_t dbm, uint8_t * symbols,uint8_t verbos);
void wspr_message_prep(char * call, char * loc, int8_t dbm);
void wspr_bit_packing(uint8_t * c);
void convolve(uint8_t * c, uint8_t * s, uint8_t message_size, uint8_t bit_size);
void wspr_interleave(uint8_t * s);
void wspr_merge_sync_vector(uint8_t * g, uint8_t * symbols);
uint8_t wspr_code(char c);
void pad_callsign(char * call);

//si5315 from Hans' sample
uint8_t i2cSendRegister(uint8_t reg, uint8_t data);
void si5351aSetFrequency(uint64_t frequency); //Frequency is in centiHz
void si5351aOutputOff(uint8_t clk);
void setupMultisynth(uint8_t synth, uint32_t Divider, uint8_t rDiv);
void setupPLL(uint8_t pll, uint8_t mult, uint32_t num, uint32_t denom);


//TX CHannel

TxChannelContext *TxChannelInit(const uint32_t bit_period_us, 
                                uint8_t timer_alarm_num,RfGenStruct *RfGen);

uint8_t TxChannelPending(TxChannelContext *pctx);
int TxChannelPush(TxChannelContext *pctx, uint8_t *psrc, int n);
int TxChannelPop(TxChannelContext *pctx, uint8_t *pdst);
void TxChannelClear(TxChannelContext *pctx);
//int32_t RfGenStructGetFreqShiftMilliHertz(const RfGenStruct *pdco, uint64_t u64_desired_frq_millihz);

//wspr beacon

WSPRbeaconContext *WSPRbeaconInit(const char *pcallsign, const char *pgridsquare, int txpow_dbm, uint32_t dial_freq_hz, uint32_t shift_freq_hz,
                                  int gpio,  uint8_t start_minute,  uint8_t id13 ,  uint8_t suffix,const char *DEXT_config,RfGenStruct *RfGen);
void WSPRbeaconSetDialFreq(WSPRbeaconContext *pctx, uint32_t freq_hz);
int WSPRbeaconCreatePacket(WSPRbeaconContext *pctx,int packet_type);
char* add_brackets(const char * call);
int WSPRbeaconSendPacket(const WSPRbeaconContext *pctx);
char EncodeBase36(uint8_t val);
int WSPRbeaconTxScheduler(WSPRbeaconContext *pctx, int verbose);
void WSPRbeaconDumpContext(const WSPRbeaconContext *pctx);
char *WSPRbeaconGetLastQTHLocator(WSPRbeaconContext *pctx);
uint8_t WSPRbeaconIsGPSsolutionActive(const WSPRbeaconContext *pctx);
void encode_telen(uint32_t telen_val1,uint32_t telen_val2,char * telen_chars,uint8_t * telen_power, uint8_t packet_type);  
void encode_telen2(uint32_t telen_val1,uint32_t telen_val2,char * telen_chars,uint8_t * telen_power, uint8_t packet_type);  
void telem_add_values_to_Big64(int slot, WSPRbeaconContext *c); 
void telem_add_header(int slot, WSPRbeaconContext *c);
void telem_convert_Big64_to_GridLocPower(WSPRbeaconContext *c);
int calc_solar_angle(int hour, int min, int64_t int_lat, int64_t int_lon);

//Si5351 stuff from utilities 

void si5351_stop();

// GPS TIME

GPStimeContext *GPStimeInit(int uart_baud);
void GPStimeDestroy(GPStimeContext **pp);
int parse_GPS_data(GPStimeContext *pg);
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