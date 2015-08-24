#include "gpslogger.h"


/** Lifecycle and USB flags */
volatile int8_t		_secondsTick		= 0;
volatile int8_t		_hostConnected		= 0;
volatile int32_t	_flashWriteAddr		= 0;


/** NMEA processing data & flags */
static char			_nmea[256];
static bool			_hasFix				= false;



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


/** Configure timers */
void Timer_Initialize(void)
{
	// Configure TC0 for 1 second interrupt
	{
		/* Select clock source. */
		TC0_ConfigClockSource( &TCC0, TC_CLKSEL_DIV1024_gc );

		/* Set period/TOP value. */
		TC_SetPeriod( &TCC0, F_CPU / 1024 );

		/* Setup interrupts for Compare/Capture A */
		TC0_SetCCAIntLevel( &TCC0, TC_CCAINTLVL_LO_gc );

		/* Setup interrupts for Compare/Capture B */
		TCC0.CCB = F_CPU / 1024 * 0.05;
		TC0_SetCCBIntLevel( &TCC0, TC_CCBINTLVL_LO_gc );
	}
}


/** Sends a greeting to the host if the host is connected
void sendGreeting(const char* message)
{
	if (NULL == message)
		return;

	if (!_hostConnected)
		return;

	fputs(message, &USBSerialStream);
}
*/

/** Finds the first free page in the dataflash chip */
int32_t FindFirstFreeAddress(void)
{
	char msg[128];

	uint8_t status = device_is_ready(SPI_CS);
	sprintf_P(msg, PSTR("device is ready: %i\r\n"), status);
	fputs(msg, &USBSerialStream);

	sprintf_P(msg, PSTR("Looking for a free page in flash\r\n"));
	fputs(msg, &USBSerialStream);

	sprintf_P(msg, PSTR("Page count: %lu\r\n"), FLASH_PAGE_COUNT);
	fputs(msg, &USBSerialStream);
	sprintf_P(msg, PSTR("Page size: %lu\r\n"), FLASH_PAGE_SIZE);
	fputs(msg, &USBSerialStream);
	sprintf_P(msg, PSTR("Total size: %lu\r\n"), FLASH_TOTAL_SIZE);
	fputs(msg, &USBSerialStream);

	sprintf_P(msg, PSTR("Checking page:\r\n"));
	fputs(msg, &USBSerialStream);

	char page[256];
	uint32_t address = 0;
	for (int i = 0; i < FLASH_PAGE_COUNT; i++)
	{
		// compute next read address
		address = i * FLASH_PAGE_SIZE;

		sprintf_P(msg, PSTR("% -8lu"), address);
		fputs(msg, &USBSerialStream);

		if (((i+1) % 14) == 0)
		{
			sprintf_P(msg, PSTR("\r\n"));
			fputs(msg, &USBSerialStream);
		}

		// read the specific block of data from the dataflash
		read_block(SPI_CS, address, sizeof(page), page);

		// check if the whole block is initialized (all bytes = 0xFF)
		bool blockIsValid = true;
		for (int j = 0; j < sizeof(page); j++)
		{
			if (0xFF != page[j])
			{
				blockIsValid = false;
				break;
			}
		}

		if (blockIsValid)
		{
			sprintf_P(msg, PSTR("\r\nFound an open page at address %i\r\n"), address);
			fputs(msg, &USBSerialStream);

			// found an empty block, let's start here
			return address;
		}
	}

	sprintf_P(msg, PSTR("\r\nNo free pages found!\r\n"));
	fputs(msg, &USBSerialStream);

	// no wholly initialized blocks available!
	return -1;
}


/** Callback function which is invoked when a complete message is received */
void HandleLatestGpsMessage(void)
{
	// initialize the NMEA buffer before we capture the latest string
	memset(_nmea, 0, sizeof(_nmea));

	// copy the latest message
	int idx		= 0;
	char msg[256];

	// Clone the most recent GPS message
	idx = Adafruit_GPS_CloneLastMessage(_nmea, sizeof(_nmea));
	if (idx < 5)
	{
		// nothing to do
		return;
	}

	// parse specific messages
	if (0 == strncmp(_nmea, "$GPGGA", 6))
	{
		//sprintf_P(msg, PSTR("Found GPGGA message!\r\n"));
		//fputs(msg, &USBSerialStream);

		// location data - save to flash!
		if (!_hasFix)
		{
			// don't log anything unless we have a satellite fix
			return;
		}

		// If the flash write address is less than 0, then no free pages remain
		// don't write the data!
		if (_flashWriteAddr < 0)
		{
			return;
		}

		// TODO:
		// Once we write data to the flash chip, the GPS should be disabled
		//Adafruit_GPS_Suspend();

		//fputs(_nmea, &USBSerialStream);
		//sprintf_P(msg, PSTR("\r\n"));
		//fputs(msg, &USBSerialStream);

		// write latest message to flash
		//write_block(SPI_CS, _flashWriteAddr, sizeof(_nmea), _nmea);
		//_flashWriteAddr += sizeof(_nmea);
	}
	else if (0 == strncmp(_nmea, "$GPRMC", 6))
	{
		sprintf_P(msg, PSTR("********************************\r\nFound GPRMC message!\r\n"));
		fputs(msg, &USBSerialStream);
		fputs(_nmea, &USBSerialStream);
		sprintf_P(msg, PSTR("\r\n"));
		fputs(msg, &USBSerialStream);

		// process fix data - look for 'V' or 'A'
		// with fix:	$GPRMC,072450.000,A,4078.8163,N,10921.9609,W,0.14,355.98,170815,,,A*7A
		// no fix:		$GPRMC,072450.000,V,4078.8163,N,10921.9609,W,0.14,355.98,170815,,,A*7A

		char* p = strchr(_nmea, ',') + 1;

		/*
		float timef = atof(p);
		uint32_t time = timef;
		int hour = time / 10000;
		int minute = (time % 10000) / 100;
		int seconds = (time % 100);
		int milliseconds = fmod(timef, 1.0) * 1000;
		*/

		p = strchr(p, ',') + 1;

		// check for an A (active) character which indicate a valid GPS satellite fix
		_hasFix = ('A' == p[0]);
		if (_hasFix)
			sprintf_P(msg, PSTR("GPS fix active!\r\n\r\n"));
		else
			sprintf_P(msg, PSTR("data is void\r\n\r\n"));
		fputs(msg, &USBSerialStream);
	}
	else if (0 == strncmp(_nmea, "$PMTK", 5))
	{
		sprintf_P(msg, PSTR("Found PMTK message!\r\n"));
		fputs(msg, &USBSerialStream);

		// GPS module status info
		// process any status messages from the GPS

		fputs(_nmea, &USBSerialStream);
		sprintf_P(msg, PSTR("\r\n"));
		fputs(msg, &USBSerialStream);
	}
	else
	{
		/*
		sprintf_P(msg, PSTR("Unrecognized NMEA sentence: "));
		fputs(msg, &USBSerialStream);
		fputs(_nmea, &USBSerialStream);
		sprintf_P(msg, PSTR("\r\n\r\n"));
		fputs(msg, &USBSerialStream);
		*/
	}
}


/** Main program entry point. This routine contains the overall program flow, including initial
 *  setup of all components and the main program loop.
 */
int main(void)
{
	int delay = 25;

	SetupHardware();

	/* Create a regular character stream for the interface so that it can be used with the stdio.h functions */
	CDC_Device_CreateStream(&VirtualSerial_CDC_Interface, &USBSerialStream);

	GlobalInterruptEnable();

	// to receive data from the CDC:
	// CDC_Device_ReceiveByte(&VirtualSerial_CDC_Interface);

	for (;;)
	{
		if (_secondsTick)
		{
			if (delay)
			{
				delay--;

				if (!delay)
				{
					DIAG_LED_port->OUTCLR = DIAG_LED_pin;

					sprintf_P(_nmea, PSTR("Delay complete.\r\n"));
					fputs(_nmea, &USBSerialStream);

					/* Locate the first initialzed page address in the flash storage device */
					_flashWriteAddr = FindFirstFreeAddress();
				}
			}

			_secondsTick = 0;
		}

		if (Adafruit_GPS_MessageAvailable())
		{
			DIAG_LED_port->OUTSET = DIAG_LED_pin;
			HandleLatestGpsMessage();
		}
		else
		{
			DIAG_LED_port->OUTCLR = DIAG_LED_pin;
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

	/* Enable lov, med and high level interrupts in the PMIC */
	PMIC.CTRL = PMIC_LOLVLEN_bm | PMIC_MEDLVLEN_bm | PMIC_HILVLEN_bm;

	/* Hardware Initialization */
	USB_Init();

	// initialize user hardware here
	LED_PORT->OUTCLR = LED_PIN;
	LED_PORT->DIRSET = LED_PIN;

	// initialize debug LED
	DIAG_LED_port->DIRSET = DIAG_LED_pin;
	DIAG_LED_port->OUTSET = DIAG_LED_pin;

	// intialize the GPS module
	GPS_UART_port->DIRSET = GPS_TX_pin;
	GPS_UART_port->DIRCLR = GPS_RX_pin;

	Adafruit_GPS_Enable();
	Adafruit_GPS_Init(GPS_USART, GPS_EN_port, GPS_EN_pin);
	Adafruit_GPS_SendCommand(PSTR(PMTK_SET_NMEA_UPDATE_1HZ));
	//Adafruit_GPS_SendCommand(PSTR(PMTK_SET_NMEA_OUTPUT_RMCONLY));
	Adafruit_GPS_SendCommand(PSTR(PMTK_SET_NMEA_OUTPUT_RMCGGA));
	Adafruit_GPS_Suspend();

	// Initialize the FLASH driver
	FlashDriver_Init(&SPI_DEVICE, SPI_PORT, SPI_CS);

	// Initialize the SPI device for the flash chip
	SPI_Init(&SPI_DEVICE,
		SPI_MODE_MASTER |
		SPI_SPEED_FCPU_DIV_128 |
		SPI_ORDER_MSB_FIRST |
		SPI_SCK_LEAD_RISING |
		SPI_SAMPLE_LEADING
	);

	// Initialize the timer(s)
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
