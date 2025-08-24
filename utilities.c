
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <defines.h>
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"   
#define BUFFER_SIZE 256

static char logBuffer[BUFFER_SIZE] = {0};

static char callsign[12];
static char locator[7];
static int8_t power;
static int _verbos;

void get_user_input(const char *prompt, char *input_variable, int max_length) {
    int index = 0;
    int ch;
    
    printf("%s", prompt);  // Display the prompt to the user
    fflush(stdout);

    while (1) {
        ch = getchar();
        if (ch == '\n' || ch == '\r') {  // Enter key pressed
            break;
        } else if (ch == 127 || ch == 8) {  // Backspace key pressed (127 for most Unix, 8 for Windows)
            if (index > 0) {
                index--;
                printf("\b \b");  // Move back, print space, move back again
            }
        } else if (isprint(ch)) {
            if (index < max_length - 1) {  // Ensure room for null terminator
                input_variable[index++] = ch;
                printf("%c", ch);  // Echo character
            }
        }
        fflush(stdout);
    }

    input_variable[index] = '\0';  // Null-terminate the string
    printf("\n");
}

/*                                              -- hardcoded at 48Mhz for Kazu's PLL
void InitPicoClock(int PLL_SYS_MHZ)
{
    const uint32_t clkhz = PLL_SYS_MHZ * 1000000L;
	 printf("\n ABOUT TO SET SYSTEM KLOCK TO %dMhz\n", PLL_SYS_MHZ); 
	
    set_sys_clock_khz(clkhz / kHz, true);

    clock_configure(clk_peri, 0,
                    CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLKSRC_PLL_SYS,
                    PLL_SYS_MHZ * MHZ,
                    PLL_SYS_MHZ * MHZ);
					
}
*/


/*
 *-------------------------------------------------------------------------------
 *
 * This file is part of the WSPR application, Weak Signal Propagation Reporter
 *
 * File Name:   nhash.c
 * Description: Functions to produce 32-bit hashes for hash table lookup
 *
 * Copyright (C) 2008-2014 Joseph Taylor, K1JT
 * License: GPL-3
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 3 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 51 Franklin
 * Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Files: lookup3.c
 * Copyright: Copyright (C) 2006 Bob Jenkins <bob_jenkins@burtleburtle.net>
 * License: public-domain
 *  You may use this code any way you wish, private, educational, or commercial.
 *  It's free.
 *
 *-------------------------------------------------------------------------------
*/

/*
These are functions for producing 32-bit hashes for hash table lookup.
hashword(), hashlittle(), hashlittle2(), hashbig(), mix(), and final() 
are externally useful functions.  Routines to test the hash are included 
if SELF_TEST is defined.  You can use this free for any purpose.  It's in
the public domain.  It has no warranty.

You probably want to use hashlittle().  hashlittle() and hashbig()
hash byte arrays.  hashlittle() is is faster than hashbig() on
little-endian machines.  Intel and AMD are little-endian machines.
On second thought, you probably want hashlittle2(), which is identical to
hashlittle() except it returns two 32-bit hashes for the price of one.  
You could implement hashbig2() if you wanted but I haven't bothered here.

If you want to find a hash of, say, exactly 7 integers, do
  a = i1;  b = i2;  c = i3;
  mix(a,b,c);
  a += i4; b += i5; c += i6;
  mix(a,b,c);
  a += i7;
  final(a,b,c);
then use c as the hash value.  If you have a variable length array of
4-byte integers to hash, use hashword().  If you have a byte array (like
a character string), use hashlittle().  If you have several byte arrays, or
a mix of things, see the comments above hashlittle().  

Why is this so big?  I read 12 bytes at a time into 3 4-byte integers, 
then mix those integers.  This is fast (you can do a lot more thorough
mixing with 12*3 instructions on 3 integers than you can with 3 instructions
on 1 byte), but shoehorning those bytes into integers efficiently is messy.
*/

#define SELF_TEST 1

#include <stdio.h>      /* defines printf for tests */
#include <time.h>       /* defines time_t for timings in the test */
#ifdef Win32
#include "win_stdint.h"	/* defines uint32_t etc */
#else
#include <stdint.h>	/* defines uint32_t etc */
#endif
//#include <sys/param.h>  /* attempt to define endianness */
//#ifdef linux
//# include <endian.h>    /* attempt to define endianness */
//#endif

#define HASH_LITTLE_ENDIAN 1

#define hashsize(n) ((uint32_t)1<<(n))
#define hashmask(n) (hashsize(n)-1)
#define rot(x,k) (((x)<<(k)) | ((x)>>(32-(k))))

/*
-------------------------------------------------------------------------------
mix -- mix 3 32-bit values reversibly.

This is reversible, so any information in (a,b,c) before mix() is
still in (a,b,c) after mix().

If four pairs of (a,b,c) inputs are run through mix(), or through
mix() in reverse, there are at least 32 bits of the output that
are sometimes the same for one pair and different for another pair.
This was tested for:
* pairs that differed by one bit, by two bits, in any combination
  of top bits of (a,b,c), or in any combination of bottom bits of
  (a,b,c).
* "differ" is defined as +, -, ^, or ~^.  For + and -, I transformed
  the output delta to a Gray code (a^(a>>1)) so a string of 1's (as
  is commonly produced by subtraction) look like a single 1-bit
  difference.
* the base values were pseudorandom, all zero but one bit set, or 
  all zero plus a counter that starts at zero.

Some k values for my "a-=c; a^=rot(c,k); c+=b;" arrangement that
satisfy this are
    4  6  8 16 19  4
    9 15  3 18 27 15
   14  9  3  7 17  3
Well, "9 15 3 18 27 15" didn't quite get 32 bits diffing
for "differ" defined as + with a one-bit base and a two-bit delta.  I
used http://burtleburtle.net/bob/hash/avalanche.html to choose 
the operations, constants, and arrangements of the variables.

This does not achieve avalanche.  There are input bits of (a,b,c)
that fail to affect some output bits of (a,b,c), especially of a.  The
most thoroughly mixed value is c, but it doesn't really even achieve
avalanche in c.

This allows some parallelism.  Read-after-writes are good at doubling
the number of bits affected, so the goal of mixing pulls in the opposite
direction as the goal of parallelism.  I did what I could.  Rotates
seem to cost as much as shifts on every machine I could lay my hands
on, and rotates are much kinder to the top and bottom bits, so I used
rotates.
-------------------------------------------------------------------------------
*/
#define mix(a,b,c) \
{ \
  a -= c;  a ^= rot(c, 4);  c += b; \
  b -= a;  b ^= rot(a, 6);  a += c; \
  c -= b;  c ^= rot(b, 8);  b += a; \
  a -= c;  a ^= rot(c,16);  c += b; \
  b -= a;  b ^= rot(a,19);  a += c; \
  c -= b;  c ^= rot(b, 4);  b += a; \
}

/*
-------------------------------------------------------------------------------
final -- final mixing of 3 32-bit values (a,b,c) into c

Pairs of (a,b,c) values differing in only a few bits will usually
produce values of c that look totally different.  This was tested for
* pairs that differed by one bit, by two bits, in any combination
  of top bits of (a,b,c), or in any combination of bottom bits of
  (a,b,c).
* "differ" is defined as +, -, ^, or ~^.  For + and -, I transformed
  the output delta to a Gray code (a^(a>>1)) so a string of 1's (as
  is commonly produced by subtraction) look like a single 1-bit
  difference.
* the base values were pseudorandom, all zero but one bit set, or 
  all zero plus a counter that starts at zero.

These constants passed:
 14 11 25 16 4 14 24
 12 14 25 16 4 14 24
and these came close:
  4  8 15 26 3 22 24
 10  8 15 26 3 22 24
 11  8 15 26 3 22 24
-------------------------------------------------------------------------------
*/
#define final(a,b,c) \
{ \
  c ^= b; c -= rot(b,14); \
  a ^= c; a -= rot(c,11); \
  b ^= a; b -= rot(a,25); \
  c ^= b; c -= rot(b,16); \
  a ^= c; a -= rot(c,4);  \
  b ^= a; b -= rot(a,14); \
  c ^= b; c -= rot(b,24); \
}

/*
-------------------------------------------------------------------------------
hashlittle() -- hash a variable-length key into a 32-bit value
  k       : the key (the unaligned variable-length array of bytes)
  length  : the length of the key, counting by bytes
  initval : can be any 4-byte value
Returns a 32-bit value.  Every bit of the key affects every bit of
the return value.  Two keys differing by one or two bits will have
totally different hash values.

The best hash table sizes are powers of 2.  There is no need to do
mod a prime (mod is sooo slow!).  If you need less than 32 bits,
use a bitmask.  For example, if you need only 10 bits, do
  h = (h & hashmask(10));
In which case, the hash table should have hashsize(10) elements.

If you are hashing n strings (uint8_t **)k, do it like this:
  for (i=0, h=0; i<n; ++i) h = hashlittle( k[i], len[i], h);

By Bob Jenkins, 2006.  bob_jenkins@burtleburtle.net.  You may use this
code any way you wish, private, educational, or commercial.  It's free.

Use for hash table lookup, or anything where one collision in 2^^32 is
acceptable.  Do NOT use for cryptographic purposes.
-------------------------------------------------------------------------------
*/


//uint32_t hashlittle( const void *key, size_t length, uint32_t initval)
#ifdef STDCALL
uint32_t __stdcall NHASH( const void *key, size_t *length0, uint32_t *initval0)
#else
uint32_t nhash_( const void *key, int *length0, uint32_t *initval0)
#endif
{
  uint32_t a,b,c;                                          /* internal state */
  size_t length;
  uint32_t initval;
  union { const void *ptr; size_t i; } u;     /* needed for Mac Powerbook G4 */

  length=*length0;
  initval=*initval0;

  /* Set up the internal state */
  a = b = c = 0xdeadbeef + ((uint32_t)length) + initval;

  u.ptr = key;
  if (HASH_LITTLE_ENDIAN && ((u.i & 0x3) == 0)) {
    const uint32_t *k = (const uint32_t *)key;         /* read 32-bit chunks */
    const uint8_t  *k8;

    k8=0;                                     //Silence compiler warning
    /*------ all but last block: aligned reads and affect 32 bits of (a,b,c) */
    while (length > 12)
    {
      a += k[0];
      b += k[1];
      c += k[2];
      mix(a,b,c);
      length -= 12;
      k += 3;
    }

    /*----------------------------- handle the last (probably partial) block */
    /* 
     * "k[2]&0xffffff" actually reads beyond the end of the string, but
     * then masks off the part it's not allowed to read.  Because the
     * string is aligned, the masked-off tail is in the same word as the
     * rest of the string.  Every machine with memory protection I've seen
     * does it on word boundaries, so is OK with this.  But VALGRIND will
     * still catch it and complain.  The masking trick does make the hash
     * noticably faster for short strings (like English words).
     */
#ifndef VALGRIND

    switch(length)
    {
    case 12: c+=k[2]; b+=k[1]; a+=k[0]; break;
    case 11: c+=k[2]&0xffffff; b+=k[1]; a+=k[0]; break;
    case 10: c+=k[2]&0xffff; b+=k[1]; a+=k[0]; break;
    case 9 : c+=k[2]&0xff; b+=k[1]; a+=k[0]; break;
    case 8 : b+=k[1]; a+=k[0]; break;
    case 7 : b+=k[1]&0xffffff; a+=k[0]; break;
    case 6 : b+=k[1]&0xffff; a+=k[0]; break;
    case 5 : b+=k[1]&0xff; a+=k[0]; break;
    case 4 : a+=k[0]; break;
    case 3 : a+=k[0]&0xffffff; break;
    case 2 : a+=k[0]&0xffff; break;
    case 1 : a+=k[0]&0xff; break;
    case 0 : return c;              /* zero length strings require no mixing */
    }

#else /* make valgrind happy */

    k8 = (const uint8_t *)k;
    switch(length)
    {
    case 12: c+=k[2]; b+=k[1]; a+=k[0]; break;
    case 11: c+=((uint32_t)k8[10])<<16;  /* fall through */
    case 10: c+=((uint32_t)k8[9])<<8;    /* fall through */
    case 9 : c+=k8[8];                   /* fall through */
    case 8 : b+=k[1]; a+=k[0]; break;
    case 7 : b+=((uint32_t)k8[6])<<16;   /* fall through */
    case 6 : b+=((uint32_t)k8[5])<<8;    /* fall through */
    case 5 : b+=k8[4];                   /* fall through */
    case 4 : a+=k[0]; break;
    case 3 : a+=((uint32_t)k8[2])<<16;   /* fall through */
    case 2 : a+=((uint32_t)k8[1])<<8;    /* fall through */
    case 1 : a+=k8[0]; break;
    case 0 : return c;
    }

#endif /* !valgrind */

  } else if (HASH_LITTLE_ENDIAN && ((u.i & 0x1) == 0)) {
    const uint16_t *k = (const uint16_t *)key;         /* read 16-bit chunks */
    const uint8_t  *k8;

    /*--------------- all but last block: aligned reads and different mixing */
    while (length > 12)
    {
      a += k[0] + (((uint32_t)k[1])<<16);
      b += k[2] + (((uint32_t)k[3])<<16);
      c += k[4] + (((uint32_t)k[5])<<16);
      mix(a,b,c);
      length -= 12;
      k += 6;
    }

    /*----------------------------- handle the last (probably partial) block */
    k8 = (const uint8_t *)k;
    switch(length)
    {
    case 12: c+=k[4]+(((uint32_t)k[5])<<16);
             b+=k[2]+(((uint32_t)k[3])<<16);
             a+=k[0]+(((uint32_t)k[1])<<16);
             break;
    case 11: c+=((uint32_t)k8[10])<<16;     /* fall through */
    case 10: c+=k[4];
             b+=k[2]+(((uint32_t)k[3])<<16);
             a+=k[0]+(((uint32_t)k[1])<<16);
             break;
    case 9 : c+=k8[8];                      /* fall through */
    case 8 : b+=k[2]+(((uint32_t)k[3])<<16);
             a+=k[0]+(((uint32_t)k[1])<<16);
             break;
    case 7 : b+=((uint32_t)k8[6])<<16;      /* fall through */
    case 6 : b+=k[2];
             a+=k[0]+(((uint32_t)k[1])<<16);
             break;
    case 5 : b+=k8[4];                      /* fall through */
    case 4 : a+=k[0]+(((uint32_t)k[1])<<16);
             break;
    case 3 : a+=((uint32_t)k8[2])<<16;      /* fall through */
    case 2 : a+=k[0];
             break;
    case 1 : a+=k8[0];
             break;
    case 0 : return c;                     /* zero length requires no mixing */
    }

  } else {                        /* need to read the key one byte at a time */
    const uint8_t *k = (const uint8_t *)key;

    /*--------------- all but the last block: affect some 32 bits of (a,b,c) */
    while (length > 12)
    {
      a += k[0];
      a += ((uint32_t)k[1])<<8;
      a += ((uint32_t)k[2])<<16;
      a += ((uint32_t)k[3])<<24;
      b += k[4];
      b += ((uint32_t)k[5])<<8;
      b += ((uint32_t)k[6])<<16;
      b += ((uint32_t)k[7])<<24;
      c += k[8];
      c += ((uint32_t)k[9])<<8;
      c += ((uint32_t)k[10])<<16;
      c += ((uint32_t)k[11])<<24;
      mix(a,b,c);
      length -= 12;
      k += 12;
    }

    /*-------------------------------- last block: affect all 32 bits of (c) */
    switch(length)                   /* all the case statements fall through */
    {
    case 12: c+=((uint32_t)k[11])<<24; /* fall through */
    case 11: c+=((uint32_t)k[10])<<16; /* fall through */
    case 10: c+=((uint32_t)k[9])<<8;   /* fall through */
    case 9 : c+=k[8];                  /* fall through */
    case 8 : b+=((uint32_t)k[7])<<24;  /* fall through */
    case 7 : b+=((uint32_t)k[6])<<16;  /* fall through */
    case 6 : b+=((uint32_t)k[5])<<8;   /* fall through */
    case 5 : b+=k[4];                  /* fall through */
    case 4 : a+=((uint32_t)k[3])<<24;  /* fall through */
    case 3 : a+=((uint32_t)k[2])<<16;  /* fall through */
    case 2 : a+=((uint32_t)k[1])<<8;   /* fall through */
    case 1 : a+=k[0];
             break;
    case 0 : return c;
    }
  }

  final(a,b,c);
  return c;
}

//uint32_t __stdcall NHASH(const void *key, size_t length, uint32_t initval)

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

void wspr_encode(const char * call, const char * loc, const int8_t dbm, uint8_t * symbols, uint8_t verbos)
{
  char call_[13];
  char loc_[7];
  uint8_t dbm_ = dbm;
  strcpy(call_, call);
  strcpy(loc_, loc);
  _verbos=verbos;
  if (verbos>=4)
  {printf(" THECALLSIGN IS: %s\n",call);
	printf(" THE LoCaToR IS: %s and its length is %i\n",loc,strlen(loc));
  }

  // Ensure that the message text conforms to standards
  // --------------------------------------------------
  wspr_message_prep(call_, loc_, dbm_);

  // Bit packing
  // -----------
  uint8_t c[11];
  wspr_bit_packing(c);

  // Convolutional Encoding
  // ---------------------
  uint8_t s[WSPR_SYMBOL_COUNT];
  convolve(c, s, 11, WSPR_BIT_COUNT);

  // Interleaving
  // ------------
  wspr_interleave(s);

  // Merge with sync vector
  // ----------------------
  wspr_merge_sync_vector(s, symbols);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////
void wspr_message_prep(char * call, char * loc, int8_t dbm)
{
  // Callsign validation and padding
  // -------------------------------
	
	// Ensure that the only allowed characters are digits, uppercase letters, slash, and angle brackets
	uint8_t i;
   if (_verbos>=4) printf(" wsprs MSG prep callsign: %s and its length is %i\n",call,strlen(call));
   if (_verbos>=4) printf(" wsprs MSG prep POWER: %s and its length is %i and dbm %i\n",loc,strlen(loc),dbm);

  for(i = 0; i < 12; i++)  //puts all letters in callsign uppercase
	{
		if(call[i] != '/' && call[i] != '<' && call[i] != '>')
		{
			call[i] = toupper(call[i]);
			if(!((isdigit(call[i]) || isupper(call[i])) || (isupper(call[i])==0)))  //changed Feb 9, 2025 -  previous code was changing the null terminator to a space, wheich then included potential garbage from unitialized space (similar to a non null-terminated string). if this garbage included a / then subsequent code would encode as a type 3 wspr message and screw up telemtry big time intermittently.  this was contributing to non 100% spot reception on previous flights! (i also added parenthesis to clarify the strange C order of operator presendce regarding ! )
			{
				call[i] = ' ';
			}
		}
	}
  call[12] = 0;

  strncpy(callsign, call, 12);

	// Grid locator validation
  if(strlen(loc) == 4 || strlen(loc) == 6)
	{
		for(i = 0; i <= 1; i++)
		{
			loc[i] = toupper(loc[i]);
			if((loc[i] < 'A' || loc[i] > 'R'))
			{
				strncpy(loc, "SS00AA", 7);
			}
		}
		for(i = 2; i <= 3; i++)
		{
			if(!(isdigit(loc[i])))
			{
				strncpy(loc, "BB00AA", 7);
			}
		}
	}
	else
	{
		printf("length: %d, contents: %s\n",strlen(loc),loc);
		strncpy(loc, "BB00BB", 7); 
	}

	if(strlen(loc) == 6)
	{
		for(i = 4; i <= 5; i++)
		{
			loc[i] = toupper(loc[i]);
			if((loc[i] < 'A' || loc[i] > 'X'))
			{
				printf("it didnt like i: %d and char: %c\n",i,loc[i]);
				strncpy(loc, "DD00AA", 7); 				
			}
		}
	}

  strncpy(locator, loc, 7);

	// Power level validation
	// Only certain increments are allowed
	if(dbm > 60)
	{
		dbm = 60;
	}
  //const uint8_t VALID_DBM_SIZE = 28;
  const int8_t valid_dbm[VALID_DBM_SIZE] =
    {-30, -27, -23, -20, -17, -13, -10, -7, -3, 
     0, 3, 7, 10, 13, 17, 20, 23, 27, 30, 33, 37, 40,
     43, 47, 50, 53, 57, 60};
  for(i = 0; i < VALID_DBM_SIZE; i++)
  {
    if(dbm == valid_dbm[i])
    {
      power = dbm;
    }
  }
  // If we got this far, we have an invalid power level, so we'll round down
  for(i = 1; i < VALID_DBM_SIZE; i++)
  {
    if(dbm < valid_dbm[i] && dbm >= valid_dbm[i - 1])
    {
      power = valid_dbm[i - 1];
    }
  }
}

void wspr_bit_packing(uint8_t * c)
{
  uint32_t n, m;

  // Determine if type 1, 2 or 3 message
	char* slash_avail = strchr(callsign, (int)'/');
	if(callsign[0] == '<')
	{
		//printf("BIT Packing Type 3 \n");
		// Type 3 message
		char base_call[13];
		memset(base_call, 0, 13);
		uint32_t init_val = 146;
		char* bracket_avail = strchr(callsign, (int)'>');
		int call_len = bracket_avail - callsign - 1;
		strncpy(base_call, callsign + 1, call_len);
		uint32_t hash = nhash_(base_call, &call_len, &init_val);
		hash &= 32767;

		// Convert 6 char grid square to "callsign" format for transmission
		// by putting the first character at the end
		char temp_loc = locator[0];
		locator[0] = locator[1];
		locator[1] = locator[2];
		locator[2] = locator[3];
		locator[3] = locator[4];
		locator[4] = locator[5];
		locator[5] = temp_loc;

		n = wspr_code(locator[0]);
		n = n * 36 + wspr_code(locator[1]);
		n = n * 10 + wspr_code(locator[2]);
		n = n * 27 + (wspr_code(locator[3]) - 10);
		n = n * 27 + (wspr_code(locator[4]) - 10);
		n = n * 27 + (wspr_code(locator[5]) - 10);

		m = (hash * 128) - (power + 1) + 64;
	}
	else if(slash_avail == (void *)0)
	{
		// Type 1 message
		//printf("BIT Packing type 1 \n");
		pad_callsign(callsign);
		n = wspr_code(callsign[0]);
		n = n * 36 + wspr_code(callsign[1]);
		n = n * 10 + wspr_code(callsign[2]);
		n = n * 27 + (wspr_code(callsign[3]) - 10);
		n = n * 27 + (wspr_code(callsign[4]) - 10);
		n = n * 27 + (wspr_code(callsign[5]) - 10);
				
		m = ((179 - 10 * (locator[0] - 'A') - (locator[2] - '0')) * 180) +   
			(10 * (locator[1] - 'A')) + (locator[3] - '0');
		m = (m * 128) + power + 64;
	}
	else if(slash_avail)   //so slash is supposed to be type 2, but if also had < then its trumped to type 3,
	{
		// Type 2 message
		int slash_pos = slash_avail - callsign;
    uint8_t i;

		// Determine prefix or suffix
		if(callsign[slash_pos + 2] == ' ' || callsign[slash_pos + 2] == 0)
		{
			//printf("BIT Packing single suffix \n");
			// Single character suffix
			char base_call[7];
      memset(base_call, 0, 7);
			strncpy(base_call, callsign, slash_pos);
			for(i = 0; i < 7; i++)
			{
				base_call[i] = toupper(base_call[i]);
				if(!(isdigit(base_call[i]) || isupper(base_call[i])))
				{
					base_call[i] = ' ';
				}
			}
			pad_callsign(base_call);

			n = wspr_code(base_call[0]);
			n = n * 36 + wspr_code(base_call[1]);
			n = n * 10 + wspr_code(base_call[2]);
			n = n * 27 + (wspr_code(base_call[3]) - 10);
			n = n * 27 + (wspr_code(base_call[4]) - 10);
			n = n * 27 + (wspr_code(base_call[5]) - 10);

			char x = callsign[slash_pos + 1];
			if(x >= 48 && x <= 57)
			{
				x -= 48;
			}
			else if(x >= 65 && x <= 90)
			{
				x -= 55;
			}
			else
			{
				x = 38;
			}

			m = 60000 - 32768 + x;

			m = (m * 128) + power + 2 + 64;
		}
		else if(callsign[slash_pos + 3] == ' ' || callsign[slash_pos + 3] == 0)
		{
			// Two-digit numerical suffix
			//printf("bit packingtwo digi t siffice \n");
			char base_call[7];
      memset(base_call, 0, 7);
			strncpy(base_call, callsign, slash_pos);
			for(i = 0; i < 6; i++)
			{
				base_call[i] = toupper(base_call[i]);
				if(!(isdigit(base_call[i]) || isupper(base_call[i])))
				{
					base_call[i] = ' ';
				}
			}
			pad_callsign(base_call);

			n = wspr_code(base_call[0]);
			n = n * 36 + wspr_code(base_call[1]);
			n = n * 10 + wspr_code(base_call[2]);
			n = n * 27 + (wspr_code(base_call[3]) - 10);
			n = n * 27 + (wspr_code(base_call[4]) - 10);
			n = n * 27 + (wspr_code(base_call[5]) - 10);

			// TODO: needs validation of digit
			m = 10 * (callsign[slash_pos + 1] - 48) + callsign[slash_pos + 2] - 48;
			m = 60000 + 26 + m;
			m = (m * 128) + power + 2 + 64;
		}
		else
		{
			// Prefix
			//printf("BIT Packing this is prefix \n");
			char prefix[4];
			char base_call[7];
            memset(prefix, 0, 4);
            memset(base_call, 0, 7);
			strncpy(prefix, callsign, slash_pos);
			strncpy(base_call, callsign + slash_pos + 1, 7);

			if(prefix[2] == ' ' || prefix[2] == 0)
			{
				// Right align prefix
				prefix[3] = 0;
				prefix[2] = prefix[1];
				prefix[1] = prefix[0];
				prefix[0] = ' ';
			}

			for(uint8_t i = 0; i < 6; i++)
			{
				base_call[i] = toupper(base_call[i]);
				if(!(isdigit(base_call[i]) || isupper(base_call[i])))
				{
					base_call[i] = ' ';
				}
			}
			pad_callsign(base_call);

			n = wspr_code(base_call[0]);
			n = n * 36 + wspr_code(base_call[1]);
			n = n * 10 + wspr_code(base_call[2]);
			n = n * 27 + (wspr_code(base_call[3]) - 10);
			n = n * 27 + (wspr_code(base_call[4]) - 10);
			n = n * 27 + (wspr_code(base_call[5]) - 10);

			m = 0;
			for(uint8_t i = 0; i < 3; ++i)
			{
				m = 37 * m + wspr_code(prefix[i]);
			}

			if(m >= 32768)
			{
				m -= 32768;
				m = (m * 128) + power + 2 + 64;
			}
			else
			{
				m = (m * 128) + power + 1 + 64;
			}
		}
	}

  // Callsign is 28 bits, locator/power is 22 bits.
	// A little less work to start with the least-significant bits
	c[3] = (uint8_t)((n & 0x0f) << 4);
	n = n >> 4;
	c[2] = (uint8_t)(n & 0xff);
	n = n >> 8;
	c[1] = (uint8_t)(n & 0xff);
	n = n >> 8;
	c[0] = (uint8_t)(n & 0xff);

	c[6] = (uint8_t)((m & 0x03) << 6);
	m = m >> 2;
	c[5] = (uint8_t)(m & 0xff);
	m = m >> 8;
	c[4] = (uint8_t)(m & 0xff);
	m = m >> 8;
	c[3] |= (uint8_t)(m & 0x0f);
	c[7] = 0;
	c[8] = 0;
	c[9] = 0;
	c[10] = 0;
}

void convolve(uint8_t * c, uint8_t * s, uint8_t message_size, uint8_t bit_size)
{
  uint32_t reg_0 = 0;
  uint32_t reg_1 = 0;
  uint32_t reg_temp = 0;
  uint8_t input_bit, parity_bit;
  uint8_t bit_count = 0;
  uint8_t i, j, k;

  for(i = 0; i < message_size; i++)
  {
    for(j = 0; j < 8; j++)
    {
      // Set input bit according the MSB of current element
      input_bit = (((c[i] << j) & 0x80) == 0x80) ? 1 : 0;

      // Shift both registers and put in the new input bit
      reg_0 = reg_0 << 1;
      reg_1 = reg_1 << 1;
      reg_0 |= (uint32_t)input_bit;
      reg_1 |= (uint32_t)input_bit;

      // AND Register 0 with feedback taps, calculate parity
      reg_temp = reg_0 & 0xf2d05351;
      parity_bit = 0;
      for(k = 0; k < 32; k++)
      {
        parity_bit = parity_bit ^ (reg_temp & 0x01);
        reg_temp = reg_temp >> 1;
      }
      s[bit_count] = parity_bit;
      bit_count++;

      // AND Register 1 with feedback taps, calculate parity
      reg_temp = reg_1 & 0xe4613c47;
      parity_bit = 0;
      for(k = 0; k < 32; k++)
      {
        parity_bit = parity_bit ^ (reg_temp & 0x01);
        reg_temp = reg_temp >> 1;
      }
      s[bit_count] = parity_bit;
      bit_count++;
      if(bit_count >= bit_size)
      {
        break;
      }
    }
  }
}

void wspr_interleave(uint8_t * s)
{
  uint8_t d[WSPR_BIT_COUNT];
	uint8_t rev, index_temp, i, j, k;

	i = 0;

	for(j = 0; j < 255; j++)
	{
		// Bit reverse the index
		index_temp = j;
		rev = 0;

		for(k = 0; k < 8; k++)
		{
			if(index_temp & 0x01)
			{
				rev = rev | (1 << (7 - k));
			}
			index_temp = index_temp >> 1;
		}

		if(rev < WSPR_BIT_COUNT)
		{
			d[rev] = s[i];
			i++;
		}

		if(i >= WSPR_BIT_COUNT)
		{
			break;
		}
	}

  memcpy(s, d, WSPR_BIT_COUNT);
}

void wspr_merge_sync_vector(uint8_t * g, uint8_t * symbols)
{
  uint8_t i;
  const uint8_t sync_vector[WSPR_SYMBOL_COUNT] =
	{1, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 1, 1, 0, 0, 0, 1, 0, 0,
	 1, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 1, 0, 0,
	 0, 0, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 0, 1, 1, 0, 1,
	 0, 0, 0, 0, 1, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 0, 1, 0, 0, 1, 0,
	 1, 1, 0, 0, 0, 1, 1, 0, 1, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1,
	 0, 0, 1, 0, 0, 1, 1, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 0, 1,
	 1, 1, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0,
	 1, 1, 0, 1, 0, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0};

	for(i = 0; i < WSPR_SYMBOL_COUNT; i++)
	{
		symbols[i] = sync_vector[i] + (2 * g[i]);
	}
}

uint8_t wspr_code(char c)
{
  // Validate the input then return the proper integer code.
  // Change character to a space if the char is not allowed.

  if(isdigit(c))
	{
		return (uint8_t)(c - 48);
	}
	else if(c == ' ')
	{
		return 36;
	}
	else if(c >= 'A' && c <= 'Z')
	{
		return (uint8_t)(c - 55);
	}
	else
	{
		return 36;
	}
}

void pad_callsign(char * call)
{
	// If only the 2nd character is a digit, then pad with a space.
	// If this happens, then the callsign will be truncated if it is
	// longer than 6 characters.
	if(isdigit(call[1]) && isupper(call[2]))
	{
		// memmove(call + 1, call, 6);
    call[5] = call[4];
    call[4] = call[3];
    call[3] = call[2];
    call[2] = call[1];
    call[1] = call[0];
		call[0] = ' ';
	}

	// Now the 3rd charcter in the callsign must be a digit
	// if(call[2] < '0' || call[2] > '9')
	// {
	// 	// return 1;
	// }
}
char letterize(int x) {
	if (x<24)
    return (char) x + 65;  
	else
	return (char) 23 + 65; /*KC3LBR 07/23/24   an alternate/redundant fix to the one below, this clamps the returned characters at 'X' or lower. The original code sometimes returned a Y for 5th or 6 char, which is invalid*/
}

char* get_mh(double lat, double lon, int size) {
    static char locator[11];
    double LON_F[]={20,2.0,0.0833333333,0.008333333,0.0003472222222222}; /*KC3LBR 07/23/24   increased resolution of 1/12 constant to prevent problems*/
    double LAT_F[]={10,1.0,0.0416666667,0.004166666,0.0001736111111111}; /*KC3LBR 07/23/24   increased resolution of 1/24 constant to prevent problems*/
    int i;
    lon += 180;
    lat += 90;

    if (size <= 0 || size > 10) size = 6;
    size/=2; size*=2;

    for (i = 0; i < size/2; i++){             //0,1,2
        if (i % 2 == 1) {                      //odd i's         
            locator[i*2] = (char) (lon/LON_F[i] + '0');         //       2,3
            locator[i*2+1] = (char) (lat/LAT_F[i] + '0');
        } else {							   //even i's        
			if (i==0)
			{
				locator[i*2] = letterize((int) (lon/LON_F[i]));     //0,1            4,5
				locator[i*2+1] = letterize((int) (lat/LAT_F[i]));
			}
				else      //The last 2 chars of a 6 char grid must be lowercase??,prolly makes no difference
		    {
				locator[i*2] = letterize((int) (lon/LON_F[i]));     //0,1            4,5
				locator[i*2+1] = letterize((int) (lat/LAT_F[i]));
			}      //technically all chars should be uppercase, these previous 2 lines not needed
			//april 2 2024 removed the +32 from previous 2 lines, YEs, they should ALWAYS be uppercase. otherwise casued problems with U4B code
					
        }
        lon = fmod(lon, LON_F[i]);
        lat = fmod(lat, LAT_F[i]);
    }
    locator[i*2]=0;
    return locator;
}

char* complete_mh(char* locator) {
    static char locator2[11] = "LL55LL55LL";
    int len = strlen(locator);
    if (len >= 10) return locator;
    memcpy(locator2, locator, strlen(locator));
    return locator2;
}

double mh2lon(char* locator) {
    double field, square, subsquare, extsquare, precsquare;
    int len = strlen(locator);
    if (len > 10) return 0;
    if (len < 10) locator = complete_mh(locator);
    field      = (locator[0] - 'A') * 20.0;
    square     = (locator[2] - '0') * 2.0;
    subsquare  = (locator[4] - 'A') / 12.0;
    extsquare  = (locator[6] - '0') / 120.0;
    precsquare = (locator[8] - 'A') / 2880.0;
    return field + square + subsquare + extsquare + precsquare - 180;
}

double mh2lat(char* locator) {
    double field, square, subsquare, extsquare, precsquare;
    int len = strlen(locator);
    if (len > 10) return 0;
    if (len < 10) locator = complete_mh(locator);
    field      = (locator[1] - 'A') * 10.0;
    square     = (locator[3] - '0') * 1.0;
    subsquare  = (locator[5] - 'A') / 24.0;
    extsquare  = (locator[7] - '0') / 240.0;
    precsquare = (locator[9] - 'A') / 5760.0;
    return field + square + subsquare + extsquare + precsquare - 90;
}


//************************si5351 stuff****************
uint8_t i2cSendRegister(uint8_t reg, uint8_t data)
{
	uint8_t buf[2];
	buf[0]=reg;
	buf[1]=data;
	i2c_write_blocking(i2c0, SI5351_ADDR, buf, 2, false);
//	 printf("%d,%X \n",reg,data);
}
void si5351_stop()
{
		i2cSendRegister(3, 0xFF ); //reg 3 disable all outputs
 
}
///////
// I2C and PLL routines from Hans Summer demo code https://www.qrp-labs.com/images/uarduino/uard_demo.ino
//
// Set up specified PLL with mult, num and denom
// mult is 15..90
// num is 0..1,048,575 (0xFFFFF)
// denom is 0..1,048,575 (0xFFFFF)
//
void setupPLL(uint8_t pll, uint8_t mult, uint32_t num, uint32_t denom)
{
  uint32_t P1; // PLL config register P1
  uint32_t P2; // PLL config register P2
  uint32_t P3; // PLL config register P3

  P1 = (uint32_t)(128 * ((float)num / (float)denom));
  P1 = (uint32_t)(128 * (uint32_t)(mult) + P1 - 512);
  P2 = (uint32_t)(128 * ((float)num / (float)denom));
  P2 = (uint32_t)(128 * num - denom * P2);
  P3 = denom;

  i2cSendRegister(pll + 0, (P3 & 0x0000FF00) >> 8);
  i2cSendRegister(pll + 1, (P3 & 0x000000FF));
  i2cSendRegister(pll + 2, (P1 & 0x00030000) >> 16);
  i2cSendRegister(pll + 3, (P1 & 0x0000FF00) >> 8);
  i2cSendRegister(pll + 4, (P1 & 0x000000FF));
  i2cSendRegister(pll + 5, ((P3 & 0x000F0000) >> 12) | ((P2 &
                  0x000F0000) >> 16));
  i2cSendRegister(pll + 6, (P2 & 0x0000FF00) >> 8);
  i2cSendRegister(pll + 7, (P2 & 0x000000FF));
}

// I2C and PLL routines from Han Summer demo code https://www.qrp-labs.com/images/uarduino/uard_demo.ino
//
// Set up MultiSynth with integer Divider and R Divider
// R Divider is the bit value which is OR'ed onto the appropriate
// register, it is a #define in si5351a.h
//
void setupMultisynth(uint8_t synth, uint32_t Divider, uint8_t rDiv)
{
  uint32_t P1; // Synth config register P1
  uint32_t P2; // Synth config register P2
  uint32_t P3; // Synth config register P3

  P1 = 128 * Divider - 512;
  P2 = 0; // P2 = 0, P3 = 1 forces an integer value for the Divider
  P3 = 1;

  i2cSendRegister(synth + 0, (P3 & 0x0000FF00) >> 8);
  i2cSendRegister(synth + 1, (P3 & 0x000000FF));
  i2cSendRegister(synth + 2, ((P1 & 0x00030000) >> 16) | rDiv);
  i2cSendRegister(synth + 3, (P1 & 0x0000FF00) >> 8);
  i2cSendRegister(synth + 4, (P1 & 0x000000FF));
  i2cSendRegister(synth + 5, ((P3 & 0x000F0000) >> 12) | ((P2 &
                  0x000F0000) >> 16));
  i2cSendRegister(synth + 6, (P2 & 0x0000FF00) >> 8);
  i2cSendRegister(synth + 7, (P2 & 0x000000FF));
}


// Switches off Si5351a output
void si5351aOutputOff(uint8_t clk)
{
  i2cSendRegister(clk, 0x80); // Refer to SiLabs AN619 to see
  //bit values - 0x80 turns off the output stage

}

// Set CLK0 output ON and to the specified frequency
// Frequency is in the range 10kHz to 150MHz and given in centiHertz (hundreds of Hertz)
// Example: si5351aSetFrequency(1000000200);
// will set output CLK0 to 10.000,002MHz
// I2C and PLL routines from Hans Summer demo code https://www.qrp-labs.com/images/uarduino/uard_demo.ino

// This example sets up PLL A
// and MultiSynth 0
// and produces the output on CLK0
//
void si5351aSetFrequency(uint64_t frequency) //Frequency is in centiHz
{
  static uint64_t oldFreq;
  int32_t FreqChange;
  uint64_t pllFreq;
  //uint32_t xtalFreq = XTAL_FREQ;
  uint32_t l;
  float f;
  uint8_t mult;
  uint32_t num;
  uint32_t denom;
  uint32_t Divider;
  uint8_t rDiv;


  if (frequency > 100000000ULL) { //If higher than 1MHz then set R output divider to 1
    rDiv = SI_R_DIV_1;
    Divider = 90000000000ULL / frequency;// Calculate the division ratio. 900MHz is the maximum VCO freq (expressed as deciHz)
    pllFreq = Divider * frequency; // Calculate the pllFrequency:
    mult = pllFreq / (synth_xtal_freq * 100UL); // Determine the multiplier to
    l = pllFreq % (synth_xtal_freq * 100UL); // It has three parts:
    f = l; // mult is an integer that must be in the range 15..90
    f *= 1048575; // num and denom are the fractional parts, the numerator and denominator
    f /= synth_xtal_freq; // each is 20 bits (range 0..1048575)
    num = f; // the actual multiplier is mult + num / denom
    denom = 1048575; // For simplicity we set the denominator to the maximum 1048575
    num = num / 100;
  }
  else // lower freq than 1MHz - use output Divider set to 128
  {
    rDiv = SI_R_DIV_128;
    //frequency = frequency * 128ULL; //Set base freq 128 times higher as we are dividing with 128 in the last output stage
    Divider = 90000000000ULL / (frequency * 128ULL);// Calculate the division ratio. 900MHz is the maximum VCO freq

    pllFreq = Divider * frequency * 128ULL; // Calculate the pllFrequency:
    //the Divider * desired output frequency
    mult = pllFreq / (synth_xtal_freq * 100UL); // Determine the multiplier to
    //get to the required pllFrequency
    l = pllFreq % (synth_xtal_freq * 100UL); // It has three parts:
    f = l; // mult is an integer that must be in the range 15..90
    f *= 1048575; // num and denom are the fractional parts, the numerator and denominator
    f /= synth_xtal_freq; // each is 20 bits (range 0..1048575)
    num = f; // the actual multiplier is mult + num / denom
    denom = 1048575; // For simplicity we set the denominator to the maximum 1048575
    num = num / 100;
  }


  // Set up PLL A with the calculated  multiplication ratio
  setupPLL(MultiSynth_frac_denom_reg_26, mult, num, denom);
															//clk0 and clk1 both use PLLA
  // Set up MultiSynth Divider 0, with the calculated Divider.
  // The final R division stage can divide by a power of two, from 1..128.
  // reprented by constants SI_R_DIV1 to SI_R_DIV128 (see si5351a.h header file)
  // If you want to output frequencies below 1MHz, you have to use the
  // final R division stage
  setupMultisynth(MS_Div_0_42, Divider, rDiv);
  setupMultisynth(50         , Divider, rDiv);   //reg 50 is same for clk1

  // Reset the PLL. This causes a glitch in the output. For small changes to
  // the parameters, you don't need to reset the PLL, and there is no glitch
  FreqChange = frequency - oldFreq;

 //if ( abs(FreqChange) > 100000) //If changed more than 1kHz then reset PLL (completely arbitrary choosen)
  //{
    i2cSendRegister(PLL_RESET_177, 0xA0);
  //}

  // Finally switch on the CLK0 output (0x4F)
  // and set the MultiSynth0 input to be PLL A
	i2cSendRegister(CLK_CNTRL_reg_16, 0x4F );   //x4F=full 8mA drive
	i2cSendRegister(17, 0x5F );  				//x5F=INVERTED clk1, full 8mA drive

	i2cSendRegister(3, 0xFC ); //reg 3 enable output 0 + 1
	oldFreq = frequency;

}



























