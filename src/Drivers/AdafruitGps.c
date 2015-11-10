#include "AdafruitGps.h"



/** Globals */
static USART_t *		_usart			= 0;
static PORT_t *			_en_port		= 0;
static uint8_t			_en_pin			= 0;

/** Create the buffer structure and its underlying storage array */
static char				_data[256];
static int				_buffidx		= 0;

/* Flags for GPS state */
static uint8_t			_gps_flags		= 0;


/** Message ready callback function */
Adafruit_GPS_OnMessageReceived_t _onMessageReady = NULL;


/** Initialize the Adafruit GPS library */
void Adafruit_GPS_Init(USART_t* const usart, PORT_t* en_port, uint8_t en_pin)
{
	_usart		= usart;
	_en_port	= en_port;
	_en_pin		= en_pin;
	_gps_flags	= 0;

	// Initialize the USART ring buffer with the created storage array
	//RingBuffer_InitBuffer(&Buffer, _data, sizeof(_data));
	memset(_data, 0, sizeof(_data) * sizeof(char));
	_buffidx = 0;

	// Initialize the GPS USART interface
	Serial_Init(_usart, 9600, false);

	// enable specific USART interrupts
	_usart->CTRLA = USART_RXCINTLVL_LO_gc |
					USART_TXCINTLVL_OFF_gc |
					USART_DREINTLVL_OFF_gc;
}

/** Enable the GPS */
void Adafruit_GPS_Enable(void)
{
	// set the GPS enable pin to input (high-z)
	_en_port->DIRCLR = _en_pin;
}

/** Disable the GPS */
void Adafruit_GPS_Disable(void)
{
	// set the GPS enable pin to output (initialized to LOW level)
	_en_port->OUTCLR = _en_pin;
	_en_port->DIRSET = _en_pin;
}

/** Sends a command string to the GPS device */
static void Adafruit_GPS_SendCommand(const char* command)
{
	Serial_SendString_P(_usart, command);
	Serial_SendByte(_usart, '\r');
	Serial_SendByte(_usart, '\n');
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
	// issue the PMTK suspend command message
	Adafruit_GPS_SendCommand(PSTR(PMTK_STANDBY));
}


/** Wakeup the GPS from a previous suspend */
void Adafruit_GPS_Wakeup(void)
{
	// issue the PMTK awake command
	Adafruit_GPS_SendCommand(PSTR(PMTK_AWAKE));
}

/**  */
void Adafruit_GPS_Update1Hz(void)
{
	// issue the 1Hz update command
	Adafruit_GPS_SendCommand(PSTR(PMTK_SET_NMEA_UPDATE_1HZ));
}

/**  */
void Adafruit_GPS_Update5Hz(void)
{
	// issue the 5Hz update command
	Adafruit_GPS_SendCommand(PSTR(PMTK_SET_NMEA_UPDATE_5HZ));
}

/**  */
void Adafruit_GPS_Update10Hz(void)
{
	// issue the 10Hz update command
	Adafruit_GPS_SendCommand(PSTR(PMTK_SET_NMEA_UPDATE_10HZ));
}

/**  */
void Adafruit_GPS_ColdStart(void)
{
	// issue the cold start command
	Adafruit_GPS_SendCommand(PSTR(PMTK_CMD_COLD_START));
}

/**  */
void Adafruit_GPS_WarmStart(void)
{
	// issue the cold start command
	Adafruit_GPS_SendCommand(PSTR(PMTK_CMD_WARM_START));
}

/**  */
void Adafruit_GPS_HotStart(void)
{
	// issue the cold start command
	Adafruit_GPS_SendCommand(PSTR(PMTK_CMD_HOT_START));
}


/**  */
void Adafruit_GPS_RMCOnly(void)
{
	// 
	Adafruit_GPS_SendCommand(PSTR(PMTK_SET_NMEA_OUTPUT_RMCONLY));
}

/**  */
void Adafruit_GPS_RMCGGADual(void)
{
	// 
	Adafruit_GPS_SendCommand(PSTR(PMTK_SET_NMEA_OUTPUT_RMCGGA));
}

/**  */
void Adafruit_GPS_AllData(void)
{
	// 
	Adafruit_GPS_SendCommand(PSTR(PMTK_SET_NMEA_OUTPUT_ALLDATA));
}

/**  */
void Adafruit_GPS_OutputOff(void)
{
	// 
	Adafruit_GPS_SendCommand(PSTR(PMTK_SET_NMEA_OUTPUT_OFF));
}

/**  */
void Adafruit_GPS_AskForVersion(void)
{

	Adafruit_GPS_SendCommand(PSTR(PMTK_Q_RELEASE));
}

/** Simple status mask helper */
static bool Adafruit_GPS_ModuleStatus(uint8_t MASK)
{
	return ((_gps_flags & MASK) != 0)
		? true
		: false;
}

/** Private method: Processes some of the GPS messages */
static bool _Adafruit_GPS_Process_Latest(const char* message, int length)
{
	char cmd_buff[24];

	// check for init
	strlcpy_P(cmd_buff, PSTR(PGACK_INIT_COMPLETE), sizeof(cmd_buff));
	if (0 == strncmp((const char*)_data, cmd_buff, 13))
	{
		_gps_flags |= ADAFRUIT_GPS_FLAGS_INIT;
		return true;
	}

	// check for cold start
	strlcpy_P(cmd_buff, PSTR(PGACK_COLD_STARTUP), sizeof(cmd_buff));
	if (0 == strncmp((const char*)_data, cmd_buff, 13))
	{
		_gps_flags |= ADAFRUIT_GPS_FLAGS_COLD_START;
		return true;
	}

	// check for ident
	strlcpy_P(cmd_buff, PSTR(PGACK_CHIP_IDENT), sizeof(cmd_buff));
	if (0 == strncmp((const char*)_data, cmd_buff, 18))
	{
		_gps_flags |= ADAFRUIT_GPS_FLAGS_IDENT;
		return true;
	}

	// check for standby status
	strlcpy_P(cmd_buff, PSTR(PMTK_STANDBY_SUCCESS), sizeof(cmd_buff));
	if (0 == strncmp((const char*)_data, cmd_buff, 17))
	{
		_gps_flags |= ADAFRUIT_GPS_FLAGS_STANDBYOK;
		return true;
	}

	// check for awake status
	strlcpy_P(cmd_buff, PSTR(PMTK_AWAKE), sizeof(cmd_buff));
	if (0 == strncmp((const char*)_data, cmd_buff, 15))
	{
		_gps_flags |= ADAFRUIT_GPS_FLAGS_AWAKE;
		return true;
	}

	// check for config complete
	strlcpy_P(cmd_buff, PSTR(PGACK_CONFIG_COMPLETE), sizeof(cmd_buff));
	if (0 == strncmp((const char*)_data, cmd_buff, 15))
	{
		_gps_flags |= ADAFRUIT_GPS_FLAGS_READY;
		return true;
	}

	// no intercept done
	return false;
}

/** GPS USART Receive interrupt handler */
ISR(USARTE0_RXC_vect)
{
	// capture the received data
	char data = _usart->DATA;

	// check for end-of-message character from the GPS
	if ('\n' == data)
	{
		// Signal message receipt
		_data[_buffidx] = 0;

		if (NULL != _onMessageReady)
			_onMessageReady(_data, _buffidx);

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

		// don't go past the end of the GPS data buffer
		if (_buffidx < sizeof(_data) - 1)
		{
			// add the data to the buffer
			_data[_buffidx++] = data;
		}
	}
}
