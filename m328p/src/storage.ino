#include "gpslogger.h"

//------------------------------------------------------------------------
// Configures the SD card and SPI interface
bool initializeSDCard(void)
{
	// set date time callback function
	SdFile::dateTimeCallback(dateTime);

	bool init = false;
	for (uint8_t x = 0; x < 3; x++)
	{
		if (sd.begin(SD_CS, SPI_HALF_SPEED))
		{
			init = true;
			PRINTLN(F("SD initialized"));
			break;
		}

		PRINTLN(F("SD failure"));
		delay(1000);
	}

	if (!init)
		return false;

#ifdef print_enabled
/*
	SdFile readme;
	readme.open("README.txt");
	int r;
	while ((r = readme.read()) > 0)
		PRINT((char)r);
	readme.close();
*/
#endif

	return true;
}

//------------------------------------------------------------------------
// User provided date time callback function.
// See SdFile::dateTimeCallback() for usage.
void dateTime(uint16_t* date, uint16_t* time)
{
	// User gets date and time from GPS or real-time
	// clock in real callback function

	// return date using FAT_DATE macro to format fields
	*date = FAT_DATE(GPS.year, GPS.month, GPS.day);

	// return time using FAT_TIME macro to format fields
	*time = FAT_TIME(GPS.hour, GPS.minute, GPS.seconds);
}

//------------------------------------------------------------------------
// Formats the filename based on the current date.  Returns NULL if no
// valid date exists (GPS doesn't have a fix)
char* generateFilename(char* fileName)
{
	if (!GPS.fix)
		return NULL;

	// generate filename
	sprintf_P(fileName, PSTR("/LOGS/20%02d%02d%02d.LOG"),
		GPS.year,
		GPS.month,
		GPS.day);

	return fileName;
}

//------------------------------------------------------------------------
// Dumps the parsed GPS data
void printLogData(void)
{
	PRINT("\nTime: ");
	PRINT(GPS.hour); PRINT(':');
	PRINT(GPS.minute); PRINT(':');
	PRINT(GPS.seconds); PRINT('.');
	PRINTLN(GPS.milliseconds);
	PRINT("Date: ");
	PRINT(GPS.day); PRINT('/');
	PRINT(GPS.month); PRINT("/20");
	PRINTLN(GPS.year);
	PRINT("Fix: "); PRINT((int)GPS.fix);
	PRINT(" quality: "); PRINTLN((int)GPS.fixquality);

	float lat = convertDegMinToDecDeg(GPS.latitude);
	float lon = convertDegMinToDecDeg(GPS.longitude);

	PRINT("Location: ");
	PRINT(lat);
	PRINT(", ");
	PRINTLN(lon);

	PRINT("Speed (knots): "); PRINTLN(GPS.speed);
	PRINT("Angle: "); PRINTLN(GPS.angle);
	PRINT("Altitude: "); PRINTLN(GPS.altitude);
	PRINT("Satellites: "); PRINTLN((int)GPS.satellites);
}

//------------------------------------------------------------------------
// Saves the NMEA sentence buffer to the daily log file
bool appendLogData(const char* nmeadata)
{
	if (NULL == nmeadata)
		return false;

	char* fileName = generateFilename(_fileName);
	if (NULL == fileName)
		return false;

	PRINTLN(F("Saving GPS data"));
	PRINT(F("Filename: "));
	PRINTLN(fileName);

	// ensure logs directory
	char dir[] = "/LOGS";
	if (!sd.chdir(dir))
		sd.mkdir(dir);
	sd.chdir("/");

	SdFile fout;
	if (fout.open(fileName, O_WRITE | O_APPEND | O_CREAT))
	{
		fout.write(nmeadata);
		fout.write('\n');
		fout.close();
	}

	printLogData();

	return true;
}

