#ifndef __GPS_LOGGER_H__
#define __GPS_LOGGER_H__


#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
//#include <GpsInterface.h>
#include <HardwareSerial.h>
#include <Adafruit_GPS.h>


// The error LED is used to indicate a hard/unrecoverable failure state
#define ERR_LED_PORT		PORTB
#define ERR_LED_DDR			DDRB
#define ERR_LED_pin			PB7
#define ERR_LED_bm			(1<<ERR_LED_pin)

// Built in LED is on D13, which is C7
// We'll use D5 on PC6, which is also the OC3A pin...)
// This is used by Timer3 to show the heartbeat
#define EXT_LED_PORT		PORTC
#define EXT_LED_DDR			DDRC
#define EXT_LED_pin			PC6
#define EXT_LED_bm			(1<<EXT_LED_pin)

// Debug LED
#define DBG_LED_PORT		PORTB
#define DBG_LED_DDR			DDRB
#define DBG_LED_pin			PB6
#define DBG_LED_bm			(1<<DBG_LED_pin)

// Status LED
#define STA_LED_PORT		PORTC
#define STA_LED_DDR			DDRC
#define STA_LED_pin			PC7
#define STA_LED_bm			(1<<STA_LED_pin)

// https://www.arduino.cc/en/Hacking/PinMapping32u4
// On the Leonardo, Port B has 4 pins free
// PB4 ( 8) GPS Enable
// PB5 ( 9) GPX Fix input
// PB6 (10)	Err LED
// PB7 (11) Debug LED
#define GPS_PORT			PORTB
#define GPS_DDR				DDRB
#define GPS_PINX			PINB
#define GPS_EN_pin			PB4
#define GPS_EN_bm			(1<<GPS_EN_pin)
#define GPS_FIX_pin			PB5
#define GPS_FIX_bm			(1<<GPS_FIX_pin)


// Special/additional GPS commands
#define PMTK_GPGGA_HEADER				"$GPGGA"
#define PMTK_GPRMC_HEADER				"$GPRMC"
#define PMTK_HEADER						"$PMTK"
#define PMTK_SET_NMEA_OUTPUT_OK			"$PMTK001,314,3*36"
#define PGACK_COLD_STARTUP				"$PGACK,103*40"
#define PGACK_INIT_COMPLETE				"$PGACK,105*46"
#define PGACK_CHIP_IDENT				"$PMTK011,MTKGPS*08"
#define PGACK_CONFIG_COMPLETE			"$PMTK010,001*2E"
#define PMTK_VERSION_HEADER				"$PMTK705,AXN_"
#define PMTK_SET_NMEA_UPDATE_1HZ_OK		"$PMTK001,220,3*30"



// ----------------------------------------------------------------------------
// State machine values
typedef uint8_t state_t;
#define STATE_NONE					0
#define STATE_FAILURE				127
#define STATE_HALT					126
#define STATE_NEXT					125

// Startup states
#define STATE_STARTUP				1
#define STATE_START_WAIT			2
#define STATE_START_DONE			3


// Fix line states
#define STATE_FIX_WAIT				5
#define STATE_FIX_START				6
#define STATE_FIX_LOW_HIGH			7
#define STATE_FIX_HIGH_WAIT			8
#define STATE_FIX_LOW_WAIT			9


// RMC logging
#define STATE_RMCONLY_WAIT			10
#define STATE_RMCONLY_START			11
#define STATE_RMCONLY_SEND			12
#define STATE_RMCONLY_STARTING		13


// 1Hz updates
#define STATE_PERIOD_WAIT			20
#define STATE_PERIOD_START			21
#define STATE_PERIOD_STARTING		22

#define STATE_INIT_WAIT				30
#define STATE_INIT_START			31
#define STATE_INIT_WATCH			32

// Version query
#define STATE_VERSION_WAIT			100
#define STATE_VERSION_START			101
#define STATE_VERSION_STARTING		102


// ----------------------------------------------------------------------------
// These flags are used to determine what data we've received from the GPS
typedef uint16_t flags_t;
#define FLAGS_NONE			0
#define FLAGS_COMPLETE		(1<<0)
#define FLAGS_START			(1<<1)
#define FLAGS_IDENT			(1<<2)
#define FLAGS_AWAKE			(1<<3)
#define FLAGS_1HZ_OK		(1<<4)
#define FLAGS_STANDBYOK		(1<<5)
#define FLAGS_VERSION		(1<<6)
#define FLAGS_OUTPUT_CHG	(1<<7)
#define FLAGS_INIT_OK		(1<<8)


//
/*
After enabling GPS:
- PGACK,103*40				PGACK_COLD_STARTUP
- PGACK,105*46				PGACK_INIT_COMPLETE
- PMTK011,MTKGPS*08			PGACK_CHIP_IDENT
- PMTK010,001*2E			PGACK_CONFIG_COMPLETE



Hello.  GPS logger here.
Waiting for fix line ready.
Fix line is active.
Waiting for Init OK and Cold start flags
GPS initialization completed.
  set log level to RMC only
$GPRMC,235942.800,V,,,,,0.00,0.00,050180,,,N*42    Flags: 100001111
  done setting log level
  set logging frequency to 1Hz
  setting log frequency
  ask for version
  version: 2.10.3339
$GPRMC,235943.800,V,,,,,0.00,0.00,050180,,,N*43    Flags: 111011111

Example of location data
$GPRMC,175157.000,A,4746.5413,N,12223.2713,W,1.14,333.31,151219,,,A*73    Flags: 111011111


*/

#endif
