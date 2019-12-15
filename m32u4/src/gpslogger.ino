//#include <DataFlash.h>

#include <gpslogger.h>
#include <Adafruit_GPS.h>

Adafruit_GPS* gps = NULL;


static char version[24];
static char message[128];
static int messageIdx;

static state_t _state;

// capture the tick flag from the timer file
extern bool _secondsTick;



// ----------------------------------------------------------------------------
// Toggles the LED when the fix pin changes
void toggleFixLed(void)
{
	static bool lastState = false;
	bool currentState = (GPS_PINX & GPS_FIX_bm) != 0;

	if (lastState == currentState)
		return;

	if (currentState)
		setStaLed();
	else
		clearStaLed();

	lastState = currentState;
}


// ----------------------------------------------------------------------------
// Turns on the error LED
void setErrLed(void)
{

	ERR_LED_PORT |= (ERR_LED_bm);
}
void clearErrLed(void)
{

	ERR_LED_PORT &= ~(ERR_LED_bm);
}
void setDbgLed(void)
{

	DBG_LED_PORT |= (DBG_LED_bm);
}
void clearDbgLed(void)
{

	DBG_LED_PORT &= ~(DBG_LED_bm);
}
void setStaLed(void)
{

	STA_LED_PORT |= (STA_LED_bm);
}
void clearStaLed(void)
{

	STA_LED_PORT &= ~(STA_LED_bm);
}
void configureLeds(void)
{
	ERR_LED_DDR |= (ERR_LED_bm);
	DBG_LED_DDR |= (DBG_LED_bm);
	STA_LED_DDR |= (STA_LED_bm);

	clearErrLed();
	clearDbgLed();
	clearStaLed();
}
void configureGpsPins(void)
{
	// Set FIX and PPS pins as input with pull-ups enabled
	GPS_DDR &= ~(GPS_FIX_bm);
	GPS_PORT |= (GPS_FIX_bm);

	// Set EN pin as output and disable the GPS module
	GPS_DDR |= (GPS_EN_bm);
	GPS_PORT &= (GPS_EN_bm);
}


// ----------------------------------------------------------------------------
// 
void setup(void)
{
	_state = STATE_STARTUP;
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

	sei();
}


// ----------------------------------------------------------------------------
// Halts and waits for the GPS Fix line to toggle.  Ensures the module is ready.
void processFixState(state_t* pstate, state_t next)
{
	static state_t state = STATE_FIX_START;
	switch (state)
	{
		// Wait for the GPS Fix pin to be low
		case STATE_FIX_START:
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
				state = STATE_FIX_START;
				*pstate = next;
			}
			break;
	}
}


// ----------------------------------------------------------------------------
// Waits for the Complete and Init OK flags to be set
void processGpsInitialize(state_t* pstate, state_t next)
{
	static state_t state = STATE_INIT_START;
	switch (state)
	{
		case STATE_INIT_START:
			Serial.println(F("Waiting for Init OK and Cold start flags"));
			state = STATE_INIT_WATCH;
			break;
		case STATE_INIT_WATCH:
			if (hasStatus(FLAGS_INIT_OK) && hasStatus(FLAGS_START))
			{
				Serial.println(F("GPS initialization completed."));
				state = STATE_INIT_START;
				*pstate = next;
			}
			break;
	}
}


// ----------------------------------------------------------------------------
// Processes a basic start sequence
void processRMCOnlyState(state_t* pstate, state_t next)
{
	static state_t state = STATE_RMCONLY_START;
	switch (state)
	{
		case STATE_RMCONLY_START:
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
				state = STATE_RMCONLY_START;
				*pstate = next;
			}
			break;
	}
}


// ----------------------------------------------------------------------------
// Processes the logging frequency request
void processLoggingFrequencyState(state_t* pstate, state_t next)
{
	static state_t state = STATE_PERIOD_START;
	switch (state)
	{
		case STATE_PERIOD_START:
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
				state = STATE_PERIOD_START;
				*pstate = next;
			}
			break;
	}
}


// ----------------------------------------------------------------------------
// Processes the logging frequency request
void processVersionRequestState(state_t* pstate, state_t next)
{
	static state_t state = STATE_VERSION_START;
	switch (state)
	{
		case STATE_VERSION_START:
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
// 
void loop(void)
{
	static int count = 0;
	static state_t nextState = STATE_NONE;
	static state_t state = STATE_STARTUP;

	toggleFixLed();

	while (gps->available())
	{
		char data = gps->read();
		processReceivedData(data);
	}

	if (hasMessage())
	{
		processMessage();
		return;
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

		case STATE_STARTUP:
			state = STATE_FIX_WAIT;
			break;

		case STATE_FIX_WAIT:
			processFixState(&state, STATE_INIT_WAIT);
			break;

		case STATE_INIT_WAIT:
			processGpsInitialize(&state, STATE_RMCONLY_WAIT);
			break;

		case STATE_RMCONLY_WAIT:
			processRMCOnlyState(&state, STATE_PERIOD_WAIT);
			break;

		case STATE_PERIOD_WAIT:
			processLoggingFrequencyState(&state, STATE_VERSION_WAIT);
			break;

		case STATE_VERSION_WAIT:
			processVersionRequestState(&state, STATE_HALT);
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

