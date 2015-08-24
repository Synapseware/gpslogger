#include "AdafruitGps.h"



/** Globals */
USART_t	*		_usart			= 0;
PORT_t	*		_en_port		= 0;
uint8_t			_en_pin			= 0;
int8_t			_message_rdy	= 0;

/** Create the buffer structure and its underlying storage array */
uint8_t			_data[256];
int				_buffidx		= 0;


/** Message ready callback function */
Adafruit_GPS_OnMessageReceived_t _onMessageReady = NULL;



/** Initialize the Adafruit GPS library */
void Adafruit_GPS_Init(USART_t* const usart, PORT_t* en_port, uint8_t en_pin)
{
	_usart		= usart;
	_en_port	= en_port;
	_en_pin		= en_pin;

	// Initialize the USART ring buffer with the created storage array
	//RingBuffer_InitBuffer(&Buffer, _data, sizeof(_data));
	memset(_data, 0, sizeof(_data) * sizeof(char));
	_buffidx = 0;

	// Initialize the GPS USART interface
	Serial_Init(_usart, 9600, false);

	// enable specific USART interrupts
	_usart->CTRLA = USART_RXCINTLVL_MED_gc |
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
void Adafruit_GPS_SendCommand(const char* command)
{
	Serial_SendString_P(_usart, command);
	Serial_SendByte(_usart, '\n');
}


/** Gets called when a message from the GPS module is available */
void Adafruit_GPS_OnMessageReceived(Adafruit_GPS_OnMessageReceived_t handler)
{
	// assign message handler
	_onMessageReady = handler;
}


/** Returns 1 if a message is available, otherwise returns 0 */
int8_t Adafruit_GPS_MessageAvailable(void)
{
	// return message avail flag
	return _message_rdy;
}


/** Clone's the latest message received by copying it to the given buffer */
int Adafruit_GPS_CloneLastMessage(char* destination, int destSize)
{
	strncpy(destination, _data, destSize);
	_message_rdy = 0;
	return _buffidx;
}


/** Suspend the the GPS to save power */
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


/** GPS USART Receive interrupt handler */
ISR(USARTE0_RXC_vect)
{
	// capture the received data
	char data = _usart->DATA;

	// don't process any data if the message ready flag is set
	if (_message_rdy)
	{
		return;
	}

	// check for end-of-message character from the GPS
	if ('\n' == data)
	{
		_message_rdy = 1;
		_data[_buffidx] = 0;

		if (NULL != _onMessageReady)
			_onMessageReady();

		return;
	}

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
