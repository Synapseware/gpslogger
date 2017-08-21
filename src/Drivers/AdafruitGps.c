#include "AdafruitGps.h"



/** Create the buffer structure and its underlying storage array */
static char				_data[256];
static int				_buffidx		= 0;

/* Flags for GPS state */
static flags_t			_gps_flags		= 0;

/** Message ready callback function */
Adafruit_GPS_OnMessageReceived_t _onMessageReady = NULL;


/** Resets the GPS USART receive buffer */
static void ResetReceiveBuffer(void)
{
	memset(_data, 0, sizeof(_data));
	_buffidx = 0;
}


/** Initialize the Adafruit GPS library */
void Adafruit_GPS_Init(Adafruit_GPS_OnMessageReceived_t handler)
{
	_onMessageReady = handler;

	ResetReceiveBuffer();
	Adafruit_GPS_ResetState();	
	Adafruit_GPS_Enable();
}

/** Clears all internal state data */
void Adafruit_GPS_ResetState(void)
{

	_gps_flags = 0;
}

/** Returns the current status flags value */
flags_t Adafruit_GPS_CurrentState(void)
{

	return _gps_flags;
}

/** Enable the GPS */
void Adafruit_GPS_Enable(void)
{

	EnableGpsModule();
}

/** Disable the GPS */
void Adafruit_GPS_Disable(void)
{

	DisableGpsModule();
}

/** Sends a command string to the GPS device */
static void Adafruit_GPS_SendCommand(const char* command)
{
	WriteToGpsUart_P(command);
	WriteToGpsUart_P(PSTR("\r\n"));
}

/** Suspend the GPS to save power */
void Adafruit_GPS_Suspend(void)
{
	WaitForFixLineLow();

	_gps_flags &= ~ADAFRUIT_GPS_FLAGS_STANDBYOK;

	// issue the PMTK suspend command message
	Adafruit_GPS_SendCommand(PSTR(PMTK_STANDBY));
}

/** Wake the GPS from a previous suspend */
void Adafruit_GPS_Wakeup(void)
{
	_gps_flags &= ~ADAFRUIT_GPS_FLAGS_AWAKE;

	// wakeup the device by sending a single character
	WriteToGpsUart_P(PSTR(" "));
}

/**  */
void Adafruit_GPS_ColdStart(void)
{
	_gps_flags &= ~ADAFRUIT_GPS_FLAGS_START;

	// issue the cold start command
	Adafruit_GPS_SendCommand(PSTR(PMTK_CMD_COLD_START));
}

/**  */
void Adafruit_GPS_WarmStart(void)
{
	_gps_flags &= ~ADAFRUIT_GPS_FLAGS_START;

	// issue the warm start command
	Adafruit_GPS_SendCommand(PSTR(PMTK_CMD_WARM_START));
}

/**  */
void Adafruit_GPS_HotStart(void)
{
	_gps_flags &= ~ADAFRUIT_GPS_FLAGS_START;

	// issue the hot start command
	Adafruit_GPS_SendCommand(PSTR(PMTK_CMD_HOT_START));
}

/**  */
void Adafruit_GPS_RMCOnly(void)
{
	_gps_flags &= ~ADAFRUIT_GPS_FLAGS_OUTPUT_CHG;

	// 
	Adafruit_GPS_SendCommand(PSTR(PMTK_SET_NMEA_OUTPUT_RMCONLY));
}

/**  */
void Adafruit_GPS_RMCGGADual(void)
{
	_gps_flags &= ~ADAFRUIT_GPS_FLAGS_OUTPUT_CHG;

	// 
	Adafruit_GPS_SendCommand(PSTR(PMTK_SET_NMEA_OUTPUT_RMCGGA));
}

/**  */
void Adafruit_GPS_AllData(void)
{
	_gps_flags &= ~ADAFRUIT_GPS_FLAGS_OUTPUT_CHG;

	// 
	Adafruit_GPS_SendCommand(PSTR(PMTK_SET_NMEA_OUTPUT_ALLDATA));
}

/**  */
void Adafruit_GPS_OutputOff(void)
{
	_gps_flags &= ~ADAFRUIT_GPS_FLAGS_OUTPUT_CHG;

	// 
	Adafruit_GPS_SendCommand(PSTR(PMTK_SET_NMEA_OUTPUT_OFF));
}

/**  */
void Adafruit_GPS_Update1Hz(void)
{
	_gps_flags &= ~ADAFRUIT_GPS_FLAGS_OUTPUT_CHG;

	// issue the 1Hz update command
	Adafruit_GPS_SendCommand(PSTR(PMTK_SET_NMEA_UPDATE_1HZ));
}

/**  */
void Adafruit_GPS_Update5Hz(void)
{
	_gps_flags &= ~ADAFRUIT_GPS_FLAGS_OUTPUT_CHG;

	// issue the 5Hz update command
	Adafruit_GPS_SendCommand(PSTR(PMTK_SET_NMEA_UPDATE_5HZ));
}

/**  */
void Adafruit_GPS_Update10Hz(void)
{
	_gps_flags &= ~ADAFRUIT_GPS_FLAGS_OUTPUT_CHG;

	// issue the 10Hz update command
	Adafruit_GPS_SendCommand(PSTR(PMTK_SET_NMEA_UPDATE_10HZ));
}

/**  */
void Adafruit_GPS_AskForVersion(void)
{
	_gps_flags &= ~ADAFRUIT_GPS_FLAGS_VERSION;

	// Send the version 
	Adafruit_GPS_SendCommand(PSTR(PMTK_Q_RELEASE));
}

/** Sets various state flags of the GPS  */
static bool ProcessGpsStateInfo(const char* message, int length)
{
	if (NULL == message || length < 2)
	{
		return false;
	}

	if (strncmp_P(message, PSTR(PGACK_CONFIG_COMPLETE), 15) == 0)			// 0
	{
		_gps_flags |= ADAFRUIT_GPS_FLAGS_COMPLETE;
		return true;
	}
	
	if (strncmp_P(message, PSTR(PGACK_COLD_STARTUP), 13) == 0)				// 1
	{
		_gps_flags |= ADAFRUIT_GPS_FLAGS_START;
		return true;
	}

	if (strncmp_P(message, PSTR(PGACK_CHIP_IDENT), 18) == 0)				// 2
	{
		_gps_flags |= ADAFRUIT_GPS_FLAGS_IDENT;
		return true;
	}
	
	if (strncmp_P(message, PSTR(PMTK_AWAKE), 15) == 0)						// 3
	{
		_gps_flags |= ADAFRUIT_GPS_FLAGS_AWAKE;
		return true;
	}

	if (strncmp_P(message, PSTR(PMTK_SET_NMEA_UPDATE_1HZ_OK), 17) == 0)		// 4
	{
		_gps_flags |= ADAFRUIT_GPS_FLAGS_1HZ_OK;
		return true;
	}
	
	if (strncmp_P(message, PSTR(PMTK_STANDBY_SUCCESS), 17) == 0)			// 5
	{
		_gps_flags |= ADAFRUIT_GPS_FLAGS_STANDBYOK;
		return true;
	}
	
	if (strncmp_P(message, PSTR(PMTK_VERSION_HEADER), 12) == 0)				// 6
	{
		_gps_flags |= ADAFRUIT_GPS_FLAGS_VERSION;
		return true;
	}
	
	if (strncmp_P(message, PSTR(PMTK_SET_NMEA_OUTPUT_OK), 17) == 0)			// 7
	{
		_gps_flags |= ADAFRUIT_GPS_FLAGS_OUTPUT_CHG;
		return true;
	}

	return false;
}

/** Writes a byte of data to the internal buffer */
static void writeData(char data)
{
	if (_buffidx < sizeof(_data) - 1)
	{
		_data[_buffidx++] = data;
	}
	else
	{
		_data[_buffidx] = 0;
	}
}

/** Callback for the GPS USART receive interrupt */
void HandleGpsSerialInterrupt(const char data)
{
	switch (data)
	{
		// check for end-of-message character from the GPS
		case '\n':
			writeData(data);
			writeData(0);

			// process GPS state data
			if (ProcessGpsStateInfo(_data, _buffidx))
			{
				ResetReceiveBuffer();
				break;
			}

			// Message was not state data - process
			if (NULL != _onMessageReady)
			{
				_onMessageReady(_data, _buffidx);
				ResetReceiveBuffer();
			}

			break;

		// check for start-of-message character from the GPS
		// and reset the buffer
		case '$':
			ResetReceiveBuffer();

		// add the data to the buffer
		default:
			writeData(data);
			break;
	}
}
