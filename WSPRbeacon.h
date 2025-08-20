
#ifndef WSPRBEACON_H_
#define WSPRBEACON_H_

#include <stdint.h>
#include <string.h>
#include <TxChannel.h>


typedef struct
{
 
	uint8_t force_xmit_for_testing;
    uint8_t led_mode;
	uint8_t suffix;
    char id13[3];
    int8_t temp_in_Celsius;
	int8_t verbosity;
    double voltage;
    double voltage_at_idle;
    double voltage_at_xmit;
    int8_t oscillatorOff;
	uint32_t TELEN1_val1;
	uint32_t TELEN1_val2;
	uint32_t TELEN2_val1;
	uint32_t TELEN2_val2;
	uint32_t minutes_since_boot;
	uint32_t minutes_since_GPS_aquisition;
	uint32_t seconds_since_GPS_aquisition;
	uint32_t seconds_since_GPS_loss;
	uint32_t seconds_it_took_FIRST_GPS_lock;
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


WSPRbeaconContext *WSPRbeaconInit(const char *pcallsign, const char *pgridsquare, int txpow_dbm,
                                  PioDco *pdco, uint32_t dial_freq_hz, uint32_t shift_freq_hz,
                                  int gpio,  uint8_t start_minute,  uint8_t id13 ,  uint8_t suffix,const char *DEXT_config);
void WSPRbeaconSetDialFreq(WSPRbeaconContext *pctx, uint32_t freq_hz);
int WSPRbeaconCreatePacket(WSPRbeaconContext *pctx,int packet_type);
char* add_brackets(const char * call);
int WSPRbeaconSendPacket(const WSPRbeaconContext *pctx);
char EncodeBase36(uint8_t val);
int WSPRbeaconTxScheduler(WSPRbeaconContext *pctx, int verbose, int GPS_PPS_PIN);
void WSPRbeaconDumpContext(const WSPRbeaconContext *pctx);
char *WSPRbeaconGetLastQTHLocator(WSPRbeaconContext *pctx);
uint8_t WSPRbeaconIsGPSsolutionActive(const WSPRbeaconContext *pctx);
void encode_telen(uint32_t telen_val1,uint32_t telen_val2,char * telen_chars,uint8_t * telen_power, uint8_t packet_type);  
void encode_telen2(uint32_t telen_val1,uint32_t telen_val2,char * telen_chars,uint8_t * telen_power, uint8_t packet_type);  
void telem_add_values_to_Big64(int slot, WSPRbeaconContext *c); 
void telem_add_header(int slot, WSPRbeaconContext *c);
void telem_convert_Big64_to_GridLocPower(WSPRbeaconContext *c);
int calc_solar_angle(int hour, int min, int64_t int_lat, int64_t int_lon);
#endif
