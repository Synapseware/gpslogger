#include "gpslogger.h"


volatile int8_t _secondsTick = 0;
volatile int8_t _hostConnected = 0;


/** LUFA CDC Class driver interface configuration and state information. This structure is
 *  passed to all CDC Class driver functions, so that multiple instances of the same class
 *  within a device can be differentiated from one another.
 */
USB_ClassInfo_CDC_Device_t VirtualSerial_CDC_Interface =
	{
		.Config =
			{
				.ControlInterfaceNumber   = INTERFACE_ID_CDC_CCI,
				.DataINEndpoint           =
					{
						.Address          = CDC_TX_EPADDR,
						.Size             = CDC_TXRX_EPSIZE,
						.Banks            = 1,
					},
				.DataOUTEndpoint =
					{
						.Address          = CDC_RX_EPADDR,
						.Size             = CDC_TXRX_EPSIZE,
						.Banks            = 1,
					},
				.NotificationEndpoint =
					{
						.Address          = CDC_NOTIFICATION_EPADDR,
						.Size             = CDC_NOTIFICATION_EPSIZE,
						.Banks            = 1,
					},
			},
	};

/** Standard file stream for the CDC interface when set up, so that the virtual CDC COM port can be
 *  used like any regular character stream in the C APIs.
 */
static FILE USBSerialStream;

/** Configure timer for 1 second delay with interrupts */
void Timer_Initialize(void)
{
	/* Select clock source. */
	TC0_ConfigClockSource( &TCC0, TC_CLKSEL_DIV1024_gc );

	/* Set period/TOP value. */
	TC_SetPeriod( &TCC0, F_CPU / 1024 );

	/* Setup interrupts for Compare/Capture A */
	TC0_SetCCAIntLevel( &TCC0, TC_CCAINTLVL_LO_gc );

	/* Setup interrupts for Compare/Capture A */
	TCC0.CCB = F_CPU / 1024 * 0.05;
	TC0_SetCCBIntLevel( &TCC0, TC_CCBINTLVL_LO_gc );}

/** Sends a greeting to the host if the host is connected */
void sendGreeting(void)
{
	static int8_t sent = 0;
	if (!_hostConnected || sent)
		return;

	char * msg = "Hello.\r\n";
	fputs(msg, &USBSerialStream);
	sent = 1;	
}

/** Main program entry point. This routine contains the overall program flow, including initial
 *  setup of all components and the main program loop.
 */
int main(void)
{
	SetupHardware();

	/* Create a regular character stream for the interface so that it can be used with the stdio.h functions */
	CDC_Device_CreateStream(&VirtualSerial_CDC_Interface, &USBSerialStream);

	GlobalInterruptEnable();

	// to receive data from the CDC:
	// CDC_Device_ReceiveByte(&VirtualSerial_CDC_Interface);
	char msg[257];
	msg[sizeof(msg)-1] = 0;

	for (;;)
	{
		if (_secondsTick)
		{
			sendGreeting();
			_secondsTick = 0;
		}

		if (Adafruit_GPS_MessageAvailable())
		{
			uint8_t idx = 0;
			int data = 0;
			while (0 != (data = Adafruit_GPS_Read()))
			{
				msg[idx++] = data;
			}
			msg[idx++] = '\r';
			msg[idx++] = '\n';
			msg[idx++] = 0;

			// write the latest GPS data to the host
			fputs(msg, &USBSerialStream);
		}

		/* Must throw away unused bytes from the host, or it will lock up while waiting for the device */
		CDC_Device_ReceiveByte(&VirtualSerial_CDC_Interface);

		/* Required calls */		
		CDC_Device_USBTask(&VirtualSerial_CDC_Interface);
		USB_USBTask();
	}
}

/** Configures the board hardware and chip peripherals for the application's functionality. */
void SetupHardware(void)
{
	/* Start the PLL to multiply the 2MHz RC oscillator to 32MHz and switch the CPU core to run from it */
	XMEGACLK_StartPLL(CLOCK_SRC_INT_RC2MHZ, 2000000, F_CPU);
	XMEGACLK_SetCPUClockSource(CLOCK_SRC_PLL);

	/* Start the 32MHz internal RC oscillator and start the DFLL to increase it to 48MHz using the USB SOF as a reference */
	XMEGACLK_StartInternalOscillator(CLOCK_SRC_INT_RC32MHZ);
	XMEGACLK_StartDFLL(CLOCK_SRC_INT_RC32MHZ, DFLL_REF_INT_USBSOF, F_USB);

	PMIC.CTRL = PMIC_LOLVLEN_bm | PMIC_MEDLVLEN_bm | PMIC_HILVLEN_bm;

	/* Hardware Initialization */
	USB_Init();

	// initialize user hardware here
	LED_PORT->OUTCLR = LED_PIN;
	LED_PORT->DIRSET = LED_PIN;

	// intialize the GPS module
	Adafruit_GPS_Init(GPS_USART, GPS_EN_port, GPS_EN_pin);
	Adafruit_GPS_SendCommand(PSTR(PMTK_SET_NMEA_UPDATE_1HZ));
	Adafruit_GPS_SendCommand(PSTR(PMTK_SET_NMEA_OUTPUT_RMCGGA));
	
	Adafruit_GPS_DisableGPS();


	// Initialize the SPI device for the flash chip
	SPI_Init(&SPI_DEVICE,
		SPI_SPEED_FCPU_DIV_2 |
		SPI_ORDER_MSB_FIRST |
		SPI_SCK_LEAD_FALLING |
		SPI_SAMPLE_TRAILING |
		SPI_MODE_MASTER
	);

	Initialize_FlashDriver(&SPI_DEVICE, SPI_PORT, SPI_CS);

	Timer_Initialize();
}

/** Event handler for the library USB Connection event. */
void EVENT_USB_Device_Connect(void)
{
	//LEDs_SetAllLEDs(LEDMASK_USB_ENUMERATING);
	//(LED_PORT)->OUTSET = LED_PIN;
}

/** Event handler for the library USB Disconnection event. */
void EVENT_USB_Device_Disconnect(void)
{
	//LEDs_SetAllLEDs(LEDMASK_USB_NOTREADY);
	(LED_PORT)->OUTCLR = LED_PIN;
}

/** Event handler for the library USB Configuration Changed event. */
void EVENT_USB_Device_ConfigurationChanged(void)
{
	bool ConfigSuccess = true;

	ConfigSuccess &= CDC_Device_ConfigureEndpoints(&VirtualSerial_CDC_Interface);

	LEDs_SetAllLEDs(ConfigSuccess ? LEDMASK_USB_READY : LEDMASK_USB_ERROR);

	if (ConfigSuccess)
		(LED_PORT)->OUTSET = LED_PIN;
}

/** Event handler for the library USB Control Request reception event. */
void EVENT_USB_Device_ControlRequest(void)
{
	CDC_Device_ProcessControlRequest(&VirtualSerial_CDC_Interface);
}

/** Event handler for the library USB Control Line State Change event */
void EVENT_CDC_Device_ControLineStateChanged(USB_ClassInfo_CDC_Device_t *const CDCInterfaceInfo)
{
	if (CDCInterfaceInfo->State.ControlLineStates.HostToDevice & CDC_CONTROL_LINE_OUT_DTR)
	{
		_hostConnected = 1;
	}
	else
	{
		_hostConnected = 0;
	}
}

/** Interrupt handler for TCC0 Compare/Capture A */
ISR(TCC0_CCA_vect)
{
	_secondsTick = 1;

	// Turn the LED on
	if (_hostConnected)
		(LED_PORT)->OUTSET = LED_PIN;
}

/** Interrupt handler for TCC0 Compare/Capture B */
ISR(TCC0_CCB_vect)
{
	// Turn the LED off
	(LED_PORT)->OUTCLR = LED_PIN;
}
