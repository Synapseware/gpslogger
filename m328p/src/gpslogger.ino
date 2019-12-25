/*
#include <SdVolume.h>
#include <SdStream.h>
#include <SdSpi.h>
#include <SdInfo.h>
#include <SdFile.h>
#include <SdFatUtil.h>
#include <SdFatStructs.h>
#include <SdFatmainpage.h>
#include <SdFatConfig.h>
#include <SdFat.h>
#include <SdBaseFile.h>
#include <Sd2Card.h>
#include <ostream.h>
#include <MinimumSerial.h>
#include <istream.h>
#include <iostream.h>
#include <ios.h>
#include <bufstream.h>
*/

#include <SPI.h>
#include "SdFat.h"
#include "sdios.h"

#include <avr/sleep.h>

//#include <ArduinoStream.h>
#include <SoftwareSerial.h>
#include <Adafruit_GPS.h>
#include "gpslogger.h"

SoftwareSerial GpsSerial(GPS_RX, GPS_TX); // RX, TX
Adafruit_GPS GPS(&GpsSerial);

SdFat sd;
Sd2Card card;


volatile bool		_processBatt		= false;
volatile bool		_secondsTick		= false;
volatile int		_battery			= 0;
volatile uint16_t	_fixTimeout			= 0;
volatile uint16_t	_recordingTimeout	= 0;
volatile uint16_t	_lastReset			= 0;


static char _nmeaSentence[120];			// buffer to store NMEA sentence.
static char _fileName[64];				// buffer to store filename


void (*reset)(void) = 0;


//------------------------------------------------------------------------
void initializeGlobals(void)
{
	_processBatt		= false;
	_secondsTick		= false;
	_fixTimeout			= FIX_TIMEOUT;
	_recordingTimeout	= 10;
	_lastReset			= SECONDS_BETWEEN_RESETS;
}

//------------------------------------------------------------------------
// Configures timer1 (16 bit timer) so it generates an interrupt every
// second.  Resolution is about 63kc.  Used for timing the FIX signal
void initializeLogTimer(void)
{
	TCCR1A	=	(0<<WGM11)	|
		(0<<WGM10);

	TCCR1B	=	(0<<WGM13)	|
		(1<<WGM12)	|	// CTC (reset at OCR1A)
		(1<<CS12)	|	// CPUclk/256 (16MHz / 256 = 62,500 ticks / second)
		(0<<CS11)	|
		(0<<CS10);

	TCCR1C	=	0;
	TIMSK1	=	(1<<OCIE1A);
	OCR1A	=	F_CPU/256-1;		// 16MHz/256/62500-1 = 62499
}

//------------------------------------------------------------------------
// Timer0 is already used for millis() - we'll just interrupt somewhere
// in the middle and call the "Compare A" function
void initializeMillsInt(void)
{
	OCR0A = 0xAF;
	TIMSK0 |= _BV(OCIE0A);
}

//------------------------------------------------------------------------
void setup()
{
	wdt_disable();

	initializeGlobals();

	// set power-saving options
	power_all_enable();
	power_adc_disable();
	power_twi_disable();
	power_timer2_disable();

	pinMode(DBG_LED, OUTPUT);
	pinMode(SD_CS, OUTPUT);

	// Don't leave this behind for prodution builds!
	Serial.begin(115200);
	PRINTLN(F("GPS Logger Setup"));

	initializeSDCard();

	initializeLogTimer();

	initializeMillsInt();

	// prepare GPS
	initializeGPS();

	// shutoff the GPS
	delay(500);
	disableGPS();

	// enable the watchdog timeout on 2 second intervals (this should be done via fuses...)
	//wdt_enable(WDTO_2S);

	set_sleep_mode(SLEEP_MODE_IDLE);

#ifdef print_enabled
	if (FIX_TIMEOUT >= RECORDING_TIMEOUT)
	{
		PRINTLN(F("FIX_TIMEOUT must be less than RECORDING_TIMEOUT.  ERROR."));
		while(1);
	}
#endif
}

//------------------------------------------------------------------------
void loop()
{
	if (_secondsTick)
	{
		processLifespan();
		_secondsTick = false;
	}

	processLog();

	processBattery();

	snooze();
}

//------------------------------------------------------------------------
// shut down while we wait for a wake-up event
void snooze(void)
{
	sleep_enable();
	sleep_cpu();
	sleep_disable();
}

//------------------------------------------------------------------------
// performs wake/sleep timeouts
void processLifespan(void)
{
	if (_lastReset > 0)
		_lastReset--;

	// decrement the recording timeout
	if (_recordingTimeout > 0)
	{
		if (_lastReset == 0)
		{
			reset();
			return;
		}

		PRINT(F("Next sampling: "));
		PRINTLN(_recordingTimeout);
		_recordingTimeout--;
	}

	// recording timeout reached, enable the GPS module and try to get a fix
	if (_recordingTimeout == 0)
	{
		_fixTimeout = FIX_TIMEOUT;
		_recordingTimeout = RECORDING_TIMEOUT;

		// enabling the GPS allows the module to present an interrupt on INT0
		// to the Arduino.  The INT0 signal is used to determine fix status
		enableGPS();

		return;
	}

	if (isEnabled())
	{
		// decrement the GPS fix timeout if the GPS is enabled but we don't
		// have a valid satalite fix
		if (_fixTimeout > 0)
		{
			_fixTimeout--;
			PRINT(F("Fix timeout: "));
			PRINTLN(_fixTimeout);
			return;
		}

		// timeout reached - shutdown the GPS and try again later
		if (_fixTimeout == 0)
		{
			PRINTLN(F("Fix timeout reached.  Shutting down the GPS."));
			disableGPS();
			return;
		}
	}
}

//------------------------------------------------------------------------
void processLog(void)
{
	// don't do anything if we aren't supposed to yet
	if (isEnabled())
	{
		char* nmeadata = NULL;
		if (NULL != (nmeadata = parseGPSData()))
		{
			// we have good data - save it!
			if (appendLogData(nmeadata))
			{
				PRINTLN(F("GPS Data saved!"));

				// once the data saves OK, put the GPS back to sleep
				disableGPS();
			}
			else
				PRINTLN(F("Failed to save GPS data"));
		}
	}
}

//------------------------------------------------------------------------
// samples the battery voltage and sets the heartbeat LED to change
// BPM based on voltage (<3.0v = 3s, >3.0v = 10s)
void processBattery(void)
{
	if (!_processBatt)
		return;
	_processBatt = false;

	power_adc_enable();

	// let ADC settle
	delay(100);

	// y = (418/1.3)x - 13
	// 1.3v = 405
	// 2.6v = 823
	// 2.7v = 855
	// 3.0v = 952

	_battery = analogRead(BATT_SENSE);
	//PRINT(F("Battery: "));
	//PRINTLN(_battery);

	// shutting off the ADC between battery samples saves around 2ma
	power_adc_disable();
}

//------------------------------------------------------------------------
// Converts the lat/long from the GPS module, which outputs in degrees-minute
// format to the decimal-degrees format
double convertDegMinToDecDeg (float degMin)
{
	double min = 0.0;
	double decDeg = 0.0;

	//get the minutes, fmod() requires double
	min = fmod((double)degMin, 100.0);

	//rebuild coordinates in decimal degrees
	decDeg = (degMin / 100) + (min / 60);

	return decDeg;
}

//------------------------------------------------------------------------
// We intercept the existing timer0 to pull data from the GPS module
SIGNAL(TIMER0_COMPA_vect)
{
	wdt_reset();

	static unsigned long lastTick = 0;
	spoolData();

	if (lastTick == 0)
	{
		digitalWrite(DBG_LED, LOW);
		_processBatt = true;
	}

	if (lastTick == MILLIS_TO_BLINK)
		digitalWrite(DBG_LED, HIGH);

	lastTick++;
	if (_battery > LOW_BATTERY)
	{
		if (lastTick > MILLIS_BETWEEN_BLINK)
			lastTick = 0;
	}
	else
	{
		if (lastTick > MILLIS_BATTERY_LOW)
			lastTick = 0;
	}
}

//------------------------------------------------------------------------
// Timer which runs at a 1 second interrupt rate
SIGNAL(TIMER1_COMPA_vect)
{
	wdt_reset();

	// signal seconds ticker
	_secondsTick = true;
}
