//#include <DataFlash.h>

#include <gpslogger.h>


Adafruit_GPS* gps = NULL;
FlashDriver* flash = NULL;

static char version[24];
static char message[128];
static int messageIdx;

// Used to buffer the message when saving it to flash
static char buffer[128];

// State machine
static STATE_t _state;

// capture the tick flag from the timer file
extern bool _secondsTick;


// ----------------------------------------------------------------------------
// 
void setup(void)
{
	_state = STATE_START;
	configureLeds();
	configureGpsPins();
	setupTimer3();
	memset(version, 0, sizeof(version));
	resetReceiveBuffer();

	// start the GPS serial immediately so we can start saving up the data
	Serial1.begin(9600);
	gps = new Adafruit_GPS(&Serial1);

	// wait for USB interface to become ready
	Serial.begin(115200);	
	setErrLed();
	for (uint8_t i = 0; i < 10 && !Serial; i++)
	{
		delay(500);
	}
	clearErrLed();
	Serial.println(F("Clear screen, 5 second wait..."));
	delay(5000);
	Serial.println(F("Hello.  GPS logger here."));

	// Disable the GPS
	GPS_PORT &= ~(GPS_EN_bm);

	flash = new FlashDriver(&SPI);

	sei();
}


// ----------------------------------------------------------------------------
// Halts and waits for the GPS Fix line to toggle.  Ensures the module is ready.
void processFixState(STATE_t* pstate, STATE_t next)
{
	static STATE_t state = STATE_START;
	switch (state)
	{
		// Wait for the GPS Fix pin to be low
		case STATE_START:
			Serial.println(F("Waiting for fix line ready."));
			GPS_PORT |= (GPS_EN_bm);
			state = STATE_FIX_LOW_HIGH;
			break;

		case STATE_FIX_LOW_HIGH:
			if (!(GPS_PINX & GPS_FIX_bm))
			{
				state = STATE_FIX_HIGH_WAIT;
			}
			break;

		// Wait for the GPS Fix pin to be high
		case STATE_FIX_HIGH_WAIT:
			if ((GPS_PINX & GPS_FIX_bm))
			{
				state = STATE_FIX_LOW_WAIT;
			}
			break;

		// If the fix line
		case STATE_FIX_LOW_WAIT:
			if (!(GPS_PINX & GPS_FIX_bm))
			{
				Serial.println(F("Fix line is active."));
				state = STATE_START;
				*pstate = next;
			}
			break;
	}
}


// ----------------------------------------------------------------------------
// Waits for the Complete and Init OK flags to be set
void processGpsInitialize(STATE_t* pstate, STATE_t next)
{
	static STATE_t state = STATE_START;
	switch (state)
	{
		case STATE_START:
			Serial.println(F("Waiting for Init OK and Cold start flags"));
			state = STATE_INIT_WATCH;
			break;
		case STATE_INIT_WATCH:
			if (hasStatus(FLAGS_INIT_OK) && hasStatus(FLAGS_START))
			{
				Serial.println(F("GPS initialization completed."));
				state = STATE_START;
				*pstate = next;
			}
			break;
	}
}


// ----------------------------------------------------------------------------
// Processes a basic start sequence
void processSetLogLevel(STATE_t* pstate, STATE_t next)
{
	static STATE_t state = STATE_START;
	switch (state)
	{
		case STATE_START:
			if (!hasStatus(FLAGS_OUTPUT_CHG))
			{
				Serial.println(F("  set log level to RMC only"));
				Serial1.println(F(PMTK_SET_NMEA_OUTPUT_RMCONLY));
			}
			else
			{
				Serial.println(F("  RMC Only log level already set."));
				*pstate = next;
			}

			state = STATE_RMCONLY_STARTING;

			break;
		case STATE_RMCONLY_STARTING:
			if (hasStatus(FLAGS_OUTPUT_CHG))
			{
				Serial.println(F("  done setting log level"));
				state = STATE_START;
				*pstate = next;
			}
			break;
	}
}


// ----------------------------------------------------------------------------
// Processes the logging frequency request
void processSetLoggingFrequency(STATE_t* pstate, STATE_t next)
{
	static STATE_t state = STATE_START;
	switch (state)
	{
		case STATE_START:
			if (!hasStatus(FLAGS_1HZ_OK))
			{
				Serial.println(F("  set logging frequency to 1Hz"));
				Serial1.println(F(PMTK_SET_NMEA_UPDATE_1HZ));
				state = STATE_PERIOD_STARTING;
			}
			else
			{
				Serial.println(F("  logging frequency already set to 1Hz."));
				*pstate = next;
			}

			break;

		case STATE_PERIOD_STARTING:
			if (hasStatus(FLAGS_1HZ_OK))
			{
				Serial.println(F("  setting log frequency"));
				state = STATE_START;
				*pstate = next;
			}
			break;
	}
}


// ----------------------------------------------------------------------------
// Processes the logging frequency request
void processVersionRequestState(STATE_t* pstate, STATE_t next)
{
	static STATE_t state = STATE_START;
	switch (state)
	{
		case STATE_START:
			Serial.println(F("  ask for version"));
			Serial1.println(F(PMTK_Q_RELEASE));

			state = STATE_VERSION_STARTING;
			break;

		case STATE_VERSION_STARTING:
			if (hasStatus(FLAGS_VERSION))
			{
				Serial.print(F("  version: "));
				Serial.println(version);
				state = STATE_VERSION_START;
				*pstate = next;
			}
			break;
	}
}


// ----------------------------------------------------------------------------
// Processes the saving of a sentence to the flash
void processSaveDataToFlash(STATE_t* pstate, STATE_t next)
{
	static STATE_t state = STATE_START;
	switch (state)
	{
		case STATE_START:
			Serial.println(F("  saving NMEA sentence to flash"));

			// Copy the sentence to be logged to the flash IO buffer
			memset(buffer, 0, sizeof(buffer));
			strncpy(buffer, message, sizeof(buffer));
			state = STATE_FLASH_WRITE_BUFFER;
			break;

		case STATE_FLASH_WRITE_BUFFER:
			*pstate = next;
			state = STATE_START;
			Serial.println(buffer);
			memset(buffer, 0, sizeof(buffer));
			break;
	}
}


// ----------------------------------------------------------------------------
// 
void loop(void)
{
	static int count = 0;
	static STATE_t nextState = STATE_NONE;
	static STATE_t state = STATE_START;

	toggleFixLed();

	while (gps->available())
	{
		char data = gps->read();
		processReceivedData(data);
	}

	if (hasMessage())
	{
		if (processMessage())
		{
			state = STATE_SAVE_NMEA;
		}
		else
		{
			resetReceiveBuffer();	
		}
	}

	// Set state if next state should be processed
	if (nextState != STATE_NONE)
	{
		state = nextState;
		nextState = STATE_NONE;
	}

	// GPS state machine
	switch (state)
	{
		case STATE_NONE:
			break;

		case STATE_START:
			state = STATE_FIX_WAIT;
			break;

		case STATE_FIX_WAIT:
			processFixState(&state, STATE_INIT_WAIT);
			break;

		case STATE_INIT_WAIT:
			processGpsInitialize(&state, STATE_RMCONLY_WAIT);
			break;

		case STATE_RMCONLY_WAIT:
			processSetLogLevel(&state, STATE_PERIOD_WAIT);
			break;

		case STATE_PERIOD_WAIT:
			processSetLoggingFrequency(&state, STATE_VERSION_WAIT);
			break;

		case STATE_VERSION_WAIT:
			processVersionRequestState(&state, STATE_HALT);
			break;

		case STATE_SAVE_NMEA:
			processSaveDataToFlash(&state, STATE_HALT);
			break;

		case STATE_FAILURE:
			setErrLed();
			while(1);
			break;

		case STATE_HALT:
			setDbgLed();
			break;
	}

	// Don't let anything after this run unless a second tick has elapsed
	if (!_secondsTick)
		return;
	_secondsTick = false;
	count++;

	if (state == STATE_HALT && (count % 10) == 0)
	{
	}
}

