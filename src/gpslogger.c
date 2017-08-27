#include "gpslogger.h"


/** Lifecycle and USB flags */
volatile int8_t		_secondsTick		= 0;
volatile bool		_hostConnected		= false;
volatile bool		_hostConfigured		= false;
volatile int32_t	_flashWriteAddr		= 0;
volatile int		_gpsData			= -1;

volatile uint16_t	_gpsBattery			= 0;
volatile uint16_t	_adcaCal			= 0;

/** NMEA processing data & flags */
static bool			_hasFix				= false;
static bool			_saveNmea			= false;
static char			_nmea[256];
static char			_gpsVersion[96];
static int			_recordingTimeout	= 0;
static int			_gpsFixTimeout		= GPS_TIMEOUT;
static uint8_t		_allowedMisses		= GPS_FIX_MISSES;
static char			_buffer[256];

/** LCD variables */
static uint8_t		_lcdState			= 0;


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

static void PrintString_P(const char* message)
{
	if (!_hostConnected || NULL == message)
		return;

	//CDC_Device_SendString_P(&VirtualSerial_CDC_Interface, message);
	//CDC_Device_Flush(&VirtualSerial_CDC_Interface);
	fputs_P(message, &USBSerialStream);
}
static void PrintString(const char* message)
{
	if (!_hostConnected || NULL == message)
		return;

	//CDC_Device_SendString(&VirtualSerial_CDC_Interface, message);
	//CDC_Device_Flush(&VirtualSerial_CDC_Interface);
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
static void LCDPrintString_P(const char* str)
{
	if (TWI_StartTransmission(&LCD_TWI, LCD_TWI_ADDR | TWI_ADDRESS_WRITE, 100) == TWI_ERROR_NoError)
	{
		char nibble;
		while (true)
		{
			char data = pgm_read_byte(str++);
			if (!data)
				break;

			// high nibble first
			nibble =	(data & LCD_DATA_MASK) |
						(_lcdState & LCD_STATE_MASK) |
						LCD_WRITE_ENABLE;
			TWI_SendByte(&LCD_TWI, nibble);

			// low nibble last
			nibble =	(data << 4) |
						(_lcdState & LCD_STATE_MASK) |
						LCD_WRITE_ENABLE;
			TWI_SendByte(&LCD_TWI, nibble);
		}

		TWI_StopTransmission(&LCD_TWI);
	}
}

static uint8_t HD44780_ReadState(void)
{
	uint8_t state;
	if (TWI_StartTransmission(&LCD_TWI, LCD_TWI_ADDR | TWI_ADDRESS_READ, 100) == TWI_ERROR_NoError)
	{
		TWI_ReceiveByte(&LCD_TWI, &state, true);
		TWI_StopTransmission(&LCD_TWI);
	}

	_lcdState = state & LCD_STATE_MASK;

	return _lcdState;
}

static void HD44780_Clear(void)
{
	uint8_t data =	(CMD_DISPLAY_CLR << LCD_COMMAND_SHIFT) |
					LCD_WRITE_COMMAND |
					_lcdState;

	if (TWI_StartTransmission(&LCD_TWI, LCD_TWI_ADDR | TWI_ADDRESS_WRITE, 100) == TWI_ERROR_NoError)
	{
		TWI_SendByte(&LCD_TWI, data);
		TWI_StopTransmission(&LCD_TWI);
	}
}

static void HD44780_BacklightOn(void)
{
	if (_lcdState & LCD_BACKLIGHT)
	{
		// nothing to do - backlight is already on
		return;
	}

	// set the backlight flag
	_lcdState |= LCD_BACKLIGHT;

	if (TWI_StartTransmission(&LCD_TWI, LCD_TWI_ADDR | TWI_ADDRESS_WRITE, 100) == TWI_ERROR_NoError)
	{
		TWI_SendByte(&LCD_TWI, LCD_BACKLIGHT);
		TWI_StopTransmission(&LCD_TWI);
	}
}

static void HD44780_BacklightOff(void)
{
	if (!(_lcdState & LCD_BACKLIGHT))
	{
		// nothing to do - backlight is already on
		return;
	}

	// clear the backlight flag
	_lcdState &= ~LCD_BACKLIGHT;

	if (TWI_StartTransmission(&LCD_TWI, LCD_TWI_ADDR | TWI_ADDRESS_WRITE, 100) == TWI_ERROR_NoError)
	{
		TWI_SendByte(&LCD_TWI, 0);
		TWI_StopTransmission(&LCD_TWI);
	}
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

	// Configure TC1 for 20ms interrupts.  USB interractions
	// will fire from this handler
	{
		/* Select clock source */
		TC1_ConfigClockSource( &TCC1, TC_CLKSEL_DIV256_gc );

		/* F_CPU / 256 / 125 = 1000Hz (or 1ms) */
		TC_SetPeriod( &TCC1, 125 );

		/* Setup interrupt for compare A */
		TC1_SetCCAIntLevel( &TCC1, TC_CCAINTLVL_MED_gc );
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
	// I = 3.6/(47k*2)
	// I = 38.3uA

	// get the ADCA calibration data from ADCACAL0 and ADCACAL1
	// NVM_CMD_READ_CALIB_ROW_gc
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

/** Configure hardware to support the LCD */
static void InitializeLCD(void)
{
	ConfigureLCD();

	// Initialize the TWI interface
	TWI_Init(&LCD_TWI, TWI_BAUD_FROM_FREQ(LCD_BAUD));

	HD44780_ReadState();
	HD44780_Clear();
	HD44780_BacklightOn();

	LCDPrintString_P(PSTR("GPS Logger"));
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

	Adafruit_GPS_AskForVersion();

	//while ((Adafruit_GPS_CurrentState() & ADAFRUIT_GPS_FLAGS_VERSION) == 0);
}

/** Configures the board hardware and chip peripherals for the application's functionality. */
void SetupHardware(void)
{
	// initialize variables
	_secondsTick		= 0;
	_hostConnected		= false;
	_hostConfigured		= false;
	_flashWriteAddr		= 0;
	_recordingTimeout	= 0;
	_gpsData			= -1;
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

	// Initialize peripherals
	InitializeTimers();
	InitializeLCD();
	InitializeADC();
	InitializeSPI();

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
	char msg[64];

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
	PrintString_P(PSTR("Suspending GPS module.\r\n"));
	DisableGps();

	PrintString_P(PSTR("Saving NMEA sentence:\r\n"));
	PrintString(_nmea);

	PrintString_P(PSTR("  address before: "));
	sprintf_P(msg, PSTR("%u\r\n"), _flashWriteAddr);
	PrintString(msg);

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

/** Callback function which is invoked when a complete message is received
This method is run from the UART RX interrupt */
void HandleLatestGpsMessage(const char* message, int length)
{
	char cmd_buff[16];

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
	if (is_valid(SPI_CS))
		PrintString_P(PSTR("Y"));
	else
		PrintString_P(PSTR("N"));	
	PrintString_P(PSTR("\r\n"));

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

	/*
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
	*/

	PrintFlush();
}

/** Print GPS messages to console */
void LogGpsMessages(const char* message, int length)
{
	if (length < 1 || NULL == message)
		return;

	sprintf_P(_buffer, PSTR("< %s\r\n"), message);

	//CDC_Device_SendString(&VirtualSerial_CDC_Interface, _buffer);
	PrintString(_buffer);
	PrintFlush();
}

/** Displays a menu and allows the host to enter an interactive session
with the GPS module */
void GpsMenu(void)
{
	Adafruit_GPS_Init(&LogGpsMessages);

	WaitForFixLineLow();
	Adafruit_GPS_Suspend();

	PrintString_P(PSTR("GPS Interactive Menu\r\n"));
	PrintString_P(PSTR("H - Hot start GPS\r\n"));
	PrintString_P(PSTR("C - Cold start GPS\r\n"));
	PrintString_P(PSTR("B - Read battery voltage\r\n"));
	PrintString_P(PSTR("L - Display calibration data\r\n"));
	PrintString_P(PSTR("S - Suspend\r\n"));
	PrintString_P(PSTR("W - Wakeup\r\n"));
	PrintString_P(PSTR("U - Set update to 1Hz\r\n"));	
	PrintString_P(PSTR("V - Ask for version\r\n"));
	PrintString_P(PSTR("O - Output off\r\n"));
	PrintString_P(PSTR("A - All Data\r\n"));
	PrintString_P(PSTR("M - Minimum (GPRMC) logging only\r\n"));
	PrintString_P(PSTR("R - Reset state\r\n"));
	PrintString_P(PSTR("X - Exit\r\n"));
	PrintString_P(PSTR("\r\n"));
	PrintFlush();

	double voltage = 0.0;

	while (true)
	{
		int usb = 0;
		if (_hostConfigured && _hostConnected && CDC_Device_BytesReceived(&VirtualSerial_CDC_Interface) > 0)
		{
			usb = CDC_Device_ReceiveByte(&VirtualSerial_CDC_Interface);
		}
		else
		{
			continue;
		}

		TurnOnDiagLed();

		switch (usb | 0x40)
		{
			case 'h':
				PrintString_P(PSTR("> $PMTK101*32 (hot start)\r\n"));
				Adafruit_GPS_HotStart();
				break;
			case 'c':
				PrintString_P(PSTR("> $PMTK103*30 (cold start)\r\n"));
				Adafruit_GPS_ColdStart();
				break;
			case 'b':
				voltage = ConvertBatteryVoltage(_gpsBattery);
				sprintf_P(_buffer, PSTR("> Battery voltage: %1.3f\r\n"), voltage);
				PrintString(_buffer);
				break;
			case 'l':
				sprintf_P(_buffer, PSTR("> Calibration value: %d\r\n"), ADCA.CAL);
				PrintString(_buffer);
				break;
			case 's':
				PrintString_P(PSTR("> $PMTK161,0*28 (suspending)\r\n"));
				WaitForFixLineLow();
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
			case 'a':
				PrintString_P(PSTR("> $PMTK314,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0*28 (all data)\r\n"));
				Adafruit_GPS_AllData();
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

		usb = -1;
		PrintFlush();

		TurnOffDiagLed();
	}
}

/** Main program entry point. This routine contains the overall program flow, including initial
 *  setup of all components and the main program loop.
 */
int main(void)
{
	uint8_t delay = 1;

	SetupHardware();

	/* Create a regular character stream for the interface so that it can be used with the stdio.h functions */
	CDC_Device_CreateStream(&VirtualSerial_CDC_Interface, &USBSerialStream);

	// wait for host to connect
	while(!_hostConnected);

	// introduce a slight pause to allow
	// host to finalize connection
	_delay_ms(500);

	// issue greeting
	InitialGreeting();

	GpsMenu();

	while(true);

	//
	while(true)
	{
		int data = CDC_Device_ReceiveByte(&VirtualSerial_CDC_Interface);
		if (data == ' ')
		{
			GpsMenu();
		}

		// check for valid serial data
		if (_gpsData > -1)
		{
			//HandleGpsSerialInterrupt(_gpsData);
			//_gpsData = -1;
		}

		// save the latest NMEA sentence
		if (_saveNmea)
		{
			if (SaveNMEA())
			{
				PrintString_P(PSTR("  successfully saved nmea\r\n"));

				sprintf_P(_buffer, PSTR("  address after: %u\r\n"), _flashWriteAddr);
				PrintString(_buffer);
			}
			else
			{
				PrintString_P(PSTR("  failed to save\r\n"));
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
						PrintString(_buffer);
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
			TurnOffInfoLed();
		}
	}
}

/** USB Connect */
void EVENT_USB_Device_Connect(void)
{

	// No-Op
}

/** USB Disconnect */
void EVENT_USB_Device_Disconnect(void)
{

	// No-Op
}

/** Process control line state changes */
void EVENT_CDC_Device_ControLineStateChanged(USB_ClassInfo_CDC_Device_t* const CDCInterfaceInfo)
{
	uint8_t mask = (CDC_CONTROL_LINE_OUT_DTR | CDC_CONTROL_LINE_OUT_RTS);
	if (CDCInterfaceInfo->State.ControlLineStates.HostToDevice & mask)
	{
		_hostConnected = true;
	}
	else
	{
		_hostConnected = false;
	}
}

/** Event handler for the library USB Configuration Changed event. */
void EVENT_USB_Device_ConfigurationChanged(void)
{
	// configure the CDC interface
	_hostConfigured = CDC_Device_ConfigureEndpoints(&VirtualSerial_CDC_Interface);
	if (_hostConfigured)
	{
		// configuration successful
	}
}

/** Event handler for the library USB Control Request reception event. */
void EVENT_USB_Device_ControlRequest(void)
{
	// 
	CDC_Device_ProcessControlRequest(&VirtualSerial_CDC_Interface);
}

/** Interrupt handler for TCC0 Compare/Capture A */
ISR(TCC0_CCA_vect)
{
	_secondsTick = 1;

	// start an ADC conversion to read the GPS backup battery
	ADCA.CTRLA = ADC_ENABLE_bm | ADC_CH1START_bm;

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
	ERR_LED_port.OUTTGL = ERR_LED_pin;

	// USB tasks
	CDC_Device_USBTask(&VirtualSerial_CDC_Interface);
	USB_USBTask();

	// clear interrupt flag
	TCC1.INTFLAGS = TC1_CCAIF_bm;
}

/** GPS USART Receive interrupt handler */
ISR(GPS_UART_ISR)
{
	char data = GPS_UART.DATA;

	HandleGpsSerialInterrupt(data);
	//_gpsData = data;
}

/** ADC Channel 1 interrupt handler */
ISR(ADCA_CH1_vect)
{
	// read the battery value
	_gpsBattery = ADCA.CH1.RES;
}
