#include "gpslogger.h"

volatile bool _gps_enabled		= true;

//------------------------------------------------------------------------
void initializeGPS(void)
{
	PRINTLN(F("GPS initializing"));

	GPS.begin(9600);
	GPS.sendCommand(PMTK_HOT_START);
	GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCGGA);
	GPS.sendCommand(PMTK_SET_NMEA_UPDATE_1HZ);
}

//------------------------------------------------------------------------
// pulls data off the GPS
void spoolData(void)
{
	GPS.read();
}

//------------------------------------------------------------------------
void enableGPS(void)
{
	PRINTLN(F("Enabling GPS"));

	delay(100);
	
	while (GPS.read())
		wdt_reset();

	GpsSerial.print(0xAA);
	_gps_enabled = true;
}

//------------------------------------------------------------------------
void disableGPS(void)
{
	PRINTLN(F("Disabling GPS"));
	_gps_enabled = false;

	// wait for fix line to go low
	pinMode(GPS_FIX, INPUT);
	digitalWrite(GPS_FIX, HIGH);
	while(digitalRead(GPS_FIX))
		wdt_reset();
	pinMode(GPS_FIX, OUTPUT);

	GPS.sendCommand(PMTK_STANDBY);
}

//------------------------------------------------------------------------
bool isEnabled(void)
{
	return _gps_enabled;
}

//------------------------------------------------------------------------
// Parses the latest NMEA sentence and returns a pointer to the global
// array if the parse was successful and we have a fix (otherwise null)
char* parseGPSData(void)
{
	memset(_nmeaSentence, 0, sizeof(_nmeaSentence)*sizeof(char));

	// block while we wait for GPS data
	GPS.waitForSentence("$GPRMC,");

	// copy the current sentence to our local buffer
	strcpy(_nmeaSentence, GPS.lastNMEA());

	if (!GPS.parse(_nmeaSentence))
		return NULL;

	if (!GPS.fix)
		return NULL;

	PRINT(F("NMEA: "));
	PRINTLN(_nmeaSentence);

	return _nmeaSentence;
}
