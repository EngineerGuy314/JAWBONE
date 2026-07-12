/////////////////////////////////////////////////////////////////////////////
//  Majority of code forked from work by
//  Roman Piksaykin [piksaykin@gmail.com], R2BDY
//  https://www.qrz.com/db/r2bdy
//  PROJECT PAGE
//  https://github.com/RPiks/pico-WSPR-tx
///////////////////////////////////////////////////////////////////////////////

#include "defines.h"

static GPStimeContext *spGPStimeContext = NULL;
static GPStimeData *spGPStimeData = NULL;
static uint16_t byte_count;

static int sm;
static uint offset;
static int32_t tics_per_second;
static int32_t nanosecs_per_tick;

GPStimeContext *GPStimeInit(int uart_baud)
{
  
    // Set up our UART with the required speed & assign pins.
    uart_init(uart1, uart_baud);
    gpio_set_function(8, GPIO_FUNC_UART);
    gpio_set_function(9, GPIO_FUNC_UART);
    
    GPStimeContext *pgt = calloc(1, sizeof(GPStimeContext));
	pgt->_uart_baudrate = uart_baud;

    spGPStimeContext = pgt;
    spGPStimeData = &pgt->_time_data;
    uart_set_hw_flow(uart1, false, false);
    uart_set_format(uart1, 8, 1, UART_PARITY_NONE);
    uart_set_fifo_enabled(uart1, true);   // 32-byte RX FIFO buffers bytes while parse_GPS_data runs in the ISR
    irq_set_exclusive_handler(UART1_IRQ, GPStimeUartRxIsr);
    irq_set_enabled(UART1_IRQ, true);
    uart_set_irq_enables(uart1, true, false);

	return pgt;
}

GPStimeContext *GPStimeInitAutobaud(void)
{
    static const int bauds[] = {9600, 115200};
    int idx = 0;
    GPStimeContext *pg = GPStimeInit(bauds[0]);

    for (;;) {
        int baud = bauds[idx];
        printf("GPS autobaud: trying %d\n", baud);

        absolute_time_t deadline = make_timeout_time_ms(5000);
        while (!time_reached(deadline)) {
            sleep_ms(500);
            if (pg->_pbytebuff[0] == '$') {
                printf("GPS: locked at %d baud\n", baud);
                return pg;
            }
        }

        // No valid NMEA in 5 s — switch to the other baud rate.
        idx = 1 - idx;
        int next_baud = bauds[idx];
        irq_set_enabled(UART1_IRQ, false);
        uart_set_baudrate(uart1, next_baud);
        while (uart_is_readable(uart1))          // drain stale FIFO bytes
            (void)uart_get_hw(uart1)->dr;
        pg->_uart_baudrate   = next_baud;
        pg->_framing_errors  = 0;
        pg->_u8_ixw          = 0;
        pg->_is_sentence_ready = 0;
        memset(pg->_pbytebuff, 0, sizeof(pg->_pbytebuff));
        irq_set_enabled(UART1_IRQ, true);
    }
}

void GPStimeDestroy(GPStimeContext **pp)
{
    spGPStimeContext = NULL;    /* Detach global context Ptr. */
    spGPStimeData = NULL;
    uart_deinit(uart1);
    free(*pp);
    *pp = NULL;
}

///  UART FIFO ISR. Processes another N chars received from GPS receiver
void RAM (GPStimeUartRxIsr)()
{
    if((spGPStimeContext))
    {
		uart_inst_t *puart_id = uart1;
        while (uart_is_readable(puart_id))
        {
            uint32_t dr = uart_get_hw(puart_id)->dr;
            if (dr & UART_UARTDR_FE_BITS)
                spGPStimeContext->_framing_errors++;
            uint8_t chr = (uint8_t)(dr & UART_UARTDR_DATA_BITS);
            spGPStimeContext->_pbytebuff[spGPStimeContext->_u8_ixw++] = chr;
            if (spGPStimeContext->_u8_ixw >= sizeof(spGPStimeContext->_pbytebuff) - 1)
                spGPStimeContext->_u8_ixw = 0; // overlong sentence — reset to avoid buffer overflow
            if ('\n' == chr)
			{
				spGPStimeContext->_pbytebuff[spGPStimeContext->_u8_ixw]=0;//null terminates
				spGPStimeContext->_is_sentence_ready =1;
				break;
			}
        }
		
	   if(spGPStimeContext->_is_sentence_ready)
        {
			
				
			spGPStimeContext->_u8_ixw = 0;     
														if ((spGPStimeContext->verbosity>=8)&&(spGPStimeContext->user_setup_menu_active==0 ))  printf("dump ALL RAW FIFO: %s",(char *)spGPStimeContext->_pbytebuff);           
														if (spGPStimeContext->Optional_Debug&(1<<0)) {
									strncpy((char*)spGPStimeContext->_debug_print_buff,(char*)spGPStimeContext->_pbytebuff,sizeof(spGPStimeContext->_debug_print_buff)-1);
									spGPStimeContext->_debug_print_buff[sizeof(spGPStimeContext->_debug_print_buff)-1]=0;
									spGPStimeContext->_debug_print_pending=1;
								}
														
            spGPStimeContext->_is_sentence_ready =0;
			spGPStimeContext->_i32_error_count -= parse_GPS_data(spGPStimeContext);
        }
    }
}

/// @brief Extracts field N (1-based, where 1 is the sentence type like "$GPRMC")
/// from a NMEA sentence into a null-terminated string. Returns true if non-empty.
static bool get_nmea_field_str(const char *line, int n, char *out, int out_len)
{
    char buf[256];
    strncpy(buf, line, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    char *rest = buf;
    char *token;
    int fn = 0;
    while ((token = strsep(&rest, ",")) != NULL) {
        if (++fn == n) {
            strncpy(out, token, out_len - 1);
            out[out_len - 1] = '\0';
            // strip checksum suffix (*XX) if present
            char *star = strchr(out, '*');
            if (star) *star = '\0';
            return out[0] != '\0';
        }
    }
    return false;
}

/// @brief Processes a NMEA sentence GxRMC.
///////****************************************************************************
bool get_8th_field_as_float(const char *line, double *out_val) {
    if (!line || !out_val) return false;

    char buf[512];
    strncpy(buf, line, sizeof(buf));
    buf[sizeof(buf)-1] = '\0'; // ensure null termination

    char *token;
    char *rest = buf;
    int field_num = 0;

    while ((token = strsep(&rest, ",")) != NULL) {  // strsep handles empty fields
        field_num++;
        if (field_num == 8) {
            // Skip empty fields
            if (token[0] == '\0') return false;

            char *endptr;
            double val = strtod(token, &endptr);
            if (endptr == token) return false;  // not a number
            *out_val = val;
            return true;
        }
    }

    return false;  // fewer than 8 fields
}
//***************************************************************************
int parse_GPS_data(GPStimeContext *pg)
{                                               //"$GxRMC has time, locations, altitude and sat count! unlike $xxGGA it does NOT have date, but so what
    uint8_t *prmc  = (uint8_t *)strnstr((char *)pg->_pbytebuff, "$GNGGA,", sizeof(pg->_pbytebuff));
    if (!prmc)  prmc  = (uint8_t *)strnstr((char *)pg->_pbytebuff, "$GPGGA,", sizeof(pg->_pbytebuff));
    if (!prmc)  prmc  = (uint8_t *)strnstr((char *)pg->_pbytebuff, "$NGGA,",  sizeof(pg->_pbytebuff));
    uint8_t *gnrmc = (uint8_t *)strnstr((char *)pg->_pbytebuff, "$GNRMC,", sizeof(pg->_pbytebuff));
    if (!gnrmc) gnrmc = (uint8_t *)strnstr((char *)pg->_pbytebuff, "$GPRMC,", sizeof(pg->_pbytebuff));
    if (!gnrmc) gnrmc = (uint8_t *)strnstr((char *)pg->_pbytebuff, "$NRMC,",  sizeof(pg->_pbytebuff));
    
	if(gnrmc)
    {
			double speed;
			get_8th_field_as_float((const char *)gnrmc, &speed);
			pg->_time_data.knots = speed / 2;

			// RMC field 10 (1-based) = date as DDMMYY
			char date_str[8] = {0};
			if (get_nmea_field_str((const char *)gnrmc, 10, date_str, sizeof(date_str))
			    && strlen(date_str) >= 6) {
				pg->_time_data.day   = (date_str[0]-'0')*10 + (date_str[1]-'0');
				pg->_time_data.month = (date_str[2]-'0')*10 + (date_str[3]-'0');
				pg->_time_data.year  = (date_str[4]-'0')*10 + (date_str[5]-'0');
			}
	}
	
	if(prmc)
    {			
																								if ((spGPStimeContext->verbosity>=7)&&(spGPStimeContext->user_setup_menu_active==0 )) 	printf("Found GxGGA len: %d  full buff: %s",sizeof(pg->_pbytebuff),(char *)pg->_pbytebuff);// printf("prmc found: %s\n",(char *)prmc);
		pg->message_count++;      //valid mesg count
        uint64_t tm_fix = GetUptime64();
        uint8_t u8ixcollector[16] = {0};   //collects locations of commas
        uint8_t chksum = 0;
        for(uint8_t u8ix = 0, i = 0; u8ix != strlen(prmc); ++u8ix)
        {
            uint8_t *p = prmc + u8ix;
            chksum ^= *p;
            if(',' == *p)
            {
                *p = 0;
                u8ixcollector[i++] = u8ix + 1;
                if('*' == *p || 12 == i)
                    break;
            }
        }		
		pg->_time_data._u8_last_digit_minutes= *(prmc + u8ixcollector[0] + 3);
		pg->_time_data._seconds= atoi(((const char *)prmc + u8ixcollector[0] + 4));
		char first_digit_minute=*(prmc + u8ixcollector[0] + 2);		
		pg->_time_data._u8_last_digit_hour= *(prmc + u8ixcollector[0] + 1);		
		char first_digit_hour = *(prmc + u8ixcollector[0]);			
		pg->_time_data.minute= 10*(first_digit_minute-'0')+ (pg->_time_data._u8_last_digit_minutes-'0');
		pg->_time_data.hour =10*(first_digit_hour-'0')+ (pg->_time_data._u8_last_digit_hour-'0');		
		strncpy(pg->_time_data._full_time_string, (const char *)prmc + u8ixcollector[0], 6);pg->_time_data._full_time_string[6]=0;
				
        pg->_time_data._u8_is_solution_active = (prmc[u8ixcollector[5]]>48);   //numeric 0 for no fix, 1 2 or 3 for various fix types //printf("char is: %c\n",prmc[u8ixcollector[5]]);
		pg->_time_data.sat_count = atoi((const char *)prmc + u8ixcollector[6]); 
		
															if ((spGPStimeContext->verbosity>=6)&&(spGPStimeContext->user_setup_menu_active==0 )) printf("sat count: %d\n",pg->_time_data.sat_count);

        if(pg->_time_data._u8_is_solution_active)
        {											 

//			printf("_u8_is_solution_active: %d GxGGA len: %d  contents %s  minute last dig : %c seconds: %d ",pg->_time_data._u8_is_solution_active,sizeof(pg->_pbytebuff),(char *)pg->_pbytebuff,pg->_time_data._u8_last_digit_minutes,pg->_time_data._seconds);
			char firstTwo[3]; // Array to hold the first two characters
			strncpy(firstTwo, (const char *)prmc + u8ixcollector[1], 2);
			firstTwo[2] = '\0'; // Null terminate the string
			int dd_lat= atoi(firstTwo);
			pg->_time_data._i64_lat_100k = (int64_t)(.5f + 1e5 * ( (100*dd_lat) + atof((const char *)prmc + u8ixcollector[1]+2)/0.6)          ); 
            if('N' == prmc[u8ixcollector[2]]) { }
            else if('S' == prmc[u8ixcollector[2]])  //Thanks Ross!
            {
                INVERSE(pg->_time_data._i64_lat_100k);
            }
            else
                return -2;
			
			char firstThree[4]; // Array to hold the first two characters
			strncpy(firstThree, (const char *)prmc + u8ixcollector[3], 3);
			firstThree[3] = '\0'; // Null terminate the string
			int dd_lon= atoi(firstThree);									   
            pg->_time_data._i64_lon_100k = (int64_t)(.5f + 1e5 * ( (100*dd_lon) + atof((const char *)prmc + u8ixcollector[3]+3)/0.6)  );
            if('E' == prmc[u8ixcollector[4]]) { }
            else if('W' == prmc[u8ixcollector[4]])
            {
                INVERSE(pg->_time_data._i64_lon_100k);
            }
            else
            {
                return -3;
            }	
			float f;
			f = (float)atof((char *)prmc+u8ixcollector[8]);  
			pg->_altitude=f;    	
			//pg->_altitude=12500;     //FORCING A SPECIFIC ALTITUDE for debugging			
		  //printf("GPS Latitude:%lld Longtitude:%lld\n", pg->_time_data._i64_lat_100k, pg->_time_data._i64_lon_100k);		
		
		}
    }

    // GSV sentences: field 4 (1-based) = total sats in view for that constellation.
    // Each constellation sends its own sentence type; we pick up whichever one
    // arrived in this buffer and update the matching counter, then recompute the sum.
    {
        const char *pfx[5] = {"$GPGSV,", "$GLGSV,", "$GAGSV,", "$GBGSV,", "$GQGSV,"};
        uint8_t    *cnt[5] = {
            &pg->_time_data.sats_gps,
            &pg->_time_data.sats_glonass,
            &pg->_time_data.sats_galileo,
            &pg->_time_data.sats_beidou,
            &pg->_time_data.sats_qzss,
        };
        for (int g = 0; g < 5; g++) {
            const char *p = strnstr((const char *)pg->_pbytebuff, pfx[g], sizeof(pg->_pbytebuff));
            if (p) {
                char f4[8] = {0};
                if (get_nmea_field_str(p, 4, f4, sizeof(f4)))
                    *cnt[g] = (uint8_t)atoi(f4);
                pg->_time_data.sats_in_view =
                    pg->_time_data.sats_gps      + pg->_time_data.sats_glonass +
                    pg->_time_data.sats_galileo  + pg->_time_data.sats_beidou  +
                    pg->_time_data.sats_qzss;
                break;
            }
        }
    }

    return 0;
}


/// @brief Dumps the GPS data struct to stdio.
/// @param pd Ptr to Context.
void GPStimeDump(const GPStimeData *pd)
{
    printf("\nGPS solution is active:%u\n", pd->_u8_is_solution_active);
    printf("GPS Latitude:%lld Longtitude:%lld\n", pd->_i64_lat_100k, pd->_i64_lon_100k);

}
