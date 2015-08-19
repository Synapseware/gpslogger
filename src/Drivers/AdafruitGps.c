#include "AdafruitGps.h"


/** Globals */
USART_t	*	_usart			= 0;
PORT_t	*	_en_port		= 0;
uint8_t		_en_pin			= 0;
int8_t		_message_rdy	= 0;

/** Create the buffer structure and its underlying storage array */
RingBuffer_t Buffer;
uint8_t      BufferData[256];


/** Initialize the Adafruit GPS library */
void Adafruit_GPS_Init(USART_t* const usart, PORT_t* en_port, uint8_t en_pin)
{
	_usart		= usart;
	_en_port	= en_port;
	_en_pin		= en_pin;

	// Initialize the USART ring buffer with the created storage array
	RingBuffer_InitBuffer(&Buffer, BufferData, sizeof(BufferData));

	// Initialize the GPS USART interface
	Serial_Init(_usart, 9600, false);

	// enable specific USART interrupts
	_usart->CTRLA = USART_RXCINTLVL_MED_gc |
					  USART_TXCINTLVL_OFF_gc |
					  USART_DREINTLVL_OFF_gc;
}


/** Enable the GPS */
void Adafruit_GPS_EnableGPS(void)
{
	// set the GPS enable pin to input (high-z)
	_en_port->DIRCLR = _en_pin;
}


/** Disable the GPS */
void Adafruit_GPS_DisableGPS(void)
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


/** Reads a string of data from the GPS device */
uint16_t Adafruit_GPS_BufferSize(void)
{
	return RingBuffer_GetCount(&Buffer);
}


/** Gets called when a message from the GPS module is available */
void Adafruit_GPS_OnMessageReceived(Adafruit_GPS_OnMessageReceived_t handler)
{

}


/** Returns 1 if a message is available, otherwise returns 0 */
int8_t Adafruit_GPS_MessageAvailable(void)
{
	return _message_rdy;
}


/** Returns data from the receive buffer */
int Adafruit_GPS_Read(void)
{
	if (RingBuffer_IsEmpty(&Buffer))
	{
		return -1;
	}

	// read a byte from the buffer
	uint8_t data = RingBuffer_Remove(&Buffer);

	// signal buffer empty/done
	if (RingBuffer_IsEmpty(&Buffer))
	{
		_message_rdy = 0;
	}

	return data;
}


/** Looks into the NMEA sentence for fix data */
char Adafruit_GPS_HasFix(const char* nmea)
{
	if (NULL == nmea || nmea != strstr(nmea, "$GPRMC"))
	{
		return '\0';
	}

	// look for the A or V in
	// "$GPRMC,072450.000,A,4078.8163,N,10921.9609,W,0.14,355.98,170815,,,A*7A"
	strtok(nmea, ',')

	return 'V';
}


/** GPS USART Receive interrupt handler */
ISR(USARTE0_RXC_vect)
{
	uint8_t data = _usart->DATA;

	if ('\n' == data)
	{
		_message_rdy = 1;
		RingBuffer_Insert(&Buffer, 0);
	}
	else
	{
		RingBuffer_Insert(&Buffer, data);
	}
}

