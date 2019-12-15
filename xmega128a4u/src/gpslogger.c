#include "gpslogger.h"


/** Lifecycle and USB flags */
volatile int8_t		_secondsTick		= 0;
volatile int32_t	_flashWriteAddr		= 0;
volatile int		_gpsData			= -1;

volatile uint16_t	_gpsBattery			= 0;
volatile uint16_t	_adcaCal			= 0;

/** NMEA processing data & flags */
static bool			_hasFix				= false;
static bool			_saveNmea			= false;
static bool			_displayMessage		= false;
static bool			_processGpsMessage	= false;
static char			_nmea[256];
static char			_gpsVersion[96];
static int			_recordingTimeout	= 0;
static int			_gpsFixTimeout		= GPS_TIMEOUT;
static uint8_t		_allowedMisses		= GPS_FIX_MISSES;
static char 		_lastGpsMessage[256];
static char			_buffer[256];


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

// Setup the TWI configuration
LCD_TWI_Interface_t lcdInterface =
{
	.TWI = &LCD_TWI,
	.Port = &LCD_PORT,
	.Pins = LCD_SDA | LCD_SCL, // SDA & SCL
	//.Control = LCD_CTRL,
	.Address = 0x7E,
	.Baud = 100000
};



/** Configures the CPU */
static void ConfigureCPU(void)
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
static void InitializeADC(void)
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

/** Poweroff the GPS */
static void DisableGps(void)
{
	// Sends the suspend command
	Adafruit_GPS_Suspend(500);
	Adafruit_GPS_Disable();
}

/** Enables and performs initial configuration */
static void EnableGps(void)
{
	TurnOnDiagLed();
	Adafruit_GPS_Enable();
	Adafruit_GPS_Wakeup();

	Adafruit_GPS_WaitForPPS(1000);
	Adafruit_GPS_ColdStart(500);
	Adafruit_GPS_Update1Hz(500);
	Adafruit_GPS_RMCOnly(500);

	memset(_gpsVersion, 0, sizeof(_gpsVersion));
	if (!Adafruit_GPS_AskForVersion(500, _gpsVersion, sizeof(_gpsVersion)))
	{
		strncpy_P(_gpsVersion, PSTR("NO VERSION FOUND"), sizeof(_gpsVersion));
		TurnOnErrLed();
		while(true);
	}

	TurnOffDiagLed();
}

/** Print GPS messages to console */
void DebugLogGpsMessages(const char* message)
{
	if (NULL == message)
		return;

	int length = strlen(message);

	if (message[0] == '\r' || message[0] == '\n' || length < 5)
	{
		memset(message, 0, length);
		return;
	}

	USB_PrintString_P(PSTR("> "));
	USB_PrintString(message);
	USB_PrintString_P(PSTR("\r\n"));

	// clear the buffer
	memset(message, 0, length);
}


/** Callback function which is invoked when a complete message is received
This method is run from the UART RX interrupt
GPS NMEA data: http://www.gpsinformation.org/dale/nmea.htm */
void HandleLatestGpsMessage(const char* message)
{
	strlcpy(_lastGpsMessage, message, sizeof(_lastGpsMessage));
	memset(message, 0, strlen(message));
	_processGpsMessage = true;
}

void ProcessGpsMessage(const char* message)
{
	char cmd_buff[16];

	// make sure we have a valid sentence
	if (NULL == message || '$' != message[0])
	{
		// nothing to do
		return;
	}

	int length = strlen(message);
	if (length < 5)
		return;

	// parse GPRMC - recommended minimum navigation data
	strlcpy_P(cmd_buff, PMTK_GPRMC_HEADER, sizeof(cmd_buff));
	if (0 == strncmp(message, cmd_buff, 6))
	{
		// process fix data - look for 'V' or 'A'
		//					  time(hhmmss.sss)									 date(yymmdd)
		// with fix:	$GPRMC,072450.000,A,4078.8163,N,10921.9609,W,0.14,355.98,170815,,,A*7A
		// no fix:		$GPRMC,072450.000,V,4078.8163,N,10921.9609,W,0.14,355.98,170815,,,A*7A
		// 				$GPRMC,043301.000,A,4802.9906,N,12249.8940,W,0.54,0.00,301115,,,A*7E

		char* p = strchr(message, ',') + 1;
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

		_displayMessage = true;

		return;
	}

	// Parse PMTK
	strlcpy_P(cmd_buff, PMTK_HEADER, sizeof(cmd_buff));
	if (0 == strncmp(message, cmd_buff, 5))
	{
		strlcpy_P(cmd_buff, PMTK_VERSION_HEADER, sizeof(cmd_buff));
		if (0 == strncmp(message, cmd_buff, 9))
		{
			strncpy(_gpsVersion, message, sizeof(_gpsVersion));
		}

		return;
	}
}

/** Configure the GPS hardware */
static void InitializeGps(void)
{
	ConfigureGpsUart();
	ConfigureGpsPins();

	Adafruit_GPS_Init(&HandleLatestGpsMessage);
}

/** Configures the board hardware and chip peripherals for the application's functionality. */
void SetupHardware(void)
{
	// initialize variables
	_secondsTick		= 0;
	_flashWriteAddr		= 0;
	_recordingTimeout	= 0;
	_gpsData			= -1;
	_hasFix				= false;
	_saveNmea			= false;
	_allowedMisses		= GPS_FIX_MISSES;
	_processGpsMessage	= false;
	memset(_buffer, 0, sizeof(_buffer));
	memset(_nmea, 0, sizeof(_nmea));
	memset(_gpsVersion, 0, sizeof(_gpsVersion));
	memset(_lastGpsMessage, 0, sizeof(_lastGpsMessage));

	ConfigureCPU();

	// setup LEDs
	ConfigureLeds();
	TurnOffLeds();

	// Initialize peripherals
	InitializeTimers();
	//LCD_Configure(&lcdInterface);
	InitializeADC();
	InitializeSPI();

	// Prepare GPS module
	InitializeGps();

	// all done with setup - enable interrupts
	GlobalInterruptEnable();

	/* Usb Initialization */
	TurnOnBuiltInLed();
	USB_Initialize();
	TurnOffBuiltInLed();
	//LCD_PrintString_P(PSTR("GPS Logger"));
}

/** Finds the first free page in the dataflash chip */
static int32_t FindFirstFreeAddress(void)
{
	if (!device_is_ready(SPI_CS))
	{
		TurnOnErrLed();
	}

	TurnOnInfoLed();

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

	TurnOffInfoLed();

	return freefrom;
}

/** Maps the raw ADC sample to a voltage value */
static double ConvertBatteryVoltage(uint16_t sample)
{
	// Input range is 12-bit
	// Voltage range is 0.0 to 3.0v
	// ADC input is VBatt/2
	// ADC transfer function is:
	// RES = ((VINP-VINN)/VREF)*GAIN*(TOP+1)
	// Map transfer function is:
	// VIN = (RES/TOP+1)*VREF
	// VIN = (sample/4096)*3.3

	// instead of multiplying by 3.3, substracting a constant
	// and then doubling to recover from the voltage divider,
	// just multiply by 5.191
	return (sample/4096.0) * 5.191;
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

	// shutoff the GPS
	USB_PrintString_P(PSTR("Suspending GPS module.\r\n"));
	DisableGps();

	USB_PrintString_P(PSTR("Saving NMEA sentence:\r\n"));
	USB_PrintString(_nmea);

	USB_PrintString_P(PSTR("  address before: "));
	sprintf_P(_buffer, PSTR("%u\r\n"), _flashWriteAddr);
	USB_PrintString(_buffer);

	TurnOnInfoLed();

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

	TurnOffInfoLed();

	return true;
}

/** Prints an initial greeting to the USB port */
static void InitialGreeting(void)
{
	USB_PrintString_P(PSTR("GPS Logger v1.1 - 20150801\r\n"));
	USB_PrintString_P(PSTR(" Version: "));
	if (strlen(_gpsVersion) > 0)
		USB_PrintString(_gpsVersion);
	else
		USB_PrintString_P(PSTR("(unknown)"));
	USB_PrintString_P(PSTR("\r\n"));

	USB_PrintString_P(PSTR("Flash info:\r\n"));
	USB_PrintString_P(PSTR("  Valid: "));
	if (is_valid(SPI_CS))
		USB_PrintString_P(PSTR("Y"));
	else
		USB_PrintString_P(PSTR("N"));	
	USB_PrintString_P(PSTR("\r\n"));

	{
		/*
		TurnOnErrLed();

		global_unprotect(SPI_CS);

		USB_PrintString_P(PSTR("  Formatting flash...\r\n"));
		erase_block_4k(SPI_CS, 0);
		while(is_write_enabled(SPI_CS));

		USB_PrintString_P(PSTR("  Done!\r\n"));

		TurnOffErrLed();
		TurnOnInfoLed();

		while(1);
		*/
	}

	USB_PrintString_P(PSTR("  Searching for free start address...\r\n"));
	_flashWriteAddr = FindFirstFreeAddress();
	if (_flashWriteAddr < 0)
	{
		USB_PrintString_P(PSTR("  Failed to find a free address.\r\n"));
		return;
	}

	sprintf_P(_buffer, PSTR("  Flash start address: %u\r\n"), _flashWriteAddr);
	USB_PrintString(_buffer);

	/*
	USB_PrintString_P(PSTR("  Contents:\r\n"));
	begin_read(SPI_CS, 0);
	while(true)
	{
		char data = read_byte();
		if (0xFF == data)
			break;

		PrintChar(data);
	}
	end_read(SPI_CS);
	USB_PrintString_P(PSTR("\r\n"));
	*/

	USB_PrintFlush();
}

void GpsMenuShowHelp(void)
{
	USB_PrintString_P(PSTR("GPS Interactive Menu\r\n"));
	USB_PrintString_P(PSTR(" H - Hot start GPS\r\n"));
	USB_PrintString_P(PSTR(" C - Cold start GPS\r\n"));
	USB_PrintString_P(PSTR(" B - Read battery voltage\r\n"));
	USB_PrintString_P(PSTR(" L - Display calibration data\r\n"));
	USB_PrintString_P(PSTR(" S - Suspend\r\n"));
	USB_PrintString_P(PSTR(" W - Wakeup\r\n"));
	USB_PrintString_P(PSTR(" E - Enable\r\n"));
	USB_PrintString_P(PSTR(" D - Disable\r\n"));
	USB_PrintString_P(PSTR(" U - Set update to 1Hz\r\n"));	
	USB_PrintString_P(PSTR(" V - Ask for version\r\n"));
	USB_PrintString_P(PSTR(" O - Output off\r\n"));
	USB_PrintString_P(PSTR(" A - All Data\r\n"));
	USB_PrintString_P(PSTR(" M - Minimum (GPRMC) logging only\r\n"));
	USB_PrintString_P(PSTR(" R - Reset state\r\n"));
	USB_PrintString_P(PSTR(" X - Exit\r\n"));
	USB_PrintString_P(PSTR(" ? - Show this menu\r\n"));
	USB_PrintString_P(PSTR("\r\n"));
}

/** Displays a menu and allows the host to enter an interactive session
with the GPS module */
void GpsMenu(void)
{
	GpsMenuShowHelp();

	Adafruit_GPS_Disable();
	USB_PrintString_P(PSTR("GPS is now disabled\r\n"));

	double voltage = 0.0;

	while (USB_IsHostConfigured() && USB_IsHostConnected())
	{
		int usb = 0;
		if (USB_BytesReceived() > 0)
		{
			usb = USB_ReceiveByte();
		}
		else
		{
			continue;
		}

		TurnOnDiagLed();

		switch (usb | 0x40)
		{
			case '?':
				GpsMenuShowHelp();
				break;
			case 'h':
				USB_PrintString_P(PSTR("> "));
				USB_PrintString_P(PMTK_CMD_HOT_START);
				USB_PrintString_P(PSTR(" (hot start)\r\n"));
				Adafruit_GPS_HotStart();
				break;
			case 'c':
				USB_PrintString_P(PSTR("> "));
				USB_PrintString_P(PMTK_CMD_COLD_START);
				USB_PrintString_P(PSTR(" (cold start)\r\n"));
				Adafruit_GPS_ColdStart(500);
				break;
			case 'b':
				voltage = ConvertBatteryVoltage(_gpsBattery);
				sprintf_P(_buffer, PSTR("> Battery voltage: %1.3f\r\n"), voltage);
				USB_PrintString(_buffer);
				break;
			case 'l':
				sprintf_P(_buffer, PSTR("> Calibration value: %d\r\n"), ADCA.CAL);
				USB_PrintString(_buffer);
				break;
			case 's':
				WaitForFixLineLow();
				USB_PrintString_P(PSTR("> "));
				USB_PrintString_P(PMTK_STANDBY);
				USB_PrintString_P(PSTR(" (suspending)\r\n"));
				Adafruit_GPS_Suspend(500);
				break;
			case 'w':
				USB_PrintString_P(PSTR("> ' ' (wakeup)\r\n"));
				Adafruit_GPS_Wakeup();
				break;
			case 'u':
				USB_PrintString_P(PSTR("> "));
				USB_PrintString_P(PMTK_SET_NMEA_UPDATE_1HZ);
				USB_PrintString_P(PSTR(" (1Hz output)\r\n"));
				Adafruit_GPS_Update1Hz(500);
				break;
			case 'e':
				USB_PrintString_P(PSTR("> Enabling GPS\r\n"));
				Adafruit_GPS_Enable();
				break;
			case 'd':
				USB_PrintString_P(PSTR("> Disabling GPS\r\n"));
				Adafruit_GPS_Disable();
				break;
			case 'v':
				USB_PrintString_P(PSTR("> "));
				USB_PrintString_P(PMTK_Q_RELEASE);
				USB_PrintString_P(PSTR(" (version query)\r\n"));
				if (Adafruit_GPS_AskForVersion(500, _gpsVersion, sizeof(_gpsVersion)))
				{
					USB_PrintString_P(PSTR("Version query success: "));
					USB_PrintString(_gpsVersion);
					USB_PrintString_P(PSTR("\r\n"));
				}
				else
				{
					strncpy_P(_gpsVersion, PSTR("NO VERSION FOUND"), sizeof(_gpsVersion));
					USB_PrintString_P(PSTR("Failed to fetch version information from module\r\n"));					
				}
				break;
			case 'o':
				USB_PrintString_P(PSTR("> "));
				USB_PrintString_P(PMTK_SET_NMEA_OUTPUT_OFF);
				USB_PrintString_P(PSTR(" (output off)\r\n"));
				Adafruit_GPS_OutputOff();
				break;
			case 'a':
				USB_PrintString_P(PSTR("> "));
				USB_PrintString_P(PMTK_SET_NMEA_OUTPUT_ALLDATA);
				USB_PrintString_P(PSTR(" (all data)\r\n"));
				Adafruit_GPS_AllData(500);
				break;
			case 'm':
				USB_PrintString_P(PSTR("> "));
				USB_PrintString_P(PMTK_SET_NMEA_OUTPUT_RMCONLY);
				USB_PrintString_P(PSTR(" (GPRMC only)\r\n"));
				Adafruit_GPS_RMCOnly(500);
				break;
			case 'r':
				USB_PrintString_P(PSTR("Resetting GPS state data\r\n"));
				Adafruit_GPS_ResetState();
				break;
			case 'x':
				USB_PrintString_P(PSTR("Exiting\r\n"));
				TurnOffDiagLed();
				return;
		}

		TurnOffDiagLed();
	}
}

/** Main program entry point. This routine contains the overall program flow, including initial
 *  setup of all components and the main program loop. */
int main(void)
{
	wdt_disable();

	SetupHardware();

	// Wait 5 seconds for USB to be available
	uint8_t timeout = 5;
	while(timeout > 0)
	{
		if (USB_IsHostConnected() && USB_IsHostConfigured())
			break;

		_delay_ms(1000);
		timeout--;
	}

	EnableGps();

	// issue greeting
	InitialGreeting();

	GpsMenu();

	//
	while(true)
	{
		if (_processGpsMessage)
		{
			//ProcessGpsMessage(_lastGpsMessage);
			DebugLogGpsMessages(_lastGpsMessage);
			_processGpsMessage = false;
			continue;
		}

		int data = USB_ReceiveByte();
		if (data == ' ')
		{
			GpsMenu();
		}

		// save the latest NMEA sentence
		if (_saveNmea)
		{
			if (SaveNMEA())
			{
				USB_PrintString_P(PSTR("  successfully saved nmea\r\n"));

				sprintf_P(_buffer, PSTR("  address after: %u\r\n"), _flashWriteAddr);
				USB_PrintString(_buffer);
			}
			else
			{
				USB_PrintString_P(PSTR("  failed to save\r\n"));
			}

			_saveNmea = false;
			_gpsFixTimeout = 0;
			_allowedMisses = 0;

			//EnterPowerSave();
		}

		// only main-loop code below this point
		if (!_secondsTick)
			continue;
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
						sprintf_P(_buffer, PSTR("Skipping fix timeout %i\r\n"), _allowedMisses);
						USB_PrintString(_buffer);
						_allowedMisses--;
					}
					else
					{
						// GPS fix timeout reached - suspend the module
						DisableGps();

						// turn on the fix error status LED
						TurnOnErrLed();

						USB_PrintString_P(PSTR("GPS failed to get a fix within the timeout period.\r\n"));
						USB_PrintString_P(PSTR("Disabling GPS.\r\n"));
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
			USB_PrintString_P(PSTR("Enabling GPS.\r\n"));

			// recording timeout is 0 - wakeup the GPS module
			_recordingTimeout = GPS_INTERVAL;
			_gpsFixTimeout = GPS_TIMEOUT;
			_allowedMisses = GPS_FIX_MISSES;
			EnableGps();

			// turn off the fix error status LED
			TurnOffErrLed();
			TurnOffInfoLed();
		}
	}
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

/** GPS Pin change interrupt handler */
ISR(PORTD_INT0_vect)
{
	// Toggled when PPS pin changes from H to L
	Adafruit_GPS_SetPpsState();

	// clear the interrupt flag
	GPS_port.INTFLAGS = PORT_INT0IF_bm;
}
