#ifndef __GPSLOGGER__H_
#define __GPSLOGGER__H_

#include <avr/wdt.h>
#include <avr/sleep.h>
#include <avr/power.h>
#include <avr/interrupt.h>



//------------------------------------------------------------------------
// Designate pins
const int GPS_TX			= 5;
const int GPS_RX			= 6;
const int BATT_SENSE		= 0;

/*
 * GPX_FIX    2     PD2
 * GPX_TX     5     PD5
 * GPX_RX     6     PD6
 * DBG_LED    8     PB0 
 * SD_CS      10    PB2
 * BATT_SENSE n/a
*/

// We don't want to use Arduino's heavy-handed port manip code for something
// that takes only a byte or two with direct port access
#define SD_PORT         PORTB
#define SD_DDR          DDRB
#define SD_CS           PORTB2

#define DBG_LED_PORT    PORTB
#define DBG_LED_DDR     DDRB
#define DBG_LED         PORTB0

#define GPS_PORT        PORTD
#define GPS_DDR         DDRD
#define GPS_PIN         PIND
#define GPS_FIX         PORTD2

//------------------------------------------------------------------------
const uint32_t SECONDS_BETWEEN_RESETS	= 86400UL;

//------------------------------------------------------------------------
// Heartbeat delays for normal and low battery conditions
const uint16_t MILLIS_TO_BLINK			= 5;		// length of heartbeat
const uint16_t MILLIS_BETWEEN_BLINK		= 10000;	// ms delay between heartbeats
const uint16_t MILLIS_BATTERY_LOW		= 3000;		// ms delay between heartbeats
const int16_t  LOW_BATTERY				= 951;		// 3.0v


//------------------------------------------------------------------------
// number of seconds between fix-capture-record events
const uint16_t RECORDING_TIMEOUT		= (60 * 5);

// system will sleep if no fix after timeout (seconds)
const uint16_t FIX_TIMEOUT				= 120;


//------------------------------------------------------------------------
// Additional GPS module configuration strings
#define PMTK_HOT_START			"$PMTK101*32"

//#define PMTK_FULL_RESET			"$PMTK104*37"
//#define PMTK_PERIODIC_NORM		"$PMTK225,0*2B"
//#define PMTK_PERIODIC_INIT		"$PMTK225,1,60000,600000,120000,120000*16"
//#define PMTK_PERIODIC_LOCT		"$PMTK225,9*22"

// 5 minute timeout on backup mode
#define PMTK_BACKUP_MODE		"$PMTK291,7,0,300000,1*0D"

//#define print_enabled

#ifdef print_enabled
	#define PRINT(x)		  Serial.print(x)
	#define PRINTLN(x)		Serial.println(x)
#else
	#define PRINT(x)
	#define PRINTLN(x)
#endif


#endif
