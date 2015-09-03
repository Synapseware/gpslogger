#include "gpslogger.h"


/** Lifecycle and USB flags */
volatile int8_t		_secondsTick		= 0;
volatile int8_t		_hostConnected		= 0;
volatile int32_t	_flashWriteAddr		= 0;


/** NMEA processing data & flags */
static bool			_hasFix				= false;
static bool			_saveNmea			= false;
static char			_nmea[256];
static char			_gpsVersion[96];



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

	// Configure TC1 for 20ms interrupts.  USB interractions
	// will fire from this handler
	{
		/* Select clock source */
		TC1_ConfigClockSource( &TCC1, TC_CLKSEL_DIV256_gc );

		/* F_CPU / 256 / 1250 =  */
		TC_SetPeriod( &TCC1, F_CPU / 1250 );

		/* Setup interrupt for compare A */
		TC1_SetCCAIntLevel( &TCC1, TC_CCAINTLVL_HI_gc );
	}
}


/** Sends a greeting to the host if the host is connected */
void sendGreeting(const char* message)
{
	if (NULL == message)
		return;

	if (!_hostConnected)
		return;

	fputs(message, &USBSerialStream);
}


/** Finds the first free page in the dataflash chip */
int32_t FindFirstFreeAddress(void)
{
	char msg[128];

	uint8_t status = device_is_ready(SPI_CS);

	sprintf_P(msg, PSTR("  Page count: %lu\r\n"), FLASH_PAGE_COUNT);
	fputs(msg, &USBSerialStream);
	sprintf_P(msg, PSTR("  Page size: %lu\r\n"), FLASH_PAGE_SIZE);
	fputs(msg, &USBSerialStream);
	sprintf_P(msg, PSTR("  Total size: %lu\r\n"), FLASH_TOTAL_SIZE);
	fputs(msg, &USBSerialStream);

	if (!status)
	{
		sprintf_P(msg, PSTR("  Flash is not ready!\r\n"));
		fputs(msg, &USBSerialStream);
		return -1;
	}

	sprintf_P(msg, PSTR("  Flash is ready: %i\r\n"), status);
	fputs(msg, &USBSerialStream);

	sprintf_P(msg, PSTR("  Searching for free start address...\r\n"));
	fputs(msg, &USBSerialStream);

	int32_t address = 0;
	int32_t freefrom = -1;

	begin_read(SPI_CS, address);
	while (address < FLASH_TOTAL_SIZE)
	{
		// look for the first 0xFF followed by 0xFF's to the end
		char data = read_byte();
		address++;

		if (0xFF == data)
		{
			if (freefrom < 0)
				freefrom = address - 1;
		}
		else
		{
			freefrom = -1;
		}
	}
	end_read(SPI_CS);

	if (freefrom > -1)
	{
		sprintf_P(msg, PSTR("  First open address: %u\r\n"), freefrom);
	}
	else
	{
		sprintf_P(msg, PSTR("  No free pages found!\r\n"));
	}
	fputs(msg, &USBSerialStream);

	return freefrom;
}

/** Saves the latest NMEA logging data to the flash */
bool SaveNMEA(void)
{
	_saveNmea = false;
	char msg[64];

	// make sure our string is NULL terminated
	size_t sentenceLength = strlen(_nmea);
	if (_flashWriteAddr < 0 || sentenceLength >= sizeof(_nmea))
	{
		sprintf_P(msg, PSTR("Invalid logging message!\r\n"));
		fputs(msg, &USBSerialStream);
		return false;
	}

	sprintf_P(msg, PSTR("Saving NMEA sentence: "));
	fputs(msg, &USBSerialStream);
	fputs(_nmea, &USBSerialStream);
	sprintf_P(msg, PSTR("\r\n"));
	fputs(msg, &USBSerialStream);

	// TODO:
	// Once we write data to the flash chip, the GPS should be disabled
	sprintf_P(msg, PSTR("Suspending GPS module.\r\n"));
	fputs(msg, &USBSerialStream);
	Adafruit_GPS_Suspend();

	sprintf_P(msg, PSTR("Saving sentence to FLASH.\r\n"));
	fputs(msg, &USBSerialStream);

	// write latest message to flash
	int idx = 0;
	begin_write(SPI_CS, _flashWriteAddr);
	while (sentenceLength && _nmea[idx])
	{
		if ((_flashWriteAddr % FLASH_PAGE_SIZE) == 0)
		{
			end_write(SPI_CS);

			// wait for write to complete
			while(!device_is_ready(SPI_CS));
		}

		write_byte(_nmea[idx++]);
		_flashWriteAddr++;
	}
	end_write(SPI_CS);

	//write_enable(SPI_CS);
	//write_block(SPI_CS, _flashWriteAddr, sentenceLength, _nmea);
	//write_disable(SPI_CS);

	_flashWriteAddr += strlen(_nmea);

	sprintf_P(msg, PSTR("Data saved.\r\n\r\n"));
	fputs(msg, &USBSerialStream);

	return true;
}

/** Callback function which is invoked when a complete message is received
This method is run from the UART RX interrupt */
void HandleLatestGpsMessage(const char* message, uint16_t length)
{
	char cmd_buff[16];

	// make sure we have a valid sentence
	if (length < 5 || NULL == message)
	{
		// nothing to do
		ERR_LED_port->OUTSET = ERR_LED_pin;
		return;
	}

	// clear the error LED
	ERR_LED_port->OUTCLR = ERR_LED_pin;

	// signal GPS message receipt
	DIAG_LED_port->OUTSET = DIAG_LED_pin;

	// parse GPRMC - recommended minimum navigation data
	strlcpy_P(cmd_buff, PSTR(PMTK_GPRMC_HEADER), sizeof(cmd_buff));
	if (0 == strncmp(message, cmd_buff, 6))
	{
		// process fix data - look for 'V' or 'A'
		// with fix:	$GPRMC,072450.000,A,4078.8163,N,10921.9609,W,0.14,355.98,170815,,,A*7A
		// no fix:		$GPRMC,072450.000,V,4078.8163,N,10921.9609,W,0.14,355.98,170815,,,A*7A

		char* p = strchr(message, ',') + 1;

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

		// location data - save to flash!
		if (_hasFix)
		{
			// reset the flash buffer
			memset(_nmea, 0, sizeof(_nmea));

			// copy the NMEA sentence to the flash buffer
			strncpy(_nmea, message, sizeof(_nmea));

			_saveNmea = true;
		}

		DIAG_LED_port->OUTCLR = DIAG_LED_pin;
		return;
	}

	// Parse PMTK
	strlcpy_P(cmd_buff, PSTR(PMTK_HEADER), sizeof(cmd_buff));
	if (0 == strncmp(message, cmd_buff, 5))
	{
		strlcpy_P(cmd_buff, PSTR(PMTK_AWAKE), sizeof(cmd_buff));
		if (0 == strncmp(message, cmd_buff, 15))
		{
			// GPS is awake
		}

		strlcpy_P(cmd_buff, PSTR(PMTK_VERSION_HEADER), sizeof(cmd_buff));
		if (0 == strncmp(message, cmd_buff, 9))
		{
			strncpy(_gpsVersion, message, sizeof(_gpsVersion));
		}

		DIAG_LED_port->OUTCLR = DIAG_LED_pin;
		return;
	}
}

/** Main program entry point. This routine contains the overall program flow, including initial
 *  setup of all components and the main program loop.
 */
int main(void)
{
	int delay = 3;
	_saveNmea = false;
	memset(_nmea, 0, sizeof(_nmea));
	memset(_gpsVersion, 0, sizeof(_gpsVersion));

	SetupHardware();

	/* Create a regular character stream for the interface so that it can be used with the stdio.h functions */
	CDC_Device_CreateStream(&VirtualSerial_CDC_Interface, &USBSerialStream);

	GlobalInterruptEnable();

	// reset the first block
	erase_block_4k(SPI_CS, 0);

	for (;;)
	{
		if (_saveNmea)
		{
			SaveNMEA();
		}

		if (!_hostConnected)
		{
			delay = 3;
		}

		if (_secondsTick && _hostConnected)
		{
			if (delay)
			{
				delay--;

				if (!delay)
				{
					char msg[64];
					sprintf_P(msg, PSTR("GPS Logger v1.1 - 20150801\r\n"));
					fputs(msg, &USBSerialStream);

					if (0 != strlen(_gpsVersion))
					{
						sprintf_P(msg, PSTR("GPS Version: %s\r\n"), _gpsVersion);
					}
					else
					{
						sprintf_P(msg, PSTR("GPS Version: (unknown)\r\n"));
					}
					fputs(msg, &USBSerialStream);

					sprintf_P(msg, PSTR("Flash is Valid: %c\r\n"), is_valid(SPI_CS) != 0 ? 'Y' : 'N');
					fputs(msg, &USBSerialStream);

					_flashWriteAddr = FindFirstFreeAddress();
					sprintf_P(msg, PSTR("Flash start: %lu\r\n"), _flashWriteAddr);
					fputs(msg, &USBSerialStream);
				}
			}

			_secondsTick = 0;
		}
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
	DIAG_LED_port->OUTCLR = DIAG_LED_pin;
	DIAG_LED_port->DIRSET = DIAG_LED_pin;

	// initialize error LED
	ERR_LED_port->OUTCLR = ERR_LED_pin;
	ERR_LED_port->DIRSET = ERR_LED_pin;

	// intialize the GPS module
	GPS_UART_port->DIRSET = GPS_TX_pin;
	GPS_UART_port->DIRCLR = GPS_RX_pin;

	Adafruit_GPS_OnMessageReceived(&HandleLatestGpsMessage);
	Adafruit_GPS_Enable();
	Adafruit_GPS_Init(GPS_USART, GPS_EN_port, GPS_EN_pin);
	Adafruit_GPS_Update1Hz();
	Adafruit_GPS_RMCOnly();
	Adafruit_GPS_AskForVersion();

	// Initialize the SPI device for the flash chip
	SPI_Init(&SPI_DEVICE,
		SPI_MODE_MASTER |
		SPI_SPEED_FCPU_DIV_2 |
		SPI_ORDER_MSB_FIRST |
		SPI_SCK_LEAD_RISING |
		SPI_SAMPLE_LEADING
	);

	// Initialize the FLASH driver
	FlashDriver_Init(&SPI_DEVICE, SPI_PORT, SPI_CS);
	global_unprotect(SPI_CS);

	// Initialize the timer(s)
	Timer_Initialize();
}

/** Event handler for the library USB Connection event. */
void EVENT_USB_Device_Connect(void)
{
	// turn on the diag LED
	LED_PORT->OUTSET = LED_PIN;
}

/** Event handler for the library USB Disconnection event. */
void EVENT_USB_Device_Disconnect(void)
{
	// turn off the diag LED
	LED_PORT->OUTCLR = LED_PIN;

	_hostConnected = 0;
}

/** Event handler for the library USB Configuration Changed event. */
void EVENT_USB_Device_ConfigurationChanged(void)
{
	bool success = CDC_Device_ConfigureEndpoints(&VirtualSerial_CDC_Interface);
	if (success)
	{
	}
}

/** Event handler for the library USB Control Request reception event. */
void EVENT_USB_Device_ControlRequest(void)
{
	// pass the config data to the processor
	CDC_Device_ProcessControlRequest(&VirtualSerial_CDC_Interface);
}

/** Event handler for the library USB Control Line State Change event */
void EVENT_CDC_Device_ControLineStateChanged(USB_ClassInfo_CDC_Device_t *const CDCInterfaceInfo)
{
	bool CurrentDTRState = (CDCInterfaceInfo->State.ControlLineStates.HostToDevice & CDC_CONTROL_LINE_OUT_DTR);
	if (CurrentDTRState)
	{
		_hostConnected = 1;
	}
	else
	{
		_hostConnected = 0;
	}
}

/** Process control line signals */
void CDC_ProcessControlStateChange(void)
{
	/* Device must be connected and configured for the task to run */
	if (USB_DeviceState != DEVICE_STATE_Configured)
	  return;

	if (_hostConnected)
	{
		VirtualSerial_CDC_Interface.State.ControlLineStates.DeviceToHost |= (CDC_CONTROL_LINE_IN_DSR | CDC_CONTROL_LINE_IN_DCD);
		CDC_Device_SendControlLineStateChange(&VirtualSerial_CDC_Interface);
	}
}

/** Interrupt handler for TCC0 Compare/Capture A */
ISR(TCC0_CCA_vect)
{
	_secondsTick = 1;

	// Turn the LED on
	if (_hostConnected)
		LED_PORT->OUTSET = LED_PIN;
}

/** Interrupt handler for TCC0 Compare/Capture B */
ISR(TCC0_CCB_vect)
{
	// Turn the LED off
	LED_PORT->OUTCLR = LED_PIN;
}

/** Interrupt handler for TCC0 Compare/Capture A */
ISR(TCC1_CCA_vect)
{
	/* Must throw away unused bytes from the host, or it will lock up while waiting for the device */
	CDC_Device_ReceiveByte(&VirtualSerial_CDC_Interface);

	CDC_ProcessControlStateChange();

	/* Required calls */
	CDC_Device_USBTask(&VirtualSerial_CDC_Interface);
	USB_USBTask();
}