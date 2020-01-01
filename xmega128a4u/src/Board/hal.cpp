#include <Board/hal.h>

extern volatile int8_t _secondsTick;
extern volatile uint16_t _gpsBattery;

/** Configures the CPU */
void ConfigureCPU(void)
{
	/* Start the PLL to multiply the 2MHz RC oscillator to 32MHz and switch the CPU core to run from it */
	XMEGACLK_StartPLL(CLOCK_SRC_INT_RC2MHZ, 2000000, F_CPU);
	XMEGACLK_SetCPUClockSource(CLOCK_SRC_PLL);

	/* Start the 32MHz internal RC oscillator and start the DFLL to increase it to 48MHz using the USB SOF as a reference */
	XMEGACLK_StartInternalOscillator(CLOCK_SRC_INT_RC32MHZ);
	XMEGACLK_StartDFLL(CLOCK_SRC_INT_RC32MHZ, DFLL_REF_INT_USBSOF, F_USB);

	/* Enable low, med, and high level interrupts in the PMIC */
	PMIC.CTRL = PMIC_LOLVLEN_bm | PMIC_MEDLVLEN_bm | PMIC_HILVLEN_bm;
}

/** Configure timers */
void InitializeTimers(void)
{
	// Configure TC0 for 1 second interrupt
	{
		/* Select clock source. */
		TC0_ConfigClockSource( &TCC0, TC_CLKSEL_DIV1024_gc );

		/* Set period/TOP value. */
		TC_SetPeriod( &TCC0, F_CPU / 1024 );

		/* Setup interrupts for Compare/Capture A */
		TC_SetCompareA( &TCC0, 1 );
		TC0_SetCCAIntLevel( &TCC0, TC_CCAINTLVL_LO_gc );

		/* Setup interrupts for Compare/Capture B */
		TC_SetCompareB (&TCC0, USB_DISCONN_DELAY );
		TC0_SetCCBIntLevel( &TCC0, TC_CCBINTLVL_LO_gc );
	}

	// Configure TC1 for 1ms interrupts.  USB interractions
	// will fire from this handler
	{
		/* Select clock source */
		TC1_ConfigClockSource( &TCC1, TC_CLKSEL_DIV256_gc );

		/* F_CPU / 256 / 125 = 1000Hz (or 1ms) */
		TC_SetPeriod( &TCC1, 125 );

		/* Setup interrupt for compare A */
		TC1_SetCCAIntLevel( &TCC1, TC_CCAINTLVL_HI_gc );
	}
}

/** Configure the ADC to read the GPS backup battery voltage */
void InitializeADC(void)
{
	// Configure ADC
	// The ADC reads the GPS backup battery voltage by
	// sampling the output of 2 47k resistors (divide by 2).
	// Leakage current through the voltage divider is
	// I = V/R
	// I = 3.0/(47k*2)
	// I = 31.9uA

	// get the ADCA calibration data from ADCACAL0 and ADCACAL1
	NVM.CMD = NVM_CMD_READ_CALIB_ROW_gc;
	ADCA.CALL = pgm_read_byte(offsetof(NVM_PROD_SIGNATURES_t, ADCACAL0));
	ADCA.CALH = pgm_read_byte(offsetof(NVM_PROD_SIGNATURES_t, ADCACAL1));
	NVM.CMD = 0;

	// disable the ADC
	ADCA.CTRLA = 0;

	// Configure for Freerunning, 12-bit
	ADCA.CTRLB = ADC_CURRLIMIT_HIGH_gc |
				 ADC_RESOLUTION_12BIT_gc;

	// Configure with external Vref @ PORT A0
	ADCA.REFCTRL = ADC_REFSEL_AREFA_gc;

	// Don't use events
	ADCA.EVCTRL = 0;

	// Configure prescaler with CLKpre/512
	ADCA.PRESCALER = ADC_PRESCALER_DIV512_gc;

	// Configure ADC for 1x gain and single-ended input
	ADCA.CH1.CTRL = ADC_CH_GAIN_1X_gc |
					ADC_CH_INPUTMODE_SINGLEENDED_gc;

	// Select the ADC channel
	ADCA.CH1.MUXCTRL = ADC_CH_MUXPOS_PIN1_gc;

	// No interrupts (for now)
	ADCA.CH1.INTCTRL = ADC_CH_INTMODE_COMPLETE_gc |
					   ADC_CH_INTLVL_LO_gc;

	// Enable the ADC
	ADCA.CTRLA = ADC_ENABLE_bm;
}

/** Configure the SPI interface */
void InitializeSPI(void)
{
	// reset the SPI device
	SPI_DEVICE.CTRL = 0;
	SPI_DEVICE.INTCTRL = SPI_INTLVL_OFF_gc;
	SPI_DEVICE.STATUS = 0;

	// set I/O pins
	/*
	SPI_PORT.DIRSET |= (SPI_CS | SPI_MOSI | SPI_SCK);
	SPI_PORT.DIRCLR |= (SPI_MISO);
	SPI_PORT.OUTSET |= (SPI_CS | SPI_MOSI | SPI_MISO | SPI_SCK);

	// Pullups
	SPI_PORT.PIN4CTRL = PORT_OPC_WIREDANDPULL_gc;
	SPI_PORT.PIN5CTRL = PORT_OPC_WIREDANDPULL_gc;
	SPI_PORT.PIN6CTRL = PORT_OPC_PULLUP_gc;
	SPI_PORT.PIN7CTRL = PORT_OPC_WIREDANDPULL_gc;
	*/

	// Initialize the SPI device for the flash chip
	SPI_Init(&SPI_DEVICE,
		SPI_MODE_MASTER |
		SPI_SPEED_FCPU_DIV_2 |
		SPI_ORDER_MSB_FIRST |
		SPI_SCK_LEAD_RISING |
		SPI_SAMPLE_LEADING
	);

	// Initialize the FLASH driver
	FlashDriver_Init(&SPI_DEVICE, &SPI_PORT, SPI_CS);
}

/** Configures the pins that the GPS module needs to communicate with the
microcontroller */
void ConfigureGpsPins(void)
{
	// intialize the GPS IO pins
	GPS_port.OUTSET = (GPS_TX_pin | GPS_RX_pin | GPS_EN_pin | GPS_FIX_pin);

	// enable pullups
	PORTCFG.MPCMASK = (GPS_TX_pin | GPS_RX_pin | GPS_EN_pin);
	GPS_port.PIN2CTRL = PORT_OPC_PULLUP_gc;

	// enable pulldown on FIX pin (active high)
	GPS_port.PIN5CTRL = PORT_OPC_PULLDOWN_gc;

	// set direction bits
	GPS_port.DIRSET = (GPS_RX_pin | GPS_EN_pin);
	GPS_port.DIRCLR = (GPS_FIX_pin | GPS_TX_pin);

	// configure the ADC pins (battery input and AREF)
	GPS_BAT_port.DIRCLR = GPS_BAT_pin | PIN0_bm;
}

/** Configures the USART that the microcontroller uses to communicate with
the GPS module */
void ConfigureGpsUart(void)
{
	// Initialize the GPS USART interface
	Serial_Init(&GPS_UART, 9600, false);

	// enable specific USART interrupts
	GPS_UART.CTRLA =	USART_RXCINTLVL_HI_gc |
						USART_TXCINTLVL_OFF_gc |
						USART_DREINTLVL_OFF_gc;
}

/** Enables the GPS module */
void EnableGpsModule(void)
{
	// Setting the EN pin on the GPS module to high enables the GPS
	// module.  The module must cold-start unless the backup battery
	// is installed.
	GPS_port.OUTSET = GPS_EN_pin;
}

/** Disables the GPS module */
void DisableGpsModule(void)
{
	// Clears the EN pin.  This forces the GPS module into standby mode.
	// The module must cold-start unless the backup battery
	// is installed.
	GPS_port.OUTCLR = GPS_EN_pin;
}

/** Send data to the UART */
void WriteToGpsUart_P(const char* data)
{
	// Send a string from flash
	Serial_SendString_P(&GPS_UART, data);
}

/** Halts program execution while the FIX pin is asserted */
void WaitForFixLineLow(void)
{
	// wait for FIX line to go low
	while ((GPS_port.IN & GPS_FIX_pin) != 0);
}

/** Halts program execution while the FIX pin is not asserted */
void WaitForFixLineHigh(void)
{
	// wait for FIX line to go low
	while ((GPS_port.IN & GPS_FIX_pin) == 0);
}

/** Configures the LEDs status, diagnostics, debugging, etc. */
void ConfigureLeds(void)
{
	LED_PORT.OUTCLR = LED_PIN;
	LED_PORT.DIRSET = LED_PIN;

	INFO_LED_port.OUTCLR = LED_PIN;
	INFO_LED_port.DIRSET = INFO_LED_pin;

	DIAG_LED_port.OUTCLR = LED_PIN;
	DIAG_LED_port.DIRSET = DIAG_LED_pin;

	ERR_LED_port.OUTCLR = LED_PIN;
	ERR_LED_port.DIRSET = ERR_LED_pin;
}
void TurnOnBuiltInLed(void)
{
	LED_PORT.OUTSET = LED_PIN;
}
void TurnOffBuiltInLed(void)
{
	LED_PORT.OUTCLR = LED_PIN;
}
void TurnOnInfoLed(void)
{
	INFO_LED_port.OUTSET = INFO_LED_pin;
}
void TurnOffInfoLed(void)
{
	INFO_LED_port.OUTCLR = INFO_LED_pin;
}
void TurnOnDiagLed(void)
{
	DIAG_LED_port.OUTSET = DIAG_LED_pin;
}
void TurnOffDiagLed(void)
{
	DIAG_LED_port.OUTCLR = DIAG_LED_pin;
}
void TurnOnErrLed(void)
{
	ERR_LED_port.OUTSET = ERR_LED_pin;
}
void TurnOffErrLed(void)
{
	ERR_LED_port.OUTCLR = ERR_LED_pin;
}
void TurnOffLeds(void)
{
	TurnOffErrLed();
	TurnOffDiagLed();
	TurnOffInfoLed();
	TurnOffBuiltInLed();
}


/** Interrupt handler for TCC0 Compare/Capture A */
ISR(TCC0_CCA_vect)
{
	_secondsTick = 1;

	// start an ADC conversion to read the GPS backup battery
	ADCA.CTRLA = ADC_ENABLE_bm | ADC_CH1START_bm;

	// Turn the LED on
	if (USB_IsHostConnected())
	{
		TurnOnBuiltInLed();
	}
	else
	{
		TurnOffBuiltInLed();
	}

	// clear interrupt flag
	TCC0.INTFLAGS = TC0_CCAIF_bm;
}

/** Interrupt handler for TCC0 Compare/Capture B */
ISR(TCC0_CCB_vect)
{
	// Turn the LED off
	TurnOffBuiltInLed();

	// clear interrupt flag
	TCC0.INTFLAGS = TC0_CCBIF_bm;
}

/** Interrupt handler for TCC1 Compare/Capture A */
ISR(USB_TASK_ISR)
{
	// USB tasks
	USB_DoTasks();

	// clear interrupt flag
	TCC1.INTFLAGS = TC1_CCAIF_bm;
}

/** GPS USART Receive interrupt handler */
ISR(GPS_UART_ISR)
{
	char data = GPS_UART.DATA;

	HandleGpsSerialInterrupt(data);

	GPS_UART.STATUS = USART_RXCIF_bm;
}

/** ADC Channel 1 interrupt handler */
ISR(ADCA_CH1_vect)
{
	// read the battery value
	_gpsBattery = ADCA.CH1.RES;
}

