// AVR core
#include <avr/io.h>
#include <avr/sleep.h>
#include <avr/power.h>

// Arduino Libraries
//#include <SPI.h>
#include <SoftwareSerial.h>

// Adafruit GPS Library
#include <Adafruit_GPS.h>

// SdFat - Adafruit Fork
//#include <SdFat.h>
//#include "sdios.h"

// Project includes
#include "gpslogger.h"

// Globals
//SoftwareSerial GpsSerial(GPS_RX, GPS_TX); // RX, TX
//Adafruit_GPS GPS(&GpsSerial);

//SdFat sd;
//Sd2Card card;


volatile bool		_secondsTick		= false;
volatile int		_battery			= 0;
volatile uint16_t	_fixTimeout			= 0;
volatile uint16_t	_recordingTimeout	= 0;
volatile uint32_t	_lastReset			= 0;


static char _nmeaSentence[120];			// buffer to store NMEA sentence.
static char _fileName[64];				// buffer to store filename


//void (*reset)(void) = 0;


//------------------------------------------------------------------------
void ClearDbgLed(void)
{
	DBG_LED_PORT |= (1<<DBG_LED);
}

//------------------------------------------------------------------------
void SetDbgLed(void)
{
	DBG_LED_PORT &= ~(1<<DBG_LED);
}

//------------------------------------------------------------------------
void initializeGlobals(void)
{
	_secondsTick		= false;
	_fixTimeout			= FIX_TIMEOUT;
	_recordingTimeout	= 10;
	_lastReset			= SECONDS_BETWEEN_RESETS;
	memset(_nmeaSentence, 0, sizeof(_nmeaSentence));
	memset(_fileName, 0, sizeof(_fileName));
}

//------------------------------------------------------------------------
// Configures timer1 (16 bit timer) so it generates an interrupt every
// second.  Resolution is about 63kc.
void initializeLogTimer(void)
{
	TCCR1A	=	(0<<WGM11)	|
		        (0<<WGM10);

	TCCR1B	=	(0<<WGM13)	|
        		(1<<WGM12)	|	// CTC (reset at OCR1A)
        		(1<<CS12)	|	// CPUclk/1024 (16MHz / 1024 = 15,625 ticks / second)
        		(0<<CS11)	|
        		(1<<CS10);

	TCCR1C	=	0;
	TIMSK1	=	(1<<OCIE1A) | (1<<OCIE1B);
	OCR1A	=	(F_CPU/1024)-1;		// 16MHz/1024/15625-1 = 15624
	OCR1B	=	50;
}

//------------------------------------------------------------------------
void setup(void)
{
	//wdt_disable();

	initializeGlobals();

	// Ensure USART pullups
	DDRD &= ~(1<<PD0);
	DDRD |= (1<<PD1);
	PORTD |= (1<<PD0) | (1<<PD1);

	// Configure the SD card chip-select pin
	//SD_PORT |= (1<<SD_CS);
	//SD_DDR |= (1<<SD_CS);

	// Configure the debug LED
	DBG_LED_DDR |= (1<<DBG_LED);

	SetDbgLed();

	// Don't leave this behind for prodution builds!
#ifdef print_enabled
	Serial.begin(9600);
	delay(500);
#endif
	PRINTLN(F("GPS Logger Setup"));
	PRINTLN(F("Version 2020"));

	//initializeSDCard();

	// prepare GPS
	//initializeGPS();

	// shutoff the GPS
	//disableGPS();

	// enable the watchdog timeout on 2 second intervals (this should be done via fuses...)
	// Before this can be enabled, the GPS wait for sentence must be changed so that
	// it does not block.  Basically, we need to bring over the state processing from
	// the m32u4 branch
	//wdt_enable(WDTO_2S);

	//set_sleep_mode(SLEEP_MODE_IDLE);

#ifdef print_enabled
	/*
	if (FIX_TIMEOUT >= RECORDING_TIMEOUT)
	{
		PRINTLN(F("FIX_TIMEOUT must be less than RECORDING_TIMEOUT.  ERROR."));
		while(1);
	}
	*/
#endif

	ClearDbgLed();

	// 
	initializeLogTimer();
}

//------------------------------------------------------------------------
void loop(void)
{
	//wdt_reset();
	//spoolData();

	if (_secondsTick)
	{
		PRINTLN(F("Processing..."));
		//processLifespan();
		_secondsTick = false;
	}

	//processLog();

	//processBattery();

	//snooze();
}

//------------------------------------------------------------------------
// shut down while we wait for a wake-up event
void snooze(void)
{
	//sleep_enable();
	//sleep_cpu();
	//sleep_disable();
}

//------------------------------------------------------------------------
// performs wake/sleep timeouts
void processLifespan(void)
{
	/*
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
	*/
}

//------------------------------------------------------------------------
void processLog(void)
{
	/*
	// don't do anything if we aren't supposed to yet
	if (!isEnabled())
		return;

	if (!parseGPSData())
	{
		return;
	}

	// we have good data - save it!
	if (appendLogData(_nmeaSentence, sizeof(_nmeaSentence))
	{
		PRINTLN(F("GPS Data saved!"));

		// once the data saves OK, put the GPS back to sleep
		disableGPS();
	}
	else
	{
		PRINTLN(F("Failed to save GPS data"));
	}
	*/
}

//------------------------------------------------------------------------
// samples the battery voltage and sets the heartbeat LED to change
// BPM based on voltage (<3.0v = 3s, >3.0v = 10s)
void processBattery(void)
{
	// y = (418/1.3)x - 13
	// 1.3v = 405
	// 2.6v = 823
	// 2.7v = 855
	// 3.0v = 952
	// 3.3v = 1023

	//_battery = analogRead(BATT_SENSE);
	//PRINT(F("Battery: "));
	//PRINTLN(_battery);
}

//------------------------------------------------------------------------
// Timer which runs at a 1 second interrupt rate
ISR(TIMER1_COMPA_vect)
{
	//wdt_reset();

	SetDbgLed();

	// signal seconds ticker
	_secondsTick = true;
}

//------------------------------------------------------------------------
// 
ISR(TIMER1_COMPB_vect)
{
	ClearDbgLed();
}