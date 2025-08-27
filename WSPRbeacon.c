/////////////////////////////////////////////////////////////////////////////
//  Much of the code forked from work by
//  Roman Piksaykin [piksaykin@gmail.com], R2BDY
//  https://www.qrz.com/db/r2bdy
//  PROJECT PAGE
//  https://github.com/RPiks/pico-WSPR-tx
///////////////////////////////////////////////////////////////////////////////
#include "defines.h"
#include <math.h>
#include "pico/sleep.h"      
#include "hardware/rtc.h" 
#include "hardware/watchdog.h"
#include "pico/multicore.h"


static char grid5;
static char grid6;
static char grid7;
static char grid8;
static char grid9;
static char grid10;
static float altitude_snapshot;
static int rf_pin;
static int U4B_second_packet_has_started = 0;
static int U4B_second_packet_has_started_at_minute;
static int itx_trigger = 0;
static int itx_trigger2 = 0;		
static int forced_xmit_in_process = 0;
static int transmitter_status = 0;	
static absolute_time_t start_time;
static absolute_time_t start_time_of_GPS_search;
static absolute_time_t time_of_last_serial_packet;
static int current_minute;
static int current_second;
static int oneshots[10];
static int schedule[10];  //array index is minute, (odd minutes are unused) value is -1 for NONE or 1-4 for U4B 1st msg,U4B 2nd msg,Zachtek 1st, Zachtek 2nd, and #5 for extended TELEN
static int schedule_band[10];  //holds the band number (10, 20, 17, etc...) that will be used for that timeslot
static int at_least_one_slot_has_elapsed;
static int at_least_one_first_packet_sent=0;
static int at_least_one_GPS_fixed_has_been_obtained;
static uint8_t _callsign_for_TYPE1[12];
static 	uint8_t  altitude_as_power_fine;
static uint32_t previous_msg_count;
static absolute_time_t GPS_aquisiion_time;
static absolute_time_t GPS_loss_time;
static uint32_t minute_OF_GPS_aquisition;
static int SEQ;
static int tikk;
static char _4_char_version_of_locator[5];
static int tester;
static uint32_t OLD_GPS_active_status;
const int8_t valid_dbm[19] =
    {0, 3, 7, 10, 13, 17, 20, 23, 27, 30, 33, 37, 40,
     43, 47, 50, 53, 57, 60};  
extern char _band_hop[2];         //"extern" is a sneaky and lazy way to get access to global variables in main.c.  shhhh.. don't tell the "professional"  programmers i used it...
extern uint32_t XMIT_FREQUENCY;
extern uint32_t XMIT_FREQUENCY_10_METER;
extern int RFOUT_PIN;

static void sleep_callback(void) {
    printf("RTC woke us up\n");
}
//****************************************************************************************************************************************************************
void telem_add_values_to_Big64(int slot, WSPRbeaconContext *c) //for DEXT, cycles through value/range array and for non-zero ranges packs  'em into Big64
{							
uint64_t val=0;

int max_len = sizeof(c->telem_vals_and_ranges[0]) / sizeof(c->telem_vals_and_ranges[0][0]);  
for (int i = max_len-1; i >= 0; i--) 
	if (c->telem_vals_and_ranges[slot][i].range>0)
	{
		c->telem_vals_and_ranges[slot][i].value = c->telem_vals_and_ranges[slot][i].value % c->telem_vals_and_ranges[slot][i].range; //anti-stupid to fix potential overflow of bad values
		val *= c->telem_vals_and_ranges[slot][i].range;   // shift Big64 by the max range of the value
		val += c->telem_vals_and_ranges[slot][i].value;	// add the value to Big64
	}

c->Big64=val;
}

/////****************************************************************************************
void telem_add_header( int slot, WSPRbeaconContext *c)
{
uint64_t val=c->Big64;

val *=5; 	val+=slot;	 //slot                         5 values: 0-4
val *=16; 	val+=0;      //type 0=custom                16 values
val *=4; 	val+=0;      //reserved bits				4 values (2 bits)
val *=2; 	val+=0;      //specify extended telemetry   2 values (1 bit)   

c->Big64=val;
}

/////****************************************************************************************
void telem_convert_Big64_to_GridLocPower(WSPRbeaconContext *c)
{  		//does the unpacking of Big64 into grid callsign and power based on the radix's of char destinations

		uint64_t val  = c->Big64;
        c->telem_power         =valid_dbm[val % 19];val = val / 19;     //valid_dbm() converts from discrete to feasible power levels
        c->telem_4_char_loc[3] = '0' +  val % 10; 	val = val / 10;
        c->telem_4_char_loc[2] = '0' +  val % 10; 	val = val / 10;
        c->telem_4_char_loc[1] = 'A' +  val % 18; 	val = val / 18;
		c->telem_4_char_loc[0] = 'A' +  val % 18; 	val = val / 18;
		c->telem_4_char_loc[4] = 0;
		c->telem_callsign[6]   = 0;
        c->telem_callsign[5]   = 'A' + val % 26; val = val / 26;
        c->telem_callsign[4]   = 'A' + val % 26; val = val / 26;
        c->telem_callsign[3]   = 'A' + val % 26; val = val / 26;
		c->telem_callsign[2]   = c->_txSched.id13[1];     //fixed, based on reserved callsign
        c->telem_callsign[1]   = EncodeBase36(val % 36); val = val / 36;
        c->telem_callsign[0]   = c->_txSched.id13[0];     //fixed, based on reserved callsign        
}

//******************************************************************************************************************************
/// @brief Initializes a new WSPR beacon context.

WSPRbeaconContext *WSPRbeaconInit(const char *pcallsign, const char *pgridsquare, int txpow_dbm, uint32_t dial_freq_hz, uint32_t shift_freq_hz,
                                  int gpio,  uint8_t start_minute, uint8_t id13, uint8_t suffix, const char *DEXT_config,RfGenStruct *RfGen)
{
    WSPRbeaconContext *p = calloc(1, sizeof(WSPRbeaconContext));
    strncpy(p->_pu8_callsign, pcallsign, sizeof(p->_pu8_callsign));
    strncpy(p->_pu8_locator, pgridsquare, sizeof(p->_pu8_locator));
    p->_u8_txpower = txpow_dbm;
    p->_pTX = TxChannelInit(682667, 0, RfGen);  			  //bit_period_us Period of data bits, sets up ISR for bit banging WSPR
	at_least_one_slot_has_elapsed=0;OLD_GPS_active_status=0;
	transmitter_status=0;   //hmm, this gets set later, but never returns to zero?

	gpio_put(VFO_ENABLE_PIN,1); sleep_ms(1);gpio_put(GPS_ENABLE_PIN,0);      // power on GPS, power off VFO

	for (int i=0;i < 10;i++) schedule[i]=-1;
	p->_txSched.minutes_since_boot=0;
	p->_txSched.max_sats_seen_today=0;

	/* Following code sets packet types for each timeslot. 1:U4B 1st msg, 2: U4B 2nd msg, 3: WSPR1 or Zachtek 1st, 4:Zachtek 2nd,  5:extended TELEN #1 6:extended TELEN #2  */	
	if (id13==253)   //if U4B protocol disabled ('--' enterred for Id13),  we will ONLY do Type 1 [and Type3 (zachtek)] at the specified minute
	{		
		if (suffix != 253)
		{
			schedule[start_minute]=3;         //we get here if suffix is not '-', meaning that Zachtek (wspr type 3) message is desired  
			schedule[(start_minute+2)%10]=4;  
		}
		else
		{
			schedule[start_minute]=3;         //we get here only is both U4B and ZAchtek(suffix) are disabled. this is for standalone (WSPR Type-1) only beacon mode
		}
	}
else                                       //if we get here, U4B is enabled
	{
		schedule[start_minute]=1;          //do 1st U4b packet at selected minute 
		schedule[(start_minute+2)%10]=2;   //do second U4B packet 2 minutes later
		if (DEXT_config[0]!='-') schedule[(start_minute+4)%10]=5;   //enable DEXT slot 2
		if (DEXT_config[1]!='-') schedule[(start_minute+6)%10]=6;   //enable DEXT slot 3    
		if (DEXT_config[2]!='-') schedule[(start_minute+8)%10]=7;   //enable DEXT slot 4 

		if (suffix != 253)    // if Suffix enabled, Do zachtek messages 4 mins BEFORE (ie 6 minutes in future) of u4b (because minus (-) after char to decimal conversion is 253)
			{
				schedule[(start_minute+6)%10]=3;     //if we get here, both U4B and Zachtek (suffix) enabled. hopefully telen not also enabled!
				schedule[(start_minute+8)%10]=4;
			}
	}
		SEQ=10; //initialize WSPR schedule state machine
		p->_txSched.led_mode = 0;  //waiting for GPS
 return p;
}
//*****************************************************************************************************************************

/// @brief Arranges WSPR sending in accordance with pre-defined schedule.

int WSPRbeaconTxScheduler(WSPRbeaconContext *pctx, int verbose)   // called every half second from Main.c
{
              	
    const uint32_t is_GPS_active = pctx->_pTX->_p_oscillator->_pGPStime->_time_data._u8_is_solution_active;  //on if valid 3d fix
	pctx->_txSched.minutes_since_boot=floor((to_ms_since_boot(get_absolute_time()) / (uint32_t)60000) );     //used for DEXT
	
	if (SEQ==10)
	{
		gpio_put(VFO_ENABLE_PIN,1);sleep_ms(2);gpio_put(GPS_ENABLE_PIN,0); //VFO off, GPS ON										
		pctx->_pTX->_p_oscillator->_pGPStime->message_count=0;
		start_time_of_GPS_search=get_absolute_time();
		SEQ=20;
																		if (pctx->_pTX->_p_oscillator->_pGPStime->Optional_Debug&(1<<2))	printf("enabling GPS\n");
	}

	if (SEQ==20)  //check for GPS comms
	{
		pctx->_txSched.led_mode = 0;  //no GPS serial Comms
			if(pctx->_pTX->_p_oscillator->_pGPStime->message_count>1)
			{
				SEQ=30;
																		if (pctx->_pTX->_p_oscillator->_pGPStime->Optional_Debug&(1<<2))	 printf("GPS serial comms est\n");
			}
	}

	if (SEQ==30)
	{
		pctx->_txSched.led_mode = 1;  //GPS serial Comms established,  But NO LOCK  yet

			if(is_GPS_active)     //waiting for 3d fix
				{
																		if (pctx->_pTX->_p_oscillator->_pGPStime->Optional_Debug&(1<<2))	printf("Position Lock received! it took %.1f secs\n",absolute_time_diff_us(start_time_of_GPS_search, get_absolute_time())/1000000.0);
					SEQ=40;
					pctx->_txSched.led_mode = 2; //gps is locked
					pctx->_txSched.seconds_for_lock=absolute_time_diff_us(start_time_of_GPS_search, get_absolute_time())/1000000.0;
				}
	}

	if (SEQ==40)      //RECORD position    (will keep looping back here until time to xmit)
	{		
		strncpy(_4_char_version_of_locator, pctx->_pu8_locator, 4);     //only take first 4 chars of locator
		_4_char_version_of_locator[4]=0;  //add null terminator
		grid5 = pctx->_pu8_locator[4];  //record the values of grid chars 5 and 6 now, but they won't be used until packet type 2 is created
		grid6 = pctx->_pu8_locator[5];		
		pctx->grid7=grid7; //also record snapshot of chars 7 through 10 for extended telem
		pctx->grid8=grid8;
		pctx->grid9=grid9;
		pctx->grid10=grid10;		
		altitude_snapshot=pctx->_pTX->_p_oscillator->_pGPStime->_altitude;     //save the value for later when used in 2nd packet
		SEQ=50;
	}

	if (SEQ==50)   //check if its time to start a slot transmission   
	{
		current_minute = pctx->_pTX->_p_oscillator->_pGPStime->_time_data._u8_last_digit_minutes - '0';  //convert from char to int
		current_second = pctx->_pTX->_p_oscillator->_pGPStime->_time_data._seconds;

		if((schedule[current_minute]>0)&&(current_second==0))
			{
				SEQ=60;											//if time to start a packet
																		if (pctx->_pTX->_p_oscillator->_pGPStime->Optional_Debug&(1<<2))	printf("About to start packet. current mind: %d current sec %d slot time at this minute: %d time since initial GPS lock: %.1f secs\n",current_minute,current_second,schedule[current_minute],absolute_time_diff_us(start_time_of_GPS_search, get_absolute_time())/1000000.0);
				
			}
			else
			{
				SEQ=40;        //if not time yet, jump back to 40 and keep track of changing position
			}
		if(is_GPS_active==0) //check if GPS lock was lost while waiting to start Xmit
		{
																		if (pctx->_pTX->_p_oscillator->_pGPStime->Optional_Debug&(1<<2))	printf("* * * ** GPS lock lost while waiting to xmit! returning to state 30!");
			SEQ=30;
		}
	}


	if (SEQ==60) //GPS Off, VFO ON
	{
		pctx->_txSched.voltage_at_idle=pctx->_txSched.voltage; //save idle voltage
		start_time= get_absolute_time();  //record start time since GPS will now be off and no more time information
		gpio_put(GPS_ENABLE_PIN,1); sleep_ms(2);gpio_put(VFO_ENABLE_PIN,0);sleep_ms(2); //VFO ON, GPS off
		pctx->_txSched.led_mode = 3; //TRansmittion in progress
		SEQ=70;
	}

	if (SEQ==70) //create and send packet
	{
																		if (((pctx->_txSched.verbosity>=3)||(pctx->_pTX->_p_oscillator->_pGPStime->Optional_Debug&(1<<2)))) printf("\nStarting TX. current minute: %i Schedule Value (packet type): %i\n",current_minute,schedule[current_minute]);
			pctx->_pTX->_u32_dialfreqhz = XMIT_FREQUENCY;
			transmitter_status=1;
			WSPRbeaconCreatePacket(pctx, schedule[current_minute] ); //the schedule determines packet type (1-4 for U4B 1st msg,U4B 2nd msg,Zachtek 1st, Zachtek 2nd)
			sleep_ms(1000); //technically your supposed to wait 1 second after minute to begin TX
			WSPRbeaconSendPacket(pctx); 
			current_minute=(current_minute+2)%10;   //increment in case we come back around here without re-enabling gps
			SEQ=80;		
																		if (pctx->_pTX->_p_oscillator->_pGPStime->Optional_Debug&(1<<2))	printf("stating pak, current minute (after increment) is %d ",current_minute);
	}

	if (SEQ==80)       //wait for end of this packet being transmitted
	{

		if ((pctx->_pTX->_ix_output==162)&&(absolute_time_diff_us(start_time, get_absolute_time()) > 120000000ULL))  //wait for last bit to be sent, and 120 secs since start
		{
			SEQ=90;
																			if (pctx->_pTX->_p_oscillator->_pGPStime->Optional_Debug&(1<<2)) printf("end of pak detected time since initial GPS lock: %.1f secs\n",absolute_time_diff_us(start_time_of_GPS_search, get_absolute_time())/1000000.0);			
		}
	}


	if (SEQ==90)   //check if this next slot has a packet
	{
		
		if(schedule[current_minute]>0)	//if has a packet, do another packet immediately without re-enabling GPS
			{
				start_time = get_absolute_time();     //reset "start_time" so we know when this packet ends
				SEQ=70;
																				if (pctx->_pTX->_p_oscillator->_pGPStime->Optional_Debug&(1<<2))printf("going to do aother pak immediately\n");
			}	
		else
		{
				pctx->_txSched.voltage_at_xmit=pctx->_txSched.voltage; //done xmitting so save voltage during xmit
				SEQ=10;   //turn GPS back on
																				if (pctx->_pTX->_p_oscillator->_pGPStime->Optional_Debug&(1<<2))printf("no pak next, turning GPS back on. time since initial GPS lock: %.1f secs\n",absolute_time_diff_us(start_time_of_GPS_search, get_absolute_time())/1000000.0);			
		}

	}
		
   return 0;
}
//******************************************************************************************************************************
int WSPRbeaconCreatePacket(WSPRbeaconContext *pctx,int packet_type)  //1-6.  1: U4B 1st msg,U4B 2: 2nd msg, 3: Zachtek 1st, 4: Zachtek 2nd 5:U4B telen 1, 6:U4B telen 2
{

   if (packet_type==1)   //U4B first msg
   {
	pctx->_u8_txpower =10;               //hardcoded at 10dbM when doing u4b MSG 1
				if (pctx->_txSched.verbosity>=3){ printf("creating U4B packet 1\n");printf("location for Xmit: %s%c%c%c%c%c%c lat/lon: %lld %lld\n", _4_char_version_of_locator,grid5,grid6,grid7,grid8,grid9,grid10,pctx->_pTX->_p_oscillator->_pGPStime->_time_data._i64_lat_100k,pctx->_pTX->_p_oscillator->_pGPStime->_time_data._i64_lon_100k);}				
	wspr_encode(pctx->_pu8_callsign, _4_char_version_of_locator, pctx->_u8_txpower, pctx->_pu8_outbuf, pctx->_txSched.verbosity);   // look in utility.c for wspr_encode
   }
 if (packet_type==2)   // special encoding for 2nd packet of U4B protocol aka "standard" telemetry
   {
	if (pctx->_txSched.verbosity>=3) printf("creating U4B packet 2 \n");
	char CallsignU4B[7]; 
	char Grid_U4B[7]; 
	uint8_t  power_U4B;



/* inputs:  pctx->_pu8_locator (6 char grid)
			pctx->_txSched->temp_in_Celsius
			pctx->_txSched->id13
			pctx->_txSched->voltage
*/	
  	  // pick apart inputs
      // char grid5 = pctx->_pu8_locator[4];  values of grid 5 and 6 were already set previously when packet 1 was created
      //char grid6 = pctx->_pu8_locator[5];
      // convert inputs into components of a big number
        uint8_t grid5Val = grid5 - 'A';
        uint8_t grid6Val = grid6 - 'A';
		uint16_t altFracM =  round((double)altitude_snapshot/ 20);     

	 // convert inputs into a big number
        uint32_t val = 0;
        val *=   24; val += grid5Val;
        val *=   24; val += grid6Val;
        val *= 1068; val += altFracM;
		        // extract into altered dynamic base
        uint8_t id6Val = val % 26; val = val / 26;
        uint8_t id5Val = val % 26; val = val / 26;
        uint8_t id4Val = val % 26; val = val / 26;
        uint8_t id2Val = val % 36; val = val / 36;
        // convert to encoded CallsignU4B
        char id2 = EncodeBase36(id2Val);
        char id4 = 'A' + id4Val;
        char id5 = 'A' + id5Val;
        char id6 = 'A' + id6Val;
        CallsignU4B[0] =  pctx->_txSched.id13[0];   //string{ id13[0], id2, id13[1], id4, id5, id6 };
		CallsignU4B[1] =  id2;
		CallsignU4B[2] =  pctx->_txSched.id13[1];
		CallsignU4B[3] =  id4;
		CallsignU4B[4] =  id5;
		CallsignU4B[5] =  id6;
		CallsignU4B[6] =  0;

/* inputs:  pctx->_pu8_locator (6 char grid)
			pctx->_txSched->temp_in_Celsius
			pctx->_txSched->id13
			pctx->_txSched->voltage
*/	
/* outputs :	char CallsignU4B[6];     (callsign contains fixed id13, and encodes gridchars56 and altitude 
 				char Grid_U4B[7];        (grid and power contain voltage, knots  and temperature (and some bits)
				uint8_t  power_U4B;
				*/
        // parse input presentations
        double tempC   = pctx->_txSched.temp_in_Celsius;
        double voltage = pctx->_txSched.voltage;
       // map input presentations onto input radix (numbers within their stated range of possibilities)
        uint8_t tempCNum      = (uint8_t)(tempC - -50) % 90;
        uint8_t voltageNum    = ((int16_t)round(((voltage * 100) - 300) / 5) + 20) % 40;
 
		uint8_t speedKnotsNum = pctx->_pTX->_p_oscillator->_pGPStime->_time_data.sat_count;   //encoding # of sattelites into knots
        uint8_t gpsValidNum   = pctx->_pTX->_p_oscillator->_pGPStime->_time_data._u8_is_solution_active;
        gpsValidNum=1; //changed sept 27 2024. because the traquito site won't show the 6 char grid if this bit is even momentarily off. Anyway, redundant cause sat count is sent as knots
		// shift inputs into a big number
        val = 0;
        val *= 90; val += tempCNum;
        val *= 40; val += voltageNum;
        val *= 42; val += speedKnotsNum;
        val *=  2; val += gpsValidNum;
        val *=  2; val += 1;          // standard telemetry (1 for the 2nd U4B packet, 0 for "Extended TELEN") - Thanks Kevin!
        // unshift big number into output radix values
        uint8_t powerVal = val % 19; val = val / 19;
        uint8_t g4Val    = val % 10; val = val / 10;
        uint8_t g3Val    = val % 10; val = val / 10;
        uint8_t g2Val    = val % 18; val = val / 18;
        uint8_t g1Val    = val % 18; val = val / 18;
        // map output radix to presentation
        char g1 = 'A' + g1Val;
        char g2 = 'A' + g2Val;
        char g3 = '0' + g3Val;
        char g4 = '0' + g4Val;
 	
		Grid_U4B[0] = g1; // = string{ g1, g2, g3, g4 };
		Grid_U4B[1] = g2;
		Grid_U4B[2] = g3;
		Grid_U4B[3] = g4;
		Grid_U4B[4] = 0;

	power_U4B=valid_dbm[powerVal];

	wspr_encode(CallsignU4B, Grid_U4B, power_U4B, pctx->_pu8_outbuf,pctx->_txSched.verbosity); 
   }
	
if (packet_type==3)   // WSPR type 1 message (for standalone beacon mode, or 1st part of Zachtek protocol)
   {
	uint8_t suffix_as_string[2];
	uint8_t  power_value=10;  //if just using standalone beacon,  power is reported as 10. If doing Zachtek, this gets overwritten below with rough altitude value

	if (pctx->_txSched.verbosity>=3) printf("creating WSPR type 1 [Zachtek packet 1]\n");
	
		if (pctx->_txSched.suffix==253)  //if standalone beacon mode (suffix was enterred as '-' (253)
		{
				strcpy(_callsign_for_TYPE1,pctx->_pu8_callsign);  
				strcat(_callsign_for_TYPE1,0); //add null terminate
	    }
		else //if doing Zachtek (as opposed to just standalone beacon) do all the stuff below to append suffix and encode power. my software does not allow Zachtek without suffix for now.
		{
			 power_value=10; //unless overwritten below when using a suffix and zachtek, hardcodes power at 10 for Bruce		 
			 strcpy(_callsign_for_TYPE1,pctx->_pu8_callsign);   //this gets called with or without suffix
				if (pctx->_txSched.suffix!=40)    //dont append suffix if its an 'X' (thats option to disable suffix). X=88. 88-48('0') = 40
				{
					strcat(_callsign_for_TYPE1,"/"); 
					suffix_as_string[0]=pctx->_txSched.suffix+48;
					suffix_as_string[1]=0;
					strcat(_callsign_for_TYPE1,suffix_as_string);  
				
			power_value=0;
			if (pctx->_pTX->_p_oscillator->_pGPStime->_altitude>900) power_value=3;
			if (pctx->_pTX->_p_oscillator->_pGPStime->_altitude>2100) power_value=7;
			if (pctx->_pTX->_p_oscillator->_pGPStime->_altitude>3000) power_value=10;
			if (pctx->_pTX->_p_oscillator->_pGPStime->_altitude>3900) power_value=13;
			if (pctx->_pTX->_p_oscillator->_pGPStime->_altitude>5100) power_value=17;
			if (pctx->_pTX->_p_oscillator->_pGPStime->_altitude>6000) power_value=20;
			if (pctx->_pTX->_p_oscillator->_pGPStime->_altitude>6900) power_value=23;
			if (pctx->_pTX->_p_oscillator->_pGPStime->_altitude>8100) power_value=27;
			if (pctx->_pTX->_p_oscillator->_pGPStime->_altitude>9000) power_value=30;
			if (pctx->_pTX->_p_oscillator->_pGPStime->_altitude>9900) power_value=33;
			if (pctx->_pTX->_p_oscillator->_pGPStime->_altitude>11100) power_value=37;
			if (pctx->_pTX->_p_oscillator->_pGPStime->_altitude>12000) power_value=40;
			if (pctx->_pTX->_p_oscillator->_pGPStime->_altitude>12900) power_value=43;
			if (pctx->_pTX->_p_oscillator->_pGPStime->_altitude>14100) power_value=47;
			if (pctx->_pTX->_p_oscillator->_pGPStime->_altitude>15000) power_value=50;
			if (pctx->_pTX->_p_oscillator->_pGPStime->_altitude>15900) power_value=53;
			if (pctx->_pTX->_p_oscillator->_pGPStime->_altitude>17100) power_value=57;
			if (pctx->_pTX->_p_oscillator->_pGPStime->_altitude>18000) power_value=60;
			float fine_altitude = pctx->_pTX->_p_oscillator->_pGPStime->_altitude - (power_value*300.0f);
			altitude_as_power_fine=0;
			if (fine_altitude>60) altitude_as_power_fine=3;
			if (fine_altitude>140) altitude_as_power_fine=7;
			if (fine_altitude>200) altitude_as_power_fine=10;
			if (fine_altitude>260) altitude_as_power_fine=13;
			if (fine_altitude>340) altitude_as_power_fine=17;
			if (fine_altitude>400) altitude_as_power_fine=20;
			if (fine_altitude>460) altitude_as_power_fine=23;
			if (fine_altitude>540) altitude_as_power_fine=27;
			if (fine_altitude>600) altitude_as_power_fine=30;
			if (fine_altitude>660) altitude_as_power_fine=33;
			if (fine_altitude>740) altitude_as_power_fine=37;
			if (fine_altitude>800) altitude_as_power_fine=40;
			if (fine_altitude>860) altitude_as_power_fine=43;
			if (fine_altitude>940) altitude_as_power_fine=47;
			if (fine_altitude>1000) altitude_as_power_fine=50;
			if (fine_altitude>1060) altitude_as_power_fine=53;
			if (fine_altitude>1140) altitude_as_power_fine=57;
			if (fine_altitude>1200) altitude_as_power_fine=60;
			if (pctx->_txSched.verbosity>=3) printf("Raw altitude: %0.3f rough: %d fine: %d\n",pctx->_pTX->_p_oscillator->_pGPStime->_altitude,power_value,altitude_as_power_fine);
				}
		}

	wspr_encode(_callsign_for_TYPE1, pctx->_pu8_locator, power_value, pctx->_pu8_outbuf,pctx->_txSched.verbosity);  
   }

if (packet_type==4)   //2nd Zachtek (WSPR type 3 message)
   {
	if (pctx->_txSched.verbosity>=3) printf("creating Zachtek packet 2 (WSPR type 3)\n");
	wspr_encode(add_brackets(_callsign_for_TYPE1), pctx->_pu8_locator, altitude_as_power_fine, pctx->_pu8_outbuf,pctx->_txSched.verbosity);  			
   }

if ((packet_type==5)||(packet_type==6)||(packet_type==7)) 	 //packet type 5,6,7 corresponds to DEXT slot 2,3,4
   {	
	int DEXT_slot=packet_type-3;							 //packet type 5,6,7 corresponds to DEXT slot 2,3,4
	if (pctx->_txSched.verbosity>=3) printf("creating DEXT packet 1\n");
	
	telem_add_values_to_Big64(DEXT_slot,pctx);   //cycles through value array and for non-zero ranges and packs  'em into Big64
	telem_add_header( DEXT_slot, pctx);   //slot #
	telem_convert_Big64_to_GridLocPower(pctx); //does the unpacking of Big64 based on radix's of char destinations
	wspr_encode(pctx->telem_callsign, pctx->telem_4_char_loc, pctx->telem_power, pctx->_pu8_outbuf, pctx->_txSched.verbosity);   // look in WSPRutility.c for wspr_encode
   }
	return 0;
}
////////////////////////////////////////////////////////////////////
//******************************************************************************************************************************
/// @brief Sends a prepared WSPR packet using TxChannel.
/// @param pctx Context.
/// @return 0, if OK.
//******************************************************************************************************************************
int WSPRbeaconSendPacket(const WSPRbeaconContext *pctx)
{
    TxChannelClear(pctx->_pTX); 
    memcpy(pctx->_pTX->_pbyte_buffer, pctx->_pu8_outbuf, WSPR_SYMBOL_COUNT);  //162
    pctx->_pTX->_ix_input = WSPR_SYMBOL_COUNT;  //set count of bytes to send
    return 0;
}

///////////////////////////////////////////////////////////
/// @brief Dumps the beacon context to stdio.
/// @param pctx Ptr to Context.
void WSPRbeaconDumpContext(const WSPRbeaconContext *pctx)  //called ~ every 20 secs from main.c
{
    const uint64_t u64tmnow = GetUptime64();
    StampPrintf("__________________");
    GPStimeContext *pGPS = pctx->_pTX->_p_oscillator->_pGPStime;
	StampPrintf("LED Mode: %d",pctx->_txSched.led_mode);
	//StampPrintf("Grid: %s",(char *)WSPRbeaconGetLastQTHLocator(pctx));
	StampPrintf("lat: %lli",pctx->_pTX->_p_oscillator->_pGPStime->_time_data._i64_lat_100k);
	StampPrintf("lon: %lli",pctx->_pTX->_p_oscillator->_pGPStime->_time_data._i64_lon_100k);
	StampPrintf("altitude: %f",pctx->_pTX->_p_oscillator->_pGPStime->_altitude);	   
	StampPrintf("current minute: %i",current_minute);	   
}
///////////////////////////////////////////////////////////
/// @brief Extracts maidenhead type QTH locator (such as KO85) using GPS coords.
/// @param pctx Ptr to WSPR beacon context.
/// @return ptr to string of QTH locator (static duration object inside get_mh).
/// @remark It uses third-party project https://github.com/sp6q/maidenhead .
char *WSPRbeaconGetLastQTHLocator(WSPRbeaconContext *pctx)                   //called every second or so
{
    char ten_char_grid[10];
    double lat = 1e-7 * (double)pctx->_pTX->_p_oscillator->_pGPStime->_time_data._i64_lat_100k;  //Roman's original code used 1e-5 instead (bug)
    double lon = 1e-7 * (double)pctx->_pTX->_p_oscillator->_pGPStime->_time_data._i64_lon_100k;  //Roman's original code used 1e-5 instead (bug)
/*    lon+=(double)0.3 + (0.03*(double)pctx->_txSched.minutes_since_boot);
	lon+=(double)1.2;
	lon+=(double)0.01*(double)pctx->_txSched.minutes_since_boot;  //DEBUGGING to simulate motion     */

	snprintf(ten_char_grid,11,get_mh(lat, lon, 10));
	grid7=ten_char_grid[6];
	grid8=ten_char_grid[7];
	grid9=ten_char_grid[8];
	grid10=ten_char_grid[9];
//	printf("chars 6 through 10: %d %d %d %d\n",pctx->grid7,pctx->grid8,pctx->grid9,pctx->grid10);
	return get_mh(lat, lon, 6);
}

uint8_t WSPRbeaconIsGPSsolutionActive(const WSPRbeaconContext *pctx)
{

    return YES == pctx->_pTX->_p_oscillator->_pGPStime->_time_data._u8_is_solution_active;
}
///////////////////////////////////////////////////////////
          // used for Zachtek style
char* add_brackets(const char * call)   //adds <> around the callsign. this is what triggers a type 3 message. 
{
	static char temp_holder[20];
	temp_holder[0]=0;
	char first_brack[2]= "<";
	char second_brack[2]= ">";
    strcat(temp_holder, first_brack);  
    strcat(temp_holder, call);         
    strcat(temp_holder, second_brack);
	return temp_holder;
}	
char EncodeBase36(uint8_t val)
    {
        char retVal;

        if (val < 10)
        {
            retVal = '0' + val;
        }
        else
        {
            retVal = 'A' + (val - 10);
        }

        return retVal;
    }


