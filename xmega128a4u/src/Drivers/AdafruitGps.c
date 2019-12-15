#include "AdafruitGps.h"



/** Create the buffer structure and its underlying storage array */
static char			_data[256];
static char			_version[96];
static int			_buffidx		= 0;

/* Flags for GPS state */
static flags_t		_gps_flags		= 0;

/** Message ready callback function */
Adafruit_GPS_OnMessageReceived_t _onMessageReady = NULL;


/** Resets the GPS USART receive buffer */
static void ResetReceiveBuffer(void)
{
	memset(_data, 0, sizeof(_data));
	memset(_version, 0, sizeof(_version));
	_buffidx = 0;
}


/** Initialize the Adafruit GPS library */
void Adafruit_GPS_Init(Adafruit_GPS_OnMessageReceived_t handler)
{
	_onMessageReady = handler;

	ResetReceiveBuffer();
	Adafruit_GPS_Disable();
}

/** Clears all internal state data */
void Adafruit_GPS_ResetState(void)
{
	// clear flags
	_gps_flags = 0;
}

/** Returns the current status flags value */
flags_t Adafruit_GPS_CurrentState(void)
{
	// return current flags
	return _gps_flags;
}

// Wait for a message with a timeout
bool Adafruit_GPS_WaitForStatus(char flag, int16_t timeoutMs)
{
	while ((Adafruit_GPS_CurrentState() & flag) == 0 && timeoutMs > 0)
	{
		_delay_ms(1);
		timeoutMs -= 1;
	}

	return (Adafruit_GPS_CurrentState() & flag) != 0;
}

bool Adafruit_GPS_WaitForPPS(int16_t timeoutMs)
{
	while ((Adafruit_GPS_CurrentState() & ADAFRUIT_GPS_FLAGS_PPS) == 0 && timeoutMs > 0)
	{
		_delay_ms(1);
		timeoutMs -= 1;
	}

	return (Adafruit_GPS_CurrentState() & ADAFRUIT_GPS_FLAGS_PPS) != 0;
}

/** Enable the GPS */
void Adafruit_GPS_Enable(void)
{
	Adafruit_GPS_ResetState();
	EnableGpsModule();
}

/** Disable the GPS */
void Adafruit_GPS_Disable(void)
{
	Adafruit_GPS_ResetState();
	DisableGpsModule();
}


/** Sends a command string to the GPS device */
static void Adafruit_GPS_SendCommand(const char* command)
{
	WriteToGpsUart_P(command);
	WriteToGpsUart_P(PSTR("\r\n"));
}

/** Suspend the GPS to save power */
void Adafruit_GPS_Suspend(int16_t timeout)
{
	WaitForFixLineLow();

	_gps_flags &= ~ADAFRUIT_GPS_FLAGS_STANDBYOK;

	// issue the PMTK suspend command message
	Adafruit_GPS_SendCommand(PMTK_STANDBY);
	if (!timeout)
		return;

	Adafruit_GPS_WaitForStatus(ADAFRUIT_GPS_FLAGS_STANDBYOK, timeout);
}

/** Wake the GPS from a previous suspend */
void Adafruit_GPS_Wakeup(void)
{
	_gps_flags &= ~ADAFRUIT_GPS_FLAGS_AWAKE;

	// wakeup the device by sending a single character
	WriteToGpsUart_P(PSTR(" "));
}

/**  */
void Adafruit_GPS_ColdStart(int16_t timeout)
{
	flags_t FLAGS = (ADAFRUIT_GPS_FLAGS_START |
					ADAFRUIT_GPS_FLAGS_START |
					ADAFRUIT_GPS_FLAGS_INIT_OK |
					ADAFRUIT_GPS_FLAGS_IDENT |
					ADAFRUIT_GPS_FLAGS_COMPLETE |
					ADAFRUIT_GPS_FLAGS_AWAKE);

	if ((_gps_flags & FLAGS) != 0)
		return;

	_gps_flags &= ~(FLAGS);

	// issue the cold start command
	Adafruit_GPS_SendCommand(PMTK_CMD_COLD_START);

	if (!timeout)
		return;

	// Wait for startup
	Adafruit_GPS_WaitForStatus(ADAFRUIT_GPS_FLAGS_START, timeout);
	Adafruit_GPS_WaitForStatus(ADAFRUIT_GPS_FLAGS_INIT_OK, timeout);
	Adafruit_GPS_WaitForStatus(ADAFRUIT_GPS_FLAGS_IDENT, timeout);
	Adafruit_GPS_WaitForStatus(ADAFRUIT_GPS_FLAGS_COMPLETE, timeout);
	Adafruit_GPS_WaitForStatus(ADAFRUIT_GPS_FLAGS_AWAKE, timeout);
}

/**  */
void Adafruit_GPS_WarmStart(void)
{
	_gps_flags &= ~ADAFRUIT_GPS_FLAGS_START;

	// issue the warm start command
	Adafruit_GPS_SendCommand(PMTK_CMD_WARM_START);
}

/**  */
void Adafruit_GPS_HotStart(void)
{
	_gps_flags &= ~ADAFRUIT_GPS_FLAGS_START;

	// issue the hot start command
	Adafruit_GPS_SendCommand(PMTK_CMD_HOT_START);
}

/**  */
void Adafruit_GPS_RMCOnly(int16_t timeout)
{
	_gps_flags &= ~ADAFRUIT_GPS_FLAGS_OUTPUT_CHG;

	// 
	Adafruit_GPS_SendCommand(PMTK_SET_NMEA_OUTPUT_RMCONLY);
	if (!timeout)
		return;

	Adafruit_GPS_WaitForStatus(ADAFRUIT_GPS_FLAGS_OUTPUT_CHG, timeout);
}

/**  */
void Adafruit_GPS_RMCGGADual(int16_t timeout)
{
	_gps_flags &= ~ADAFRUIT_GPS_FLAGS_OUTPUT_CHG;

	// 
	Adafruit_GPS_SendCommand(PMTK_SET_NMEA_OUTPUT_RMCGGA);
	if (!timeout)
		return;

	Adafruit_GPS_WaitForStatus(ADAFRUIT_GPS_FLAGS_OUTPUT_CHG, timeout);
}

/**  */
void Adafruit_GPS_AllData(int16_t timeout)
{
	_gps_flags &= ~ADAFRUIT_GPS_FLAGS_OUTPUT_CHG;

	// 
	Adafruit_GPS_SendCommand(PMTK_SET_NMEA_OUTPUT_ALLDATA);
	if (!timeout)
		return;

	Adafruit_GPS_WaitForStatus(ADAFRUIT_GPS_FLAGS_OUTPUT_CHG, timeout);
}

/**  */
void Adafruit_GPS_OutputOff(void)
{
	_gps_flags &= ~ADAFRUIT_GPS_FLAGS_OUTPUT_CHG;

	// 
	Adafruit_GPS_SendCommand(PMTK_SET_NMEA_OUTPUT_OFF);
}

/**  */
void Adafruit_GPS_Update1Hz(int16_t timeout)
{
	_gps_flags &= ~ADAFRUIT_GPS_FLAGS_OUTPUT_CHG;

	// issue the 1Hz update command
	Adafruit_GPS_SendCommand(PMTK_SET_NMEA_UPDATE_1HZ);
	if (!timeout)
		return;

	Adafruit_GPS_WaitForStatus(ADAFRUIT_GPS_FLAGS_OUTPUT_CHG, timeout);
}

/**  */
void Adafruit_GPS_Update5Hz(int16_t timeout)
{
	_gps_flags &= ~ADAFRUIT_GPS_FLAGS_OUTPUT_CHG;

	// issue the 5Hz update command
	Adafruit_GPS_SendCommand(PMTK_SET_NMEA_UPDATE_5HZ);
	if (!timeout)
		return;

	Adafruit_GPS_WaitForStatus(ADAFRUIT_GPS_FLAGS_OUTPUT_CHG, timeout);
}

/**  */
void Adafruit_GPS_Update10Hz(int16_t timeout)
{
	_gps_flags &= ~ADAFRUIT_GPS_FLAGS_OUTPUT_CHG;

	// issue the 10Hz update command
	Adafruit_GPS_SendCommand(PMTK_SET_NMEA_UPDATE_10HZ);
	if (!timeout)
		return;

	Adafruit_GPS_WaitForStatus(ADAFRUIT_GPS_FLAGS_OUTPUT_CHG, timeout);
}

/**  */
bool Adafruit_GPS_AskForVersion(int16_t timeout, char* message, uint16_t size)
{
	_gps_flags &= ~ADAFRUIT_GPS_FLAGS_VERSION;

	// Send the version 
	Adafruit_GPS_SendCommand(PMTK_Q_RELEASE);
	if (!timeout)
		return false;

	Adafruit_GPS_WaitForStatus(ADAFRUIT_GPS_FLAGS_VERSION, timeout);

	// Version response does not fall back to the client.  Copy it from the internal buffer
	strncpy(message, _data, size);

	return true;
}

/** Sets various state flags of the GPS  */
static bool ProcessGpsStateInfo(const char* message, int length)
{
	if (NULL == message || length < 2)
	{
		return false;
	}

	if (strncmp_P(message, PGACK_CONFIG_COMPLETE, strlen_P(PGACK_CONFIG_COMPLETE)) == 0)
	{
		_gps_flags |= ADAFRUIT_GPS_FLAGS_COMPLETE;
		return true;
	}

	if (strncmp_P(message, PGACK_COLD_STARTUP, strlen_P(PGACK_COLD_STARTUP)) == 0)
	{
		_gps_flags |= ADAFRUIT_GPS_FLAGS_START;
		return true;
	}

	if (strncmp_P(message, PGACK_CHIP_IDENT, strlen_P(PGACK_CHIP_IDENT)) == 0)
	{
		_gps_flags |= ADAFRUIT_GPS_FLAGS_IDENT;
		return true;
	}
	
	if (strncmp_P(message, PMTK_AWAKE, strlen_P(PMTK_AWAKE)) == 0)
	{
		_gps_flags |= ADAFRUIT_GPS_FLAGS_AWAKE;
		return true;
	}

	if (strncmp_P(message, PMTK_SET_NMEA_UPDATE_1HZ_OK, strlen_P(PMTK_SET_NMEA_UPDATE_1HZ_OK)) == 0)
	{
		_gps_flags |= ADAFRUIT_GPS_FLAGS_1HZ_OK;
		return true;
	}
	
	if (strncmp_P(message, PMTK_STANDBY_SUCCESS, strlen_P(PMTK_STANDBY_SUCCESS)) == 0)
	{
		_gps_flags |= ADAFRUIT_GPS_FLAGS_STANDBYOK;
		return true;
	}
	
	if (strncmp_P(message, PMTK_VERSION_HEADER, strlen_P(PMTK_VERSION_HEADER)) == 0)
	{
		_gps_flags |= ADAFRUIT_GPS_FLAGS_VERSION;
		return true;
	}
	
	if (strncmp_P(message, PMTK_SET_NMEA_OUTPUT_OK, strlen_P(PMTK_SET_NMEA_OUTPUT_OK)) == 0)
	{
		_gps_flags |= ADAFRUIT_GPS_FLAGS_OUTPUT_CHG;
		return true;
	}

	if (strncmp_P(message, PGACK_INIT_COMPLETE, strlen_P(PGACK_INIT_COMPLETE)) == 0)
	{
		_gps_flags |= ADAFRUIT_GPS_FLAGS_INIT_OK;
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
		case 0:
		case '\r':
			break;
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
				_onMessageReady(_data);
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

void Adafruit_GPS_SetPpsState(void)
{
	// Set the state flag
	_gps_flags |= ADAFRUIT_GPS_FLAGS_PPS;
}
