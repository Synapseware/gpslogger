#include "gpslogger.h"


/** Lifecycle and USB flags */
volatile int8_t		_secondsTick		= 0;
volatile int8_t		_hostConnected		= 0;
volatile int32_t	_flashWriteAddr		= 0;


/** NMEA processing data & flags */
static bool			_hasFix				= false;
static bool			_saveNmea			= false;
static bool			_recordPosition		= true;
static char			_nmea[256];
static char			_gpsVersion[96];
static int			_recordingTimeout	= 0;
static int			_gpsFixTimeout		= GPS_TIMEOUT;


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
		TC_SetCompareA( &TCC0, 1 );

		/* Setup interrupts for Compare/Capture A */
		TC0_SetCCAIntLevel( &TCC0, TC_CCAINTLVL_LO_gc );

		/* Setup interrupts for Compare/Capture B */
		TC_SetCompareB (&TCC0, 50 );
		TC0_SetCCBIntLevel( &TCC0, TC_CCBINTLVL_LO_gc );
	}

	// Configure TC1 for 20ms interrupts.  USB interractions
	// will fire from this handler
	{
		/* Select clock source */
		TC1_ConfigClockSource( &TCC1, TC_CLKSEL_DIV256_gc );

		/* F_CPU / 256 / 1250 = 100 */
		TC_SetPeriod( &TCC1, F_CPU / 256 / 125 );

		/* Setup interrupt for compare A */
		TC1_SetCCAIntLevel( &TCC1, TC_CCAINTLVL_LO_gc );
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

/**  */
void formatFlash(void)
{
	int count = readBlock(SPI_CS, 0, _nmea, sizeof(_nmea));
	memcmp_P(_nmea, PSTR("GpsLogger v1.1\r\n"), 18);
}

/** Prints the contents of the flash */
void dumpFlashContents(int32_t length)
{
	char msg[32];

	if (length < 1)
	{
		sprintf_P(msg, PSTR("  Flash is empty!\r\n"));
		fputs(msg, &USBSerialStream);
		return;
	}

	sprintf_P(msg, PSTR("  Contents:\r\n"));
	fputs(msg, &USBSerialStream);

	begin_read(SPI_CS, 0);
	for (int32_t count = 0; count < length; count++)
	{
		char data = read_byte();
		fputc(data, &USBSerialStream);
	}
	end_read(SPI_CS);
}

/** Finds the first free page in the dataflash chip */
int32_t FindFirstFreeAddress(void)
{
	uint8_t status = device_is_ready(SPI_CS);
	if (!status)
	{
		return -1;
	}

	/*
	sprintf_P(msg, PSTR("  Formatting flash\r\n"));
	fputs(msg, &USBSerialStream);

	erase_chip(SPI_CS);

	sprintf_P(msg, PSTR("  Waiting for format to complete\r\n"));
	fputs(msg, &USBSerialStream);

	// wait for RDY/BSY to clear
	while (!device_is_ready(SPI_CS));

	sprintf_P(msg, PSTR("  Formatted\r\n"));
	fputs(msg, &USBSerialStream);
	*/

	int32_t address = 0;
	int32_t freefrom = -1;
	int matches = 0;

	begin_read(SPI_CS, address);
	while (address < FLASH_TOTAL_SIZE)
	{
		// look for the first 0xFF followed by 0xFF's to the end
		char data = read_byte();
		if (0 == data)
		{
			// null character marks end of text
			freefrom = address;
			break;
		}
		address++;

		if (0xFF == data)
		{
			matches++;
			if (freefrom < 0)
				freefrom = address - 1;
		}
		else
		{
			matches = 0;
			freefrom = -1;
		}

		// 255 0xFF's is most likely a free page...
		if (matches > 255)
			break;
	}
	end_read(SPI_CS);

	return freefrom;
}

/** Saves the latest NMEA logging data to the flash */
bool SaveNMEA(void)
{
	// make sure our string is NULL terminated
	size_t sentenceLength = strlen(_nmea);
	if (_flashWriteAddr < 0 || sentenceLength >= sizeof(_nmea))
	{
		return false;
	}

	cli();

	ERR_LED_port->OUTSET = ERR_LED_pin;

	// write latest message to flash
	int idx = 0;
	char data;
	begin_write(SPI_CS, _flashWriteAddr);
	while (0 != (data = _nmea[idx++]))
	{
		// write a byte of data to the flash
		write_byte(data);
		_flashWriteAddr++;

		// check for page-boundary crossing
		if ((_flashWriteAddr & FLASH_PAGE_MASK) == 0)
		{
			end_write(SPI_CS);
			while(is_write_enabled(SPI_CS));

			// set programming to next page
			begin_write(SPI_CS, _flashWriteAddr);
		}
	}

	// complete the write cycle
	end_write(SPI_CS);
	while(is_write_enabled(SPI_CS));

	_flashWriteAddr += sentenceLength;

	ERR_LED_port->OUTCLR = ERR_LED_pin;

	sei();

	return true;
}

/** Callback function which is invoked when a complete message is received
This method is run from the UART RX interrupt */
void HandleLatestGpsMessage(const char* message, uint16_t length)
{
	char cmd_buff[16];

	// don't process anything if we are saving the last message
	if (_saveNmea)
	{
		return;
	}

	// make sure we have a valid sentence
	if (length < 5 || NULL == message)
	{
		// nothing to do
		return;
	}

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
		_hasFix = ('A' == *p);

		// location data - save to flash!
		if (_hasFix)
		{
			// copy the NMEA sentence to the flash buffer
			memcpy(_nmea, message, sizeof(_nmea));

			_saveNmea = true;
		}

		return;
	}

	// Parse PMTK
	strlcpy_P(cmd_buff, PSTR(PMTK_HEADER), sizeof(cmd_buff));
	if (0 == strncmp(message, cmd_buff, 5))
	{
		strlcpy_P(cmd_buff, PSTR(PMTK_VERSION_HEADER), sizeof(cmd_buff));
		if (0 == strncmp(message, cmd_buff, 9))
		{
			strncpy(_gpsVersion, message, sizeof(_gpsVersion));
		}

		return;
	}
}

/** Poweroff the GPS */
void DisableGps(void)
{
	Adafruit_GPS_Suspend();
	Adafruit_GPS_Disable();
}

/** Poweron the GPS after a poweroff */
void EnableGps(void)
{
	Adafruit_GPS_Enable();
	Adafruit_GPS_Wakeup();
	Adafruit_GPS_Update1Hz();
	Adafruit_GPS_RMCOnly();
}

/** System startup init */
void InitializeGps(void)
{
	Adafruit_GPS_OnMessageReceived(&HandleLatestGpsMessage);
	Adafruit_GPS_Enable();
	Adafruit_GPS_Init(GPS_USART, GPS_EN_port, GPS_EN_pin);
	Adafruit_GPS_Update1Hz();
	Adafruit_GPS_RMCOnly();
	Adafruit_GPS_AskForVersion();
}

/** Prints an initial greeting to the USB port
*/
void InitialGreeting(void)
{
	char msg[128];

	sprintf_P(msg, PSTR("GPS Logger v1.1 - 20150801\r\n"));
	fputs(msg, &USBSerialStream);

	if (0 != strlen(_gpsVersion))
		sprintf_P(msg, PSTR("GPS Version: %s\r\n"), _gpsVersion);
	else
		sprintf_P(msg, PSTR("GPS Version: (unknown)\r\n"));
	fputs(msg, &USBSerialStream);

	sprintf_P(msg, PSTR("Flash info:\r\n"));
	fputs(msg, &USBSerialStream);

	sprintf_P(msg, PSTR("  Valid: %c\r\n"), is_valid(SPI_CS) != 0 ? 'Y' : 'N');
	fputs(msg, &USBSerialStream);

	{
		/*
		ERR_LED_port->OUTSET = ERR_LED_pin;

		global_unprotect(SPI_CS);

		sprintf_P(msg, PSTR("  Formatting flash...\r\n"));
		fputs(msg, &USBSerialStream);
		erase_block_4k(SPI_CS, 0);
		while(is_write_enabled(SPI_CS));

		sprintf_P(msg, PSTR("  Done!\r\n"));
		fputs(msg, &USBSerialStream);

		ERR_LED_port->OUTCLR = ERR_LED_pin;
		*/
	}

	sprintf_P(msg, PSTR("  Searching for free start address...\r\n"));
	fputs(msg, &USBSerialStream);

	_flashWriteAddr = FindFirstFreeAddress();
	if (_flashWriteAddr < 0)
		sprintf_P(msg, PSTR("  Failed to find a free address.\r\n"));
	else
		sprintf_P(msg, PSTR("  Flash start address: %u\r\n"), _flashWriteAddr);
	fputs(msg, &USBSerialStream);

	dumpFlashContents(_flashWriteAddr);
}

/** Main program entry point. This routine contains the overall program flow, including initial
 *  setup of all components and the main program loop.
 */
int main(void)
{
	int delay = 3;
	char msg[160];

	SetupHardware();

	/* Create a regular character stream for the interface so that it can be used with the stdio.h functions */
	CDC_Device_CreateStream(&VirtualSerial_CDC_Interface, &USBSerialStream);

	// Begin main loop
	while(true)
	{
		if (_saveNmea)
		{
			// shutoff the GPS
			fputs_P(PSTR("Suspending GPS module.\r\n"), &USBSerialStream);
			DisableGps();

			fputs_P(PSTR("Saving NMEA sentence:\r\n"), &USBSerialStream);
			if (SaveNMEA())
				fputs_P(PSTR("  successfully saved nmea\r\n"), &USBSerialStream);
			else
				fputs_P(PSTR("  failed to save!\r\n"), &USBSerialStream);

			_saveNmea = false;
			_gpsFixTimeout = 0;
		}

		// reset the host-connected delay
		if (!_hostConnected)
		{
			delay = 3;
		}

		// only main-loop code below this point
		if (!_secondsTick)
		{
			continue;
		}
		_secondsTick = 0;


		// 
		// GPS recording timeout handling state processing
		// 
		if (_recordingTimeout)
		{
			// decrement the recording timeout
			_recordingTimeout--;

			// if the recording timeout is live, check the fix timeout
			if (_gpsFixTimeout)
			{
				_gpsFixTimeout--;

				// when the fix timeout is reached, suspend the GPS module
				if (!_gpsFixTimeout)
				{
					// GPS fix timeout reached - suspend the module
					DisableGps();

					// turn on the fix error status LED
					ERR_LED_port->OUTSET = ERR_LED_pin;
				}
			}
		}
		else
		{
			// recording timeout is 0 - wakeup the GPS module
			_recordingTimeout = GPS_INTERVAL;
			_gpsFixTimeout = GPS_TIMEOUT;
			EnableGps();

			// turn off the fix error status LED
			ERR_LED_port->OUTCLR = ERR_LED_pin;
		}


		// send greeting if we've hit our host-timeout and the host is connected
		if (_hostConnected)
		{
			if (delay)
			{
				delay--;
				if (!delay)
				{
					InitialGreeting();
				}
			}
		}
	}
}

/** Configures the board hardware and chip peripherals for the application's functionality. */
void SetupHardware(void)
{
	// initialize statics
	_secondsTick		= 0;
	_hostConnected		= 0;
	_flashWriteAddr		= 0;
	_hasFix				= false;
	_saveNmea			= false;
	memset(_nmea, 0, sizeof(_nmea));
	memset(_gpsVersion, 0, sizeof(_gpsVersion));
	_recordingTimeout	= 0;

	/* Powerdown unused/uneeded modules to save power */
	power_all_disable();
	power_usb_enable();
	power_rtc_enable();
	power_usarte0_enable();
	power_spic_enable();
	power_tc0c_enable();
	power_tc1c_enable();

	/* Start the PLL to multiply the 2MHz RC oscillator to 32MHz and switch the CPU core to run from it */
	XMEGACLK_StartPLL(CLOCK_SRC_INT_RC2MHZ, 2000000, F_CPU);
	XMEGACLK_SetCPUClockSource(CLOCK_SRC_PLL);

	/* Start the 32MHz internal RC oscillator and start the DFLL to increase it to 48MHz using the USB SOF as a reference */
	XMEGACLK_StartInternalOscillator(CLOCK_SRC_INT_RC32MHZ);
	XMEGACLK_StartDFLL(CLOCK_SRC_INT_RC32MHZ, DFLL_REF_INT_USBSOF, F_USB);

	/* Configure RTC to use internal clock */
	OSC.CTRL |= OSC_RC32KEN_bm;
	while((OSC.STATUS & OSC_RC32KRDY_bm) == 0);
	CLK.RTCCTRL = CLK_RTCSRC_RCOSC_gc | CLK_RTCEN_bm;
	while (RTC_Busy());

	// TODO: Handle proper timeout between GPS fix timeout and logging timeout
	RTC_Initialize( RTC_CYCLES_1S, 0, 0, RTC_PRESCALER_DIV1_gc );
	RTC_SetIntLevels( RTC_OVFINTLVL_LO_gc, RTC_COMPINTLVL_OFF_gc );

	/* Enable lov, med and high level interrupts in the PMIC */
	PMIC.CTRL = PMIC_LOLVLEN_bm | PMIC_MEDLVLEN_bm | PMIC_HILVLEN_bm;

	/* Hardware Initialization */
	USB_Init();

	// Initialize the timer(s)
	Timer_Initialize();

	GlobalInterruptEnable();

	// initialize user hardware here
	LED_PORT->OUTCLR = LED_PIN;
	LED_PORT->DIRSET = LED_PIN;

	// initialize debug LED
	DIAG_LED_port->OUTCLR = DIAG_LED_pin;
	DIAG_LED_port->DIRSET = DIAG_LED_pin;

	// initialize error LED
	ERR_LED_port->OUTCLR = ERR_LED_pin;
	ERR_LED_port->DIRSET = ERR_LED_pin;

	INFO_LED_port->OUTCLR = INFO_LED_pin;
	INFO_LED_port->DIRSET = INFO_LED_pin;

	// intialize the GPS module
	GPS_UART_port->DIRSET = GPS_TX_pin;
	GPS_UART_port->DIRCLR = GPS_RX_pin;

	// Prepare GPS module
	InitializeGps();

	// reset the SPI device
	SPI_DEVICE.CTRL = 0;
	SPI_DEVICE.INTCTRL = 0;
	SPI_DEVICE.STATUS = 0;

	// Initialize the SPI device for the flash chip
	SPI_Init(&SPI_DEVICE,
		SPI_MODE_MASTER |
		SPI_SPEED_FCPU_DIV_32 |
		SPI_ORDER_MSB_FIRST |
		SPI_SCK_LEAD_RISING |
		SPI_SAMPLE_LEADING
	);

	// Initialize the FLASH driver
	FlashDriver_Init(&SPI_DEVICE, SPI_PORT, SPI_CS);
}

/** Event handler for the library USB Connection event. */
void EVENT_USB_Device_Connect(void)
{
	// turn on the diag LED
	//DIAG_LED_port->OUTSET = DIAG_LED_pin;

	_hostConnected = 1;
}

/** Event handler for the library USB Disconnection event. */
void EVENT_USB_Device_Disconnect(void)
{
	// turn off the diag LED
	//DIAG_LED_port->OUTCLR = DIAG_LED_pin;

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
/*
	void EVENT_CDC_Device_ControLineStateChanged(USB_ClassInfo_CDC_Device_t *const CDCInterfaceInfo)
	{
		bool CurrentDTRState = (CDCInterfaceInfo->State.ControlLineStates.HostToDevice & CDC_CONTROL_LINE_OUT_DTR);
		if (CurrentDTRState)
		{
			_hostConnected = 1;
			DIAG_LED_port->OUTSET = DIAG_LED_pin;
		}
		else
		{
			_hostConnected = 0;
			DIAG_LED_port->OUTCLR = DIAG_LED_pin;
		}
	}
*/

/** Process control line signals */
void CDC_ProcessControlStateChange(void)
{
	/* Device must be connected and configured for the task to run */
	if (USB_DeviceState != DEVICE_STATE_Configured)
	{
		return;
	}

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
	{
		LED_PORT->OUTSET = LED_PIN;
	}
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

	//CDC_ProcessControlStateChange();

	/* Required calls */
	CDC_Device_USBTask(&VirtualSerial_CDC_Interface);
	USB_USBTask();
}

/** RTC Overflow.  Just sleep */
ISR(RTC_OVF_vect)
{
	// TODO: 
}
