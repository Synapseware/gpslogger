#include "gpslogger.h"

	#define MESSAGE_SIZE		120


	/** Lifecycle and USB flags */
	volatile uint32_t	_flashWriteAddr		= 0;
	volatile int		_gpsData			= -1;
	volatile int8_t		_secondsTick		= 0;

	volatile uint16_t	_gpsBattery			= 0;
	volatile uint16_t	_adcaCal			= 0;

	/** NMEA processing data & flags */
	static bool			_hasFix				= false;
	static bool			_saveNmea			= false;
	static bool			_displayMessage		= false;
	static bool			_processGpsMessage	= false;
	static char			_nmea[MESSAGE_SIZE];
	static char			_gpsVersion[96];
	static int			_recordingTimeout	= 0;
	static int			_gpsFixTimeout		= GPS_TIMEOUT;
	static uint8_t		_allowedMisses		= GPS_FIX_MISSES;
	static char 		_lastGpsMessage[MESSAGE_SIZE];
	static char			_buffer[MESSAGE_SIZE];

	SPIClass Spi(&SPIC);
	FlashDriver Flash(&Spi, SelectOnboardFlashDevice);


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
		memset(_gpsVersion, 0, sizeof(_gpsVersion));

		TurnOnDiagLed();
		Adafruit_GPS_Enable();
		WaitForFixLineHigh();
		WaitForFixLineLow();
		Adafruit_GPS_Wakeup();
		Adafruit_GPS_Update1Hz(500);
		Adafruit_GPS_RMCOnly(500);

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

		size_t length = strlen(message);

		if (message[0] == '\r' || message[0] == '\n' || length < 5)
		{
			return;
		}

		USB_PrintString_P(PSTR("> "));
		USB_PrintString(message);
		USB_PrintString_P(PSTR("\r\n"));
	}


	/** Callback function which is invoked when a complete message is received
	This method is run from the UART RX interrupt
	GPS NMEA data: http://www.gpsinformation.org/dale/nmea.htm */
	void HandleLatestGpsMessage(const char* message)
	{
		strlcpy(_lastGpsMessage, message, sizeof(_lastGpsMessage));
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
		size_t sentenceLength = strlen(_nmea);
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
		Flash.open(MODE_WRITE, _flashWriteAddr);
		Flash.write(_nmea, sentenceLength);
		Flash.close();

		// wait for write to complete
		while(Flash.busy());

		TurnOffInfoLed();

		return true;
	}

	/** Prints an initial greeting to the USB port */
	static void InitialGreeting(void)
	{
		USB_PrintString_P(PSTR("GPS Logger v1.2 - 20191227\r\n"));
		USB_PrintString_P(PSTR(" Version: "));
		if (strlen(_gpsVersion) > 0)
			USB_PrintString(_gpsVersion);
		else
			USB_PrintString_P(PSTR("(unknown)"));
		USB_PrintString_P(PSTR("\r\n"));

		USB_PrintString_P(PSTR("Flash info:\r\n"));
		USB_PrintString_P(PSTR("  Valid: "));
		if (Flash.valid())
			USB_PrintString_P(PSTR("Y"));
		else
			USB_PrintString_P(PSTR("N"));	
		USB_PrintString_P(PSTR("\r\n"));

		{
			/*
			TurnOnErrLed();

			global_unprotect(SPI_CS);

			USB_PrintString_P(PSTR("  Formatting Flash...\r\n"));
			erase_block_4k(SPI_CS, 0);
			while(is_write_enabled(SPI_CS));

			USB_PrintString_P(PSTR("  Done!\r\n"));

			TurnOffErrLed();
			TurnOnInfoLed();

			while(1);
			*/
		}

		USB_PrintString_P(PSTR("  Searching for free start address...\r\n"));
		_flashWriteAddr = FindFirstFreeAddress(&Flash);
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
				//DebugLogGpsMessages(_lastGpsMessage);
				memset(_lastGpsMessage, 0, sizeof(_lastGpsMessage));
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
