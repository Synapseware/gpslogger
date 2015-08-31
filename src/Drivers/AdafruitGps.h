#ifndef __ADAFRUIT_GPS__
#define __ADAFRUIT_GPS__



#include <LUFA/Drivers/Peripheral/Serial.h>
#include <LUFA/Drivers/Misc/RingBuffer.h>



// different commands to set the update rate from once a second (1 Hz) to 10 times a second (10Hz)
#define PMTK_SET_NMEA_UPDATE_1HZ		"$PMTK220,1000*1F"
#define PMTK_SET_NMEA_UPDATE_5HZ		"$PMTK220,200*2C"
#define PMTK_SET_NMEA_UPDATE_10HZ		"$PMTK220,100*2F"

#define PMTK_CMD_COLD_START				"$PMTK103*30"

#define PMTK_SET_BAUD_57600				"$PMTK251,57600*2C"
#define PMTK_SET_BAUD_9600				"$PMTK251,9600*17"

// turn on only the second sentence (GPRMC)
#define PMTK_SET_NMEA_OUTPUT_RMCONLY	"$PMTK314,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0*29"
// turn on GPRMC and GGA
#define PMTK_SET_NMEA_OUTPUT_RMCGGA		"$PMTK314,0,1,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0*28"
// turn on ALL THE DATA
#define PMTK_SET_NMEA_OUTPUT_ALLDATA	"$PMTK314,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0*28"
// turn off output
#define PMTK_SET_NMEA_OUTPUT_OFF		"$PMTK314,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0*28"

// to generate your own sentences, check out the MTK command datasheet and use a checksum calculator
// such as the awesome http://www.hhhh.org/wiml/proj/nmeaxor.html

#define PMTK_LOCUS_STARTLOG				"$PMTK185,0*22"
#define PMTK_LOCUS_STOPLOG				"$PMTK185,1*23"
#define PMTK_LOCUS_STARTSTOPACK			"$PMTK001,185,3*3C"
#define PMTK_LOCUS_QUERY_STATUS			"$PMTK183*38"
#define PMTK_LOCUS_ERASE_FLASH			"$PMTK184,1*22"
#define LOCUS_OVERLAP					0
#define LOCUS_FULLSTOP					1

// standby command & boot successful message
#define PMTK_STANDBY					"$PMTK161,0*28"
#define PMTK_STANDBY_SUCCESS			"$PMTK001,161,3*36"  // Not needed currently
#define PMTK_AWAKE						"$PMTK010,002*2D"
#define PMTK_VERSION_HEADER				"$PMTK705,"

// ask for the release and version
#define PMTK_Q_RELEASE					"$PMTK605*31" // returns something like "$PMTK705,AXN_2.10_3339_2012072601,5223,PA6H,1.0*6A"

// request for updates on antenna status 
#define PGCMD_ANTENNA					"$PGCMD,33,1*6C" 
#define PGCMD_NOANTENNA					"$PGCMD,33,0*6D" 

// NMEA sentence types
#define NMEA_GPGGA						"$GPGGA"
#define NMEA_GPRMC						"$GPRMC"
#define NMEA_PMTK						"$PMTK"

// startup status messages
#define PGACK_COLD_STARTUP				"$PGACK,103*40"
#define PGACK_INIT_COMPLETE				"$PGACK,105*46"
#define PGACK_CHIP_IDENT				"$PMTK011,MTKGPS*08"
#define PGACK_CONFIG_COMPLETE			"$PMTK010,001*2E"

#define ADAFRUIT_GPS_FLAGS_INIT			(1<<0)
#define ADAFRUIT_GPS_FLAGS_COLD_START	(1<<1)
#define ADAFRUIT_GPS_FLAGS_IDENT		(1<<2)
#define ADAFRUIT_GPS_FLAGS_AWAKE		(1<<3)
#define ADAFRUIT_GPS_FLAGS_READY		(1<<4)
#define ADAFRUIT_GPS_FLAGS_STANDBYOK	(1<<5)


typedef void (*Adafruit_GPS_OnMessageReceived_t)(void);

void Adafruit_GPS_Init(USART_t* const usart, PORT_t* en_port, uint8_t en_pin);
void Adafruit_GPS_Enable(void);
void Adafruit_GPS_Disable(void);
void Adafruit_GPS_SendCommand(const char* command);
void Adafruit_GPS_OnMessageReceived(Adafruit_GPS_OnMessageReceived_t handler);
int8_t Adafruit_GPS_MessageAvailable(void);
int Adafruit_GPS_CloneLastMessage(char* destination, int destSize);
void Adafruit_GPS_Suspend(void);
void Adafruit_GPS_Wakeup(void);

static bool Adafruit_GPS_ModuleStatus(uint8_t MASK);

void Adafruit_GPS_Update1Hz(void);
void Adafruit_GPS_Update5Hz(void);
void Adafruit_GPS_Update10Hz(void);
void Adafruit_GPS_ColdStart(void);

void Adafruit_GPS_RMCOnly(void);
void Adafruit_GPS_RMCGGADual(void);
void Adafruit_GPS_AllData(void);
void Adafruit_GPS_OutputOff(void);



#endif