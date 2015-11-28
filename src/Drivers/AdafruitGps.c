#include "AdafruitGps.h"



/** Globals */
static USART_t *		_usart			= 0;
static PORT_t *			_en_port		= 0;
static uint8_t			_en_pin			= 0;

/** Create the buffer structure and its underlying storage array */
static char				_data[256];
static int				_buffidx		= 0;

/* Flags for GPS state */
static flags_t			_gps_flags		= 0;

/** Message ready callback function */
Adafruit_GPS_OnMessageReceived_t _onMessageReady = NULL;


/** Initialize the Adafruit GPS library */
void Adafruit_GPS_Init(USART_t* const usart, PORT_t* en_port, uint8_t en_pin, bool* char_received)
{
	_usart		= usart;
	_en_port	= en_port;
	_en_pin		= en_pin;

	memset(_data, 0, sizeof(_data) * sizeof(char));
	_buffidx = 0;

	Adafruit_GPS_ResetState();

	// Initialize the GPS USART interface
	Serial_Init(_usart, 9600, false);

	// enable specific USART interrupts
	_usart->CTRLA = USART_RXCINTLVL_HI_gc |
					USART_TXCINTLVL_OFF_gc |
					USART_DREINTLVL_OFF_gc;

	// set the GPS enable pin to output
	_en_port->DIRSET = _en_pin;
	Adafruit_GPS_Enable();

	// setup the FIX pin as input from the GPS
	GPS_FIX_port->DIRCLR = GPS_FIX_pin;
}

/** Clears all internal state data */
void Adafruit_GPS_ResetState(void)
{

	_gps_flags = 0;
}

/**  */
flags_t Adafruit_GPS_CurrentState(void)
{

	return _gps_flags;
}

/** Enable the GPS */
void Adafruit_GPS_Enable(void)
{
	// set the GPS enable pin to low (PNP switch)
	_en_port->OUTCLR = _en_pin;
}

/** Disable the GPS */
void Adafruit_GPS_Disable(void)
{
	// set the GPS enable pin to high (PNP switch)
	_en_port->OUTSET = _en_pin;
}

/** Sends a command string to the GPS device */
static void Adafruit_GPS_SendCommand(const char* command)
{
	Serial_SendString_P(_usart, command);
	Serial_SendString_P(_usart, PSTR("\r\n"));
}

/** Gets called when a message from the GPS module is available */
void Adafruit_GPS_OnMessageReceived(Adafruit_GPS_OnMessageReceived_t handler)
{
	// assign message handler
	_onMessageReady = handler;
}

/** Suspend the GPS to save power */
void Adafruit_GPS_Suspend(void)
{
	// wait for FIX line to go low
	while ((GPS_FIX_port->IN & GPS_FIX_pin) != 0)
	{}

	_gps_flags &= ~ADAFRUIT_GPS_FLAGS_STANDBYOK;

	// issue the PMTK suspend command message
	Adafruit_GPS_SendCommand(PSTR(PMTK_STANDBY));
}

/** Wake the GPS from a previous suspend */
void Adafruit_GPS_Wakeup(void)
{
	_gps_flags &= ~ADAFRUIT_GPS_FLAGS_AWAKE;

	// wakeup the device by sending a single character
	Serial_SendString_P(_usart, PSTR(" "));
}

/**  */
void Adafruit_GPS_ColdStart(void)
{
	_gps_flags &= ~ADAFRUIT_GPS_FLAGS_START;

	// issue the cold start command
	Adafruit_GPS_SendCommand(PSTR(PMTK_CMD_COLD_START));
}

/**  */
void Adafruit_GPS_WarmStart(void)
{
	_gps_flags &= ~ADAFRUIT_GPS_FLAGS_START;

	// issue the cold start command
	Adafruit_GPS_SendCommand(PSTR(PMTK_CMD_WARM_START));
}

/**  */
void Adafruit_GPS_HotStart(void)
{
	_gps_flags &= ~ADAFRUIT_GPS_FLAGS_START;

	// issue the cold start command
	Adafruit_GPS_SendCommand(PSTR(PMTK_CMD_HOT_START));
}

/**  */
void Adafruit_GPS_RMCOnly(void)
{
	_gps_flags &= ~ADAFRUIT_GPS_FLAGS_OUTPUT_CHG;

	// 
	Adafruit_GPS_SendCommand(PSTR(PMTK_SET_NMEA_OUTPUT_RMCONLY));
}

/**  */
void Adafruit_GPS_RMCGGADual(void)
{
	_gps_flags &= ~ADAFRUIT_GPS_FLAGS_OUTPUT_CHG;

	// 
	Adafruit_GPS_SendCommand(PSTR(PMTK_SET_NMEA_OUTPUT_RMCGGA));
}

/**  */
void Adafruit_GPS_AllData(void)
{
	_gps_flags &= ~ADAFRUIT_GPS_FLAGS_OUTPUT_CHG;

	// 
	Adafruit_GPS_SendCommand(PSTR(PMTK_SET_NMEA_OUTPUT_ALLDATA));
}

/**  */
void Adafruit_GPS_OutputOff(void)
{
	_gps_flags &= ~ADAFRUIT_GPS_FLAGS_OUTPUT_CHG;

	// 
	Adafruit_GPS_SendCommand(PSTR(PMTK_SET_NMEA_OUTPUT_OFF));
}

/**  */
void Adafruit_GPS_Update1Hz(void)
{
	_gps_flags &= ~ADAFRUIT_GPS_FLAGS_OUTPUT_CHG;

	// issue the 1Hz update command
	Adafruit_GPS_SendCommand(PSTR(PMTK_SET_NMEA_UPDATE_1HZ));
}

/**  */
void Adafruit_GPS_Update5Hz(void)
{
	_gps_flags &= ~ADAFRUIT_GPS_FLAGS_OUTPUT_CHG;

	// issue the 5Hz update command
	Adafruit_GPS_SendCommand(PSTR(PMTK_SET_NMEA_UPDATE_5HZ));
}

/**  */
void Adafruit_GPS_Update10Hz(void)
{
	_gps_flags &= ~ADAFRUIT_GPS_FLAGS_OUTPUT_CHG;

	// issue the 10Hz update command
	Adafruit_GPS_SendCommand(PSTR(PMTK_SET_NMEA_UPDATE_10HZ));
}

/**  */
void Adafruit_GPS_AskForVersion(void)
{
	_gps_flags &= ~ADAFRUIT_GPS_FLAGS_VERSION;

	// Send the version 
	Adafruit_GPS_SendCommand(PSTR(PMTK_Q_RELEASE));
}

/**  */
static bool ProcessGpsStateInfo(const char* message, int length)
{
	if (NULL == message || length < 2)
	{
		return;
	}

	if (strncmp_P(message, PSTR(PGACK_CONFIG_COMPLETE), 15) == 0)			// 0
	{
		_gps_flags |= ADAFRUIT_GPS_FLAGS_COMPLETE;
		return;
	}
	
	if (strncmp_P(message, PSTR(PGACK_COLD_STARTUP), 13) == 0)				// 1
	{
		_gps_flags |= ADAFRUIT_GPS_FLAGS_START;
		return;
	}

	if (strncmp_P(message, PSTR(PGACK_CHIP_IDENT), 18) == 0)				// 2
	{
		_gps_flags |= ADAFRUIT_GPS_FLAGS_IDENT;
		return;
	}
	
	if (strncmp_P(message, PSTR(PMTK_AWAKE), 15) == 0)						// 3
	{
		_gps_flags |= ADAFRUIT_GPS_FLAGS_AWAKE;
		return;
	}

	if (strncmp_P(message, PSTR(PMTK_SET_NMEA_UPDATE_1HZ_OK), 17) == 0)		// 4
	{
		_gps_flags |= ADAFRUIT_GPS_FLAGS_1HZ_OK;
		return;
	}
	
	if (strncmp_P(message, PSTR(PMTK_STANDBY_SUCCESS), 17) == 0)			// 5
	{
		_gps_flags |= ADAFRUIT_GPS_FLAGS_STANDBYOK;
		return;
	}
	
	if (strncmp_P(message, PSTR(PMTK_VERSION_HEADER), 12) == 0)				// 6
	{
		_gps_flags |= ADAFRUIT_GPS_FLAGS_VERSION;
		return;
	}
	
	if (strncmp_P(message, PSTR(PMTK_SET_NMEA_OUTPUT_OK), 17) == 0)			// 7
	{
		_gps_flags |= ADAFRUIT_GPS_FLAGS_OUTPUT_CHG;
		return;
	}
}

/** Writes a byte of data to the internal buffer */
static void writeData(char data)
{
	if (_buffidx < sizeof(_data) - 1)
	{
		_data[_buffidx++] = data;
	}
	else
	{
		_data[_buffidx] = 0;
	}
}

/** GPS USART Receive interrupt handler */
ISR(USARTE0_RXC_vect)
{
	// capture the received data
	char data = _usart->DATA;

	// check for end-of-message character from the GPS
	if ('\n' == data)
	{
		writeData(data);
		writeData(0);

		// process GPS state data
		ProcessGpsStateInfo(_data, _buffidx);

		if (NULL != _onMessageReady)
			_onMessageReady(_data, _buffidx);

		memset(_data, 0, sizeof(_data));
		_buffidx = 0;
	}
	else
	{
		// check for start-of-message character from the GPS
		if ('$' == data)
		{
			// reset the buffer
			memset(_data, 0, sizeof(_data));
			_buffidx = 0;
		}

		// add the data to the buffer
		writeData(data);
	}
}
