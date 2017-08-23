#include "gpslogger.h"


/** Lifecycle and USB flags */
volatile int8_t		_secondsTick		= 0;
volatile int8_t		_hostConnected		= 1;
volatile int32_t	_flashWriteAddr		= 0;
volatile int		_serialData			= -1;

/** NMEA processing data & flags */
static bool			_hasFix				= false;
static bool			_saveNmea			= false;
static char			_nmea[256];
static char			_gpsVersion[96];
static int			_recordingTimeout	= 0;
static int			_gpsFixTimeout		= GPS_TIMEOUT;
static uint8_t		_allowedMisses		= GPS_FIX_MISSES;


/** LUFA CDC Class driver interface configuration and state information. This structure is
 *  passed to all CDC Class driver functions, so that multiple instances of the same class
 *  within a device can be differentiated from one another.
 */
USB_ClassInfo_CDC_Device_t VirtualSerial_CDC_Interface =
{
	.Config =
	{
		.ControlInterfaceNumber	= INTERFACE_ID_CDC_CCI,
		.DataINEndpoint       	=
		{
			.Address          	= CDC_TX_EPADDR,
			.Size             	= CDC_TXRX_EPSIZE,
			.Banks            	= 1,
		},
		.DataOUTEndpoint =
		{
			.Address          	= CDC_RX_EPADDR,
			.Size             	= CDC_TXRX_EPSIZE,
			.Banks            	= 1,
		},
		.NotificationEndpoint =
		{
			.Address          	= CDC_NOTIFICATION_EPADDR,
			.Size             	= CDC_NOTIFICATION_EPSIZE,
			.Banks            	= 1,
		},
	},
};


/** Standard file stream for the CDC interface when set up, so that the virtual CDC COM port can be
 *  used like any regular character stream in the C APIs.
 */
static FILE USBSerialStream;

static void PrintString_P(const char* message)
{
	if (!_hostConnected || NULL == message)
		return;

	fputs_P(message, &USBSerialStream);
}
static void PrintString(const char* message)
{
	if (!_hostConnected || NULL == message)
		return;

	fputs(message, &USBSerialStream);
}
static void PrintChar(char data)
{
	if (!_hostConnected)
		return;

	fputc(data, &USBSerialStream);
}
static void PrintFlush(void)
{
	if (!_hostConnected)
		return;
	CDC_Device_Flush(&VirtualSerial_CDC_Interface);
}
static void LCDPrintString_P(const char* str, int length)
{
	if (TWI_StartTransmission(&LCD_TWI, LCD_TWI_ADDR | TWI_ADDRESS_WRITE, 100) == TWI_ERROR_NoError)
	{
		TWI_StopTransmission(&LCD_TWI);
	}
}
static void LCDClear(void)
{

}


/** Configures the CPU */
static void ConfigureCPU(void)
{
	/* Start the PLL to multiply the 2MHz RC oscillator to 32MHz and switch the CPU core to run from it */
	XMEGACLK_StartPLL(CLOCK_SRC_INT_RC2MHZ, 2000000, F_CPU);
	XMEGACLK_SetCPUClockSource(CLOCK_SRC_PLL);

	/* Start the 32MHz internal RC oscillator and start the DFLL to increase it to 48MHz using the USB SOF as a reference */
	XMEGACLK_StartInternalOscillator(CLOCK_SRC_INT_RC32MHZ);
	XMEGACLK_StartDFLL(CLOCK_SRC_INT_RC32MHZ, DFLL_REF_INT_USBSOF, F_USB);

	/* Enable lov, med and high level interrupts in the PMIC */
	PMIC.CTRL = PMIC_LOLVLEN_bm | PMIC_MEDLVLEN_bm | PMIC_HILVLEN_bm;
}

/** Configures the CPU */
static void ConfigureCPU(void)
{
	/* Start the PLL to multiply the 2MHz RC oscillator to 32MHz and switch the CPU core to run from it */
	XMEGACLK_StartPLL(CLOCK_SRC_INT_RC2MHZ, 2000000, F_CPU);
	XMEGACLK_SetCPUClockSource(CLOCK_SRC_PLL);

	/* Start the 32MHz internal RC oscillator and start the DFLL to increase it to 48MHz using the USB SOF as a reference */
	XMEGACLK_StartInternalOscillator(CLOCK_SRC_INT_RC32MHZ);
	XMEGACLK_StartDFLL(CLOCK_SRC_INT_RC32MHZ, DFLL_REF_INT_USBSOF, F_USB);

	/* Enable lov, med and high level interrupts in the PMIC */
	PMIC.CTRL = PMIC_LOLVLEN_bm | PMIC_MEDLVLEN_bm | PMIC_HILVLEN_bm;
}

/** Configure timers */
static void InitializeTimers(void)
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

	// Configure TC1 for 20ms interrupts.  USB interractions
	// will fire from this handler
	{
		/* Select clock source */
		TC1_ConfigClockSource( &TCC1, TC_CLKSEL_DIV256_gc );

		/* F_CPU / 256 / 2500 = 50Hz (or 20ms) */
		TC_SetPeriod( &TCC1, USB_TIMER_PERIOD );

		/* Setup interrupt for compare A */
		TC1_SetCCAIntLevel( &TCC1, TC_CCAINTLVL_MED_gc );
	}
}

/** Configure the ADC to read the GPS backup battery voltage */
static void InitializeADC(void)
{

	// Configure ADC
}

/** Configure the SPI interface */
static void InitializeSPI(void)
{
	// reset the SPI device
	SPI_DEVICE.CTRL = 0;
	SPI_DEVICE.INTCTRL = 0;
	SPI_DEVICE.STATUS = 0;

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

/** Configure hardware to support the LCD */
static void InitializeLCD(void)
{
	ConfigureLCD();

	// Initialize the TWI interface
	TWI_Init(&LCD_TWI, TWI_BAUD_FROM_FREQ(LCD_BAUD));

	if (TWI_StartTransmission(&LCD_TWI, LCD_TWI_ADDR | TWI_ADDRESS_WRITE, 100) == TWI_ERROR_NoError)
	{
		TWI_SendByte(&LCD_TWI, ~LCD_BACKLIGHT_ON);

		TWI_StopTransmission(&LCD_TWI);
	}
	else
	{
		TurnOnErrLed();
		while(1){}
	}
}

/** Poweroff the GPS */
static void DisableGps(void)
{
	Adafruit_GPS_Suspend();
	//while ((Adafruit_GPS_CurrentState() & ADAFRUIT_GPS_FLAGS_STANDBYOK) == 0);
}

/** Poweron the GPS after a poweroff */
static void EnableGps(void)
{	
	TurnOnInfoLed();
	Adafruit_GPS_Enable();
	Adafruit_GPS_ResetState();

	Adafruit_GPS_Wakeup();
	Adafruit_GPS_ColdStart();

	//while ((Adafruit_GPS_CurrentState() & ADAFRUIT_GPS_FLAGS_COMPLETE) == 0);
	//while ((Adafruit_GPS_CurrentState() & ADAFRUIT_GPS_FLAGS_START) == 0);
	//while ((Adafruit_GPS_CurrentState() & ADAFRUIT_GPS_FLAGS_AWAKE) == 0);

	Adafruit_GPS_Update1Hz();
	//while ((Adafruit_GPS_CurrentState() & ADAFRUIT_GPS_FLAGS_1HZ_OK) == 0);

	Adafruit_GPS_RMCOnly();
	//while ((Adafruit_GPS_CurrentState() & ADAFRUIT_GPS_FLAGS_OUTPUT_CHG) == 0);
	TurnOffInfoLed();
}

/** Configure the GPS hardware */
static void InitializeGps(void)
{
	ConfigureGpsUart();
	ConfigureGpsPins();

	Adafruit_GPS_Init(&HandleLatestGpsMessage);

	EnableGps();

	TurnOnInfoLed();

	Adafruit_GPS_ResetState();
	Adafruit_GPS_AskForVersion();

	//while ((Adafruit_GPS_CurrentState() & ADAFRUIT_GPS_FLAGS_VERSION) == 0);

	TurnOffInfoLed();
}

/** Configures the board hardware and chip peripherals for the application's functionality. */
void SetupHardware(void)
{
	// initialize variables
	_secondsTick		= 0;
	_hostConnected		= 0;
	_flashWriteAddr		= 0;
	_recordingTimeout	= 0;
	_serialData			= -1;
	_hasFix				= false;
	_saveNmea			= false;
	_allowedMisses		= GPS_FIX_MISSES;
	memset(_nmea, 0, sizeof(_nmea));
	memset(_gpsVersion, 0, sizeof(_gpsVersion));

	ConfigureCPU();

	// setup LEDs
	ConfigureLeds();
	TurnOffLeds();

	/* Hardware Initialization */
	USB_Init();

	InitializeTimers();
	InitializeADC();
	InitializeSPI();
	InitializeLCD();

	// Prepare GPS module
	InitializeGps();

	// all done with setup - enable interrupts
	GlobalInterruptEnable();
}

/** Finds the first free page in the dataflash chip */
static int32_t FindFirstFreeAddress(void)
{
	if (!device_is_ready(SPI_CS))
	{
		TurnOnErrLed();
	}

	TurnOffErrLed();
	TurnOnDiagLed();

	int32_t address = 0;
	int32_t freefrom = -1;
	int matches = 0;

	begin_read(SPI_CS, address);
	while (address < FLASH_TOTAL_SIZE && address > -1)
	{
		// look for the first 0xFF followed by 0xFF's to the end
		char data = read_byte();

		if (0xFF == data)
		{
			matches++;
			if (freefrom < 0)
				freefrom = address;
		}
		else
		{
			matches = 0;
			freefrom = -1;
		}

		address++;

		// 255 0xFF's is most likely a free page...
		if (matches > 255)
			break;
	}
	end_read(SPI_CS);

	TurnOffDiagLed();

	return freefrom;
}

/** Saves the latest NMEA logging data to the flash */
static bool SaveNMEA(void)
{
	// make sure our string is NULL terminated
	int sentenceLength = strlen(_nmea);
	if (_flashWriteAddr < 0 || sentenceLength >= sizeof(_nmea))
	{
		return false;
	}

	// make sure sentence starts with a valid character
	if ('$' != _nmea[0])
	{
		return false;
	}

	GlobalInterruptDisable();

	global_unprotect(SPI_CS);

	TurnOnDiagLed();

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

	TurnOffDiagLed();

	GlobalInterruptEnable();

	return true;
}

/** Callback function which is invoked when a complete message is received
This method is run from the UART RX interrupt */
void HandleLatestGpsMessage(const char* message, int length)
{
	char cmd_buff[16];

	// don't process anything if we are saving the last message
	if (_saveNmea)
	{
		return;
	}

	// make sure we have a valid sentence
	if (length < 5 || NULL == message || '$' != message[0])
	{
		// nothing to do
		return;
	}

	// GPS NMEA data: http://www.gpsinformation.org/dale/nmea.htm

	// parse GPRMC - recommended minimum navigation data
	strlcpy_P(cmd_buff, PSTR(PMTK_GPRMC_HEADER), sizeof(cmd_buff));
	if (0 == strncmp(message, cmd_buff, 6))
	{
		// process fix data - look for 'V' or 'A'
		//					  time(hhmmss.sss)									 date(yymmdd)
		// with fix:	$GPRMC,072450.000,A,4078.8163,N,10921.9609,W,0.14,355.98,170815,,,A*7A
		// no fix:		$GPRMC,072450.000,V,4078.8163,N,10921.9609,W,0.14,355.98,170815,,,A*7A
		// 				$GPRMC,043301.000,A,4802.9906,N,12249.8940,W,0.54,0.00,301115,,,A*7E

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

/** Prints an initial greeting to the USB port */
static void InitialGreeting(void)
{
	char msg[128];

	PrintString_P(PSTR("GPS Logger v1.1 - 20150801\r\n"));
	PrintString_P(PSTR(" Version: "));
	if (0 != strlen(_gpsVersion))
		PrintString(_gpsVersion);
	else
		PrintString_P(PSTR("(unknown)"));
	PrintString_P(PSTR("\r\n"));

	PrintString_P(PSTR("Flash info:\r\n"));

	PrintString_P(PSTR("  Valid: "));
	TurnOnErrLed();
	if (is_valid(SPI_CS))
		PrintString_P(PSTR("Y"));
	else
		PrintString_P(PSTR("N"));	
	PrintString_P(PSTR("\r\n"));
	TurnOffErrLed();

	{
		/*
		TurnOnErrLed();

		global_unprotect(SPI_CS);

		PrintString_P(PSTR("  Formatting flash...\r\n"));
		erase_block_4k(SPI_CS, 0);
		while(is_write_enabled(SPI_CS));

		PrintString_P(PSTR("  Done!\r\n"));

		TurnOffErrLed();
		TurnOnInfoLed();

		while(1);
		*/
	}

	PrintString_P(PSTR("  Searching for free start address...\r\n"));

	_flashWriteAddr = FindFirstFreeAddress();
	if (_flashWriteAddr < 0)
	{
		PrintString_P(PSTR("  Failed to find a free address.\r\n"));
		return;
	}

	sprintf_P(msg, PSTR("  Flash start address: %u\r\n"), _flashWriteAddr);
	PrintString(msg);
	PrintString_P(PSTR("  Contents:\r\n"));

	begin_read(SPI_CS, 0);
	while(true)
	{
		char data = read_byte();
		if (0xFF == data)
			break;

		PrintChar(data);
	}
	end_read(SPI_CS);

	PrintString_P(PSTR("\r\n"));
	PrintFlush();
}

/** Print GPS messages to console */
void LogGpsMessages(const char* message, int length)
{
	if (length < 2 || NULL == message)
		return;

	PrintString_P(PSTR("< "));
	PrintString(message);
	PrintFlush();
}

/**  */
void GpsMenu(void)
{
	Adafruit_GPS_Init(&LogGpsMessages);
	Adafruit_GPS_Suspend();

	PrintString_P(PSTR("GPS Interactive Menu\r\n"));
	PrintString_P(PSTR("H - Hot start GPS\r\n"));
	PrintString_P(PSTR("S - Suspend\r\n"));
	PrintString_P(PSTR("W - Wakeup\r\n"));
	PrintString_P(PSTR("U - Set update to 1Hz\r\n"));	
	PrintString_P(PSTR("V - Ask for version\r\n"));
	PrintString_P(PSTR("O - Output off\r\n"));
	PrintString_P(PSTR("M - Minimum (GPRMC) logging only\r\n"));
	PrintString_P(PSTR("R - Reset state\r\n"));
	PrintString_P(PSTR("X - Exit\r\n"));
	PrintString_P(PSTR("\r\n"));
	PrintFlush();

	while (1)
	{
		int usb = CDC_Device_ReceiveByte(&VirtualSerial_CDC_Interface);
		if (usb < 0)
			continue;

		switch (usb | 0x40)
		{
			case 'h':
				PrintString_P(PSTR("> $PMTK101*32 (hot start)\r\n"));
				Adafruit_GPS_HotStart();
				break;
			case 's':
				PrintString_P(PSTR("> $PMTK161,0*28 (suspending)\r\n"));
				Adafruit_GPS_Suspend();
				break;
			case 'w':
				PrintString_P(PSTR("> ' ' (wakeup)\r\n"));
				Adafruit_GPS_Wakeup();
				break;
			case 'u':
				PrintString_P(PSTR("> $PMTK220,100*2F (1Hz output)\r\n"));
				Adafruit_GPS_Update1Hz();
				break;
			case 'v':
				PrintString_P(PSTR("> $PMTK605*31 (version query)\r\n"));
				Adafruit_GPS_AskForVersion();
				break;
			case 'o':
				PrintString_P(PSTR("> $PMTK314,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0*28 (output off)\r\n"));
				Adafruit_GPS_OutputOff();
				break;
			case 'm':
				PrintString_P(PSTR("> $PMTK314,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0*29 (GPRMC only)\r\n"));
				Adafruit_GPS_RMCOnly();
				break;
			case 'r':
				PrintString_P(PSTR("Resetting GPS\r\n"));
				Adafruit_GPS_ResetState();
				break;
			case 'x':
				PrintString_P(PSTR("Exit GPS menu!\r\n"));
				Adafruit_GPS_Init(&HandleLatestGpsMessage);
				return;
		}
	}
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
	//CDC_Device_CreateStream(&VirtualSerial_CDC_Interface, &USBSerialStream);
	CDC_Device_CreateBlockingStream(&VirtualSerial_CDC_Interface, &USBSerialStream);

	// Begin main loop
	while(true)
	{
		// check for valid serial data
		if (_serialData > -1)
		{
			HandleGpsSerialInterrupt(_serialData);
			_serialData = -1;
		}

		if (_saveNmea)
		{
			// shutoff the GPS
			PrintString_P(PSTR("Suspending GPS module.\r\n"));
			DisableGps();

			PrintString_P(PSTR("Saving NMEA sentence:\r\n"));
			PrintString(_nmea);

			sprintf_P(msg, PSTR("  address before: %u\r\n"), _flashWriteAddr);
			PrintString(msg);

			if (SaveNMEA())
			{
				PrintString_P(PSTR("  successfully saved nmea\r\n"));

				sprintf_P(msg, PSTR("  address after: %u\r\n"), _flashWriteAddr);
				PrintString(msg);
			}
			else
				PrintString_P(PSTR("  failed to save\r\n"));

			_saveNmea = false;
			_gpsFixTimeout = 0;
			_allowedMisses = 0;

			//EnterPowerSave();
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
					if (_allowedMisses)
					{
						sprintf_P(msg, PSTR("Skipping fix timeout %i\r\n"), _allowedMisses);
						PrintString(msg);
						_allowedMisses--;
					}
					else
					{
						// GPS fix timeout reached - suspend the module
						DisableGps();

						// turn on the fix error status LED
						TurnOnErrLed();

						PrintString_P(PSTR("GPS failed to get a fix within the timeout period.\r\n"));
						PrintString_P(PSTR("Disabling GPS.\r\n"));
					}
				}
			}

			if (!_recordingTimeout)
			{
				// turn on the diag LED to indicate normal suspend
				TurnOnErrLed();
			}
		}
		else if (!_allowedMisses)
		{
			PrintString_P(PSTR("Enabling GPS.\r\n"));

			// recording timeout is 0 - wakeup the GPS module
			_recordingTimeout = GPS_INTERVAL;
			_gpsFixTimeout = GPS_TIMEOUT;
			_allowedMisses = GPS_FIX_MISSES;
			EnableGps();

			// turn off the fix error status LED
			TurnOffErrLed();
			TurnOffDiagLed();
		}


		// reset the host-connected delay
		if (!_hostConnected)
		{
			delay = 3;
		}
		else
		{
			// send greeting if we've hit our host-timeout and the host is connected
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

/** Event handler for the library USB Connection event. */
void EVENT_USB_Device_Connect(void)
{
	// turn on the diag LED
	//TurnOnDiagLed();

	_hostConnected = 1;

	TC_SetCompareB (&TCC0, USB_CONNECTED_DELAY);
}

/** Event handler for the library USB Disconnection event. */
void EVENT_USB_Device_Disconnect(void)
{
	// turn off the diag LED
	//TurnOffDiagLed();

	_hostConnected = 0;

	TC_SetCompareB (&TCC0, USB_DISCONN_DELAY);
}

/** Event handler for the library USB Configuration Changed event. */
void EVENT_USB_Device_ConfigurationChanged(void)
{
	// configure the CDC interface
	CDC_Device_ConfigureEndpoints(&VirtualSerial_CDC_Interface);
}

/** Event handler for the library USB Control Request reception event. */
void EVENT_USB_Device_ControlRequest(void)
{
	// pass the config data to the processor
	CDC_Device_ProcessControlRequest(&VirtualSerial_CDC_Interface);
}

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
		TurnOnBuiltInLed();
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
ISR(TCC1_CCA_vect)
{
	/* Must throw away unused bytes from the host, or it will lock up while waiting for the device */
	CDC_Device_ReceiveByte(&VirtualSerial_CDC_Interface);

	//CDC_ProcessControlStateChange();

	/* Required calls */
	CDC_Device_USBTask(&VirtualSerial_CDC_Interface);
	USB_USBTask();

	// clear interrupt flag
	TCC1.INTFLAGS = TC1_CCAIF_bm;
}

/** GPS USART Receive interrupt handler */
ISR(USARTE0_RXC_vect)
{
	char data = GPS_UART.DATA;

	_serialData = data;
}
