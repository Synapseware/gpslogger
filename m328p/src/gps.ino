#include "gpslogger.h"

volatile bool _gps_enabled		= true;

//------------------------------------------------------------------------
void initializeGPS(void)
{
	// Configure GPS FIX sense pin
	//GPS_DDR &= ~(1<<GPS_FIX);
	//GPS_PORT |= (1<<GPS_FIX);

	PRINTLN(F("GPS initializing"));

	GPS.begin(9600);

	// wait while fix line is low
	while ((GPS_PIN & (1<<GPS_FIX)) == 0)
	  wdt_reset();

	// wait for fix line to go low
	while ((GPS_PIN & (1<<GPS_FIX)) != 0)
	  wdt_reset();

	PRINTLN(F("GPS internal initialization complete.  Configuring for hot start, 1Hz updates, and RMCGGA only"));

	GPS.sendCommand(PSTR(PMTK_HOT_START));
	GPS.sendCommand(PSTR(PMTK_SET_NMEA_OUTPUT_RMCGGA));
	GPS.sendCommand(PSTR(PMTK_SET_NMEA_UPDATE_1HZ));

	PRINTLN(F("GPS configured"));
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

	GPS.sendCommand(PSTR(" "));

	while (GPS.read())
		wdt_reset();

	_gps_enabled = true;
}

//------------------------------------------------------------------------
void disableGPS(void)
{
	PRINTLN(F("Disabling GPS"));
	_gps_enabled = false;

	// wait while fix line is low
	while ((GPS_PIN & (1<<GPS_FIX)) == 0)
	  wdt_reset();

	// wait for fix line to go low
	while ((GPS_PIN & (1<<GPS_FIX)) != 0)
		wdt_reset();

	delay(10);
	GPS.sendCommand(PSTR(PMTK_STANDBY));
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
	GPS.waitForSentence(PSTR("$GPRMC,"));

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
