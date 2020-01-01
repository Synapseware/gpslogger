#ifndef __ADAFRUIT_GPS__
#define __ADAFRUIT_GPS__


#include "gpslogger.h"



#include <LUFA/Drivers/Peripheral/Serial.h>
//#include <LUFA/Drivers/Misc/RingBuffer.h>

/*
Typical GPS conversation:
	> board to GPS
	< GPS to board

> ' ' (wakeup)
	< $PMTK010,001*2E		(PGACK_CONFIG_COMPLETE)
	< $PMTK010,002*2D		(PMTK_AWAKE)

> $PMTK101*32 (hot start)
	< $PGACK,103*40			(PGACK_COLD_STARTUP)
	< $PGACK,105*46			(PGACK_INIT_COMPLETE)
	< $PMTK011,MTKGPS*08	(PGACK_CHIP_IDENT)
	< $PMTK010,001*2E		(PGACK_CONFIG_COMPLETE)
	< $PMTK010,002*2D		(PMTK_AWAKE)

> $PMTK605*31 (version query)
	< $PMTK705,AXN_2.10_3339_2012072601,5223,PA6H,1.0*6A

> $PMTK220,100*2F (1Hz output)
	< $PMTK001,220,3*30		(PMTK_SET_NMEA_UPDATE_1HZ_OK)

> $PMTK314,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0*29 (GPRMC only)
	< $PMTK001,314,3*36		(PMTK_SET_NMEA_OUTPUT_OK)
	< $GPRMC,045851.000,A,4803.0705,N,12249.8647,W,1.27,21.97,281115,,,A*4A
	< $GPRMC,045852.000,A,4803.0706,N,12249.8646,W,1.29,22.26,281115,,,A*4C

> $PMTK314,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0*28 (output off)
	< $PMTK001,314,3*36		(PMTK_SET_NMEA_OUTPUT_OK)

> $PMTK161,0*28 (suspending)
	< $PMTK001,161,3*36		(PMTK_STANDBY_SUCCESS)

*/


// different commands to set the update rate from once a second (1 Hz) to 10 times a second (10Hz)
#define PMTK_SET_NMEA_UPDATE_1HZ		PSTR("$PMTK220,1000*1F")
#define PMTK_SET_NMEA_UPDATE_1HZ_OK		PSTR("$PMTK001,220,3*30")
#define PMTK_SET_NMEA_UPDATE_5HZ		PSTR("$PMTK220,200*2C")
#define PMTK_SET_NMEA_UPDATE_10HZ		PSTR("$PMTK220,100*2F")

#define PMTK_CMD_HOT_START				PSTR("$PMTK101*32")
#define PMTK_CMD_WARM_START				PSTR("$PMTK102*31")
#define PMTK_CMD_COLD_START				PSTR("$PMTK103*30")

#define PMTK_SET_BAUD_57600				PSTR("$PMTK251,57600*2C")
#define PMTK_SET_BAUD_9600				PSTR("$PMTK251,9600*17")

// turn on only the second sentence (GPRMC)
#define PMTK_SET_NMEA_OUTPUT_RMCONLY	PSTR("$PMTK314,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0*29")
// turn on GPRMC and GGA
#define PMTK_SET_NMEA_OUTPUT_RMCGGA		PSTR("$PMTK314,0,1,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0*28")
// turn on ALL THE DATA
#define PMTK_SET_NMEA_OUTPUT_ALLDATA	PSTR("$PMTK314,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0*28")
// turn off output
#define PMTK_SET_NMEA_OUTPUT_OFF		PSTR("$PMTK314,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0*28")

// response to output changes
#define PMTK_SET_NMEA_OUTPUT_OK			PSTR("$PMTK001,314,3*36")

// to generate your own sentences, check out the MTK command datasheet and use a checksum calculator
// such as the awesome http://www.hhhh.org/wiml/proj/nmeaxor.html

#define PMTK_LOCUS_STARTLOG				PSTR("$PMTK185,0*22")
#define PMTK_LOCUS_STOPLOG				PSTR("$PMTK185,1*23")
#define PMTK_LOCUS_STARTSTOPACK			PSTR("$PMTK001,185,3*3C")
#define PMTK_LOCUS_QUERY_STATUS			PSTR("$PMTK183*38")
#define PMTK_LOCUS_ERASE_FLASH			PSTR("$PMTK184,1*22")
#define LOCUS_OVERLAP					0
#define LOCUS_FULLSTOP					1

// standby command & boot successful message
#define PMTK_STANDBY					PSTR("$PMTK161,0*28")
#define PMTK_STANDBY_SUCCESS			PSTR("$PMTK001,161,3*36")
#define PMTK_AWAKE						PSTR("$PMTK010,002*2D")

// ask for the release and version
// returns something like "$PMTK705,AXN_2.10_3339_2012072601,5223,PA6H,1.0*6A"
#define PMTK_Q_RELEASE					PSTR("$PMTK605*31")
#define PMTK_VERSION_HEADER				PSTR("$PMTK705,AXN")

// request for updates on antenna status 
#define PGCMD_ANTENNA					PSTR("$PGCMD,33,1*6C")
#define PGCMD_NOANTENNA					PSTR("$PGCMD,33,0*6D")

// NMEA sentence types
#define PMTK_GPGGA_HEADER				PSTR("$GPGGA")
#define PMTK_GPRMC_HEADER				PSTR("$GPRMC")
#define PMTK_HEADER						PSTR("$PMTK")

// startup status messages
#define PGACK_COLD_STARTUP				PSTR("$PGACK,103*40")
#define PGACK_INIT_COMPLETE				PSTR("$PGACK,105*46")
#define PGACK_CHIP_IDENT				PSTR("$PMTK011,MTKGPS*08")
#define PGACK_CONFIG_COMPLETE			PSTR("$PMTK010,001*2E")

typedef uint16_t flags_t;
#define ADAFRUIT_GPS_FLAGS_COMPLETE		0b0000000000000001UL
#define ADAFRUIT_GPS_FLAGS_START		0b0000000000000010UL
#define ADAFRUIT_GPS_FLAGS_IDENT		0b0000000000000100UL
#define ADAFRUIT_GPS_FLAGS_AWAKE		0b0000000000001000UL
#define ADAFRUIT_GPS_FLAGS_1HZ_OK		0b0000000000010000UL
#define ADAFRUIT_GPS_FLAGS_STANDBYOK	0b0000000000100000UL
#define ADAFRUIT_GPS_FLAGS_VERSION		0b0000000001000000UL
#define ADAFRUIT_GPS_FLAGS_OUTPUT_CHG	0b0000000010000000UL
#define ADAFRUIT_GPS_FLAGS_INIT_OK		0b0000000100000000UL


typedef void (*Adafruit_GPS_OnMessageReceived_t)(const char*);


void Adafruit_GPS_Init(Adafruit_GPS_OnMessageReceived_t);
void Adafruit_GPS_ResetState(void);
flags_t Adafruit_GPS_CurrentState(void);
bool Adafruit_GPS_WaitForStatus(char, int16_t);
bool Adafruit_GPS_WaitForPPS(int16_t);
void Adafruit_GPS_Enable(void);
void Adafruit_GPS_Disable(void);
void Adafruit_GPS_Suspend(int16_t);
void Adafruit_GPS_Wakeup(void);

void Adafruit_GPS_Update1Hz(int16_t);
void Adafruit_GPS_Update5Hz(int16_t);
void Adafruit_GPS_Update10Hz(int16_t);
void Adafruit_GPS_ColdStart(int16_t);
void Adafruit_GPS_WarmStart(void);
void Adafruit_GPS_HotStart(void);

void Adafruit_GPS_RMCOnly(int16_t);
void Adafruit_GPS_RMCGGADual(int16_t);
void Adafruit_GPS_AllData(int16_t);
void Adafruit_GPS_OutputOff(void);

bool Adafruit_GPS_AskForVersion(int16_t, char*, uint16_t);

void HandleGpsSerialInterrupt(const char);

#endif