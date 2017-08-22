#ifndef __HAL_H__
#define __HAL_H__


// GPS
#define GPS_UART_port	PORTE
#define GPS_RX_pin		PIN2_bm
#define GPS_TX_pin		PIN3_bm
#define GPS_UART		USARTE0
#define GPS_EN_port		PORTC
#define GPS_EN_pin		PIN3_bm
#define GPS_FIX_port	PORTD
#define GPS_FIX_pin		PIN5_bm

// Flash device
#define SPI_DEVICE		SPIC
#define SPI_PORT		PORTC
#define SPI_CS			PIN4_bm
#define SPI_MOSI		PIN5_bm
#define SPI_MISO		PIN6_bm
#define SPI_SCK			PIN7_bm

// Built-in LED
#define LED_PORT		PORTE
#define LED_PIN			PIN1_bm

// LED setup
#define ERR_LED_port	PORTB
#define ERR_LED_pin		PIN2_bm
#define DIAG_LED_port	PORTB
#define DIAG_LED_pin	PIN1_bm
#define INFO_LED_port	PORTB
#define INFO_LED_pin	PIN0_bm



//-----------------------------------------------------------------------------
// Configures the pins that the GPS module needs to communicate with the
// microcontroller
static inline void ConfigureGpsPins(void)
{
	// set the GPS enable pin to output
	GPS_EN_port.DIRSET = GPS_EN_pin;

	// setup the FIX pin as input from the GPS
	GPS_FIX_port.DIRCLR = GPS_FIX_pin;

	// intialize the GPS IO pins
	GPS_UART_port.DIRSET = GPS_TX_pin;
	GPS_UART_port.OUTSET = GPS_TX_pin;
	GPS_UART_port.DIRCLR = GPS_RX_pin;
}


//-----------------------------------------------------------------------------
// Configures the USART that the microcontroller uses to communicate with
// the GPS module
static inline void ConfigureGpsUart(void)
{
	// Initialize the GPS USART interface
	Serial_Init(&GPS_UART, 9600, false);

	// enable specific USART interrupts
	GPS_UART.CTRLA =	USART_RXCINTLVL_HI_gc |
						USART_TXCINTLVL_OFF_gc |
						USART_DREINTLVL_OFF_gc;
}


//-----------------------------------------------------------------------------
// Enables the GPS module
static inline void EnableGpsModule(void)
{
	// Setting the EN pin on the GPS module to high enables the GPS
	// module.  The module must cold-start unless the backup battery
	// is installed.
	GPS_EN_port.OUTSET = GPS_EN_pin;
}


//-----------------------------------------------------------------------------
// Disables the GPS module
static inline void DisableGpsModule(void)
{
	// Clears the EN pin.  This forces the GPS module into standby mode.
	// The module must cold-start unless the backup battery
	// is installed.
	GPS_EN_port.OUTCLR = GPS_EN_pin;	
}


//-----------------------------------------------------------------------------
// Send data to the UART
static inline void WriteToGpsUart_P(const char* data)
{
	
	Serial_SendString_P(&GPS_UART, data);
}


//-----------------------------------------------------------------------------
// Halts program execution while the FIX pin is asserted
static inline void WaitForFixLineLow(void)
{
	// wait for FIX line to go low
	while ((GPS_FIX_port.IN & GPS_FIX_pin) != 0)
	{}
}

static inline void ConfigureLeds(void)
{
	LED_PORT.OUTCLR = LED_PIN;
	LED_PORT.DIRSET = LED_PIN;
	ERR_LED_port.DIRSET = ERR_LED_pin;
	DIAG_LED_port.DIRSET = DIAG_LED_pin;
	INFO_LED_port.DIRSET = INFO_LED_pin;
}
static inline void TurnOnBuiltInLed(void)
{
	LED_PORT.OUTSET = LED_PIN;
}
static inline void TurnOffBuiltInLed(void)
{
	LED_PORT.OUTCLR = LED_PIN;
}
static inline void TurnOnInfoLed(void)
{
	INFO_LED_port.OUTSET = INFO_LED_pin;
}
static inline void TurnOffInfoLed(void)
{
	INFO_LED_port.OUTCLR = INFO_LED_pin;
}
static inline void TurnOnDiagLed(void)
{
	DIAG_LED_port.OUTSET = DIAG_LED_pin;
}
static inline void TurnOffDiagLed(void)
{
	DIAG_LED_port.OUTCLR = DIAG_LED_pin;
}
static inline void TurnOnErrLed(void)
{
	ERR_LED_port.OUTSET = ERR_LED_pin;
}
static inline void TurnOffErrLed(void)
{
	ERR_LED_port.OUTCLR = ERR_LED_pin;
}
static inline void TurnOffLeds(void)
{
	TurnOffErrLed();
	TurnOffDiagLed();
	TurnOffInfoLed();
	TurnOffBuiltInLed();
}




#endif
