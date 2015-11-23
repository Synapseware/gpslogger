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
static bool				_char_in		= false;

/** Message ready callback function */
Adafruit_GPS_OnMessageReceived_t _onMessageReady = NULL;


/** Initialize the Adafruit GPS library */
void Adafruit_GPS_Init(USART_t* const usart, PORT_t* en_port, uint8_t en_pin, bool* char_received)
{
	_usart		= usart;
	_en_port	= en_port;
	_en_pin		= en_pin;
	_gps_flags	= 0;
	_char_in	= char_received;

	// Initialize the USART ring buffer with the created storage array
	//RingBuffer_InitBuffer(&Buffer, _data, sizeof(_data));
	memset(_data, 0, sizeof(_data) * sizeof(char));
	_buffidx = 0;

	// Initialize the GPS USART interface
	Serial_Init(_usart, 9600, false);

	// enable specific USART interrupts
	_usart->CTRLA = USART_RXCINTLVL_HI_gc |
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
	Serial_SendString_P(_usart, PSTR("\r\n"));
}

/** Gets called when a message from the GPS module is available */
void Adafruit_GPS_OnMessageReceived(Adafruit_GPS_OnMessageReceived_t handler)
{
	// assign message handler
	_onMessageReady = handler;
}

/** Gets called when a message is received */
static bool _messageReceived = false;
static char* _messageToWaitFor = NULL;
void Adafruit_GPS_WaitForMessageCallback(const char* message, uint16_t length)
{
	_messageReceived = false;

	// check for receipt of message of interest
	if (0 == strcmp_P(message, _messageToWaitFor))
	{
		_messageReceived = true;
	}
}

/** Waits for specific message from the GPS before returning */
void Adafruit_GPS_WaitForMessage(const char* msg)
{
	_messageToWaitFor = msg;
	Adafruit_GPS_OnMessageReceived_t handler = _onMessageReady;
	_onMessageReady = Adafruit_GPS_WaitForMessageCallback;
	
	// wait for message
	while(!_messageReceived);

	// restore message handler
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
	// wakeup the device by sending a single character
	Serial_SendString_P(_usart, PSTR(" "));
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
	// Send the version 
	Adafruit_GPS_SendCommand(PSTR(PMTK_Q_RELEASE));
}

/** Simple status mask helper */
static bool Adafruit_GPS_ModuleStatus(uint8_t MASK)
{
	return ((_gps_flags & MASK) != 0)
		? true
		: false;
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

	_char_in = true;

	// check for end-of-message character from the GPS
	if ('\n' == data)
	{
		writeData(data);
		writeData(0);

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
