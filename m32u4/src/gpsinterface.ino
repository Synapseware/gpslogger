

static flags_t _flags;
static bool _hasMessage;

/*
static float _lastLat;
static float _lastLong;

struct NMEA_MSG
{
	bool Valid;
	float Latitude;
	float Longitude;
	float UtcTime;
	int UtcDate;
	char LatDir;
	char LatLong;
	float Speed;
	float Course;
};
*/

void configureGpsPins(void)
{
	// Set FIX and PPS pins as input with pull-ups enabled
	GPS_DDR &= ~(GPS_FIX_bm);
	GPS_PORT |= (GPS_FIX_bm);

	// Set EN pin as output and disable the GPS module
	GPS_DDR |= (GPS_EN_bm);
	GPS_PORT &= (GPS_EN_bm);
}

// ----------------------------------------------------------------------------
// 
bool hasMessage(void)
{
	return _hasMessage;
}


// ----------------------------------------------------------------------------
// Processes the current GPS sentence
bool processMessage(void)
{
	size_t length = strnlen(message, sizeof(message));
	if (length < 5 || length >= sizeof(message) - 1)
	{
		resetReceiveBuffer();
		return false;
	}

	if (processStateInfo())
	{
		resetReceiveBuffer();
		return false;
	}

	// Two GPRMC messages.  No fix, vs valid fix:
	// $GPRMC,183333.727,V,,,,,0.81,149.00,231219,,,N*49
	// $GPRMC,183334.727,A,4746.5339,N,12223.2518,W,2.26,119.01,231219,,,A*77
	//           UTC        Lat          Lon        Spd  Cog     Date    Chksm
	//	Position	47.775565°N
	//	122.387530°W
	//	Timestamp	Mon, 23 Dec 2019 18:33:34 UTC
	//	Movement	Speed 2.3 kts, heading 119°
	//	Close to	Woodway, United States
	//	Local time	Mon, 23 Dec 2019 10:33:34 PST
	//	Timezone	America/Los_Angeles (UTC -0800)

	// we would normally save the sentence at this point, if it was the RMC data
	if (strncmp_P(message, PSTR(PMTK_GPRMC_HEADER), strlen_P(PMTK_GPRMC_HEADER)) == 0)
	{
		// RMC data - write it to flash
		Serial.print(message);
		Serial.print(F("    Flags: "));
		Serial.println(_flags, BIN);
		_hasMessage = false;

		char* p = (char*) memchr(message, ',', sizeof(message));
		if (('A' == *p))
		{
			return true;
		}
	}

	return false;
}

/*
float getLatFromNmea(NMEA_MSG* nmea, const char* sentence)
{
	// http://aprs.gids.nl/nmea/
	for (uint8_t i = 0; i < 13; i++)
	{
		char* p = strtok(sentence, ',');
	}

	return 0.0;
}
*/

// ----------------------------------------------------------------------------
// Processes special NMEA sentences
bool processStateInfo(void)
{
	int length = strnlen(message, sizeof(message));
	if (length < 2)
	{
		return true;
	}

	// Don't process GPRMC data
	if (strncmp_P(message, PSTR(PMTK_GPRMC_HEADER), strlen_P(PSTR(PMTK_GPRMC_HEADER))) == 0)
	{
		return false;
	}

	// Don't process GPGGA data
	if (strncmp_P(message, PSTR(PMTK_GPGGA_HEADER), strlen_P(PSTR(PMTK_GPGGA_HEADER))) == 0)
	{
		return false;
	}

	if (strncmp_P(message, PSTR(PGACK_CONFIG_COMPLETE), strlen_P(PSTR(PGACK_CONFIG_COMPLETE))) == 0)
	{
		_flags |= FLAGS_COMPLETE;
		return true;
	}

	if (strncmp_P(message, PSTR(PGACK_COLD_STARTUP), strlen_P(PSTR(PGACK_COLD_STARTUP))) == 0)
	{
		_flags |= FLAGS_START;
		return true;
	}

	if (strncmp_P(message, PSTR(PGACK_CHIP_IDENT), strlen_P(PSTR(PGACK_CHIP_IDENT))) == 0)
	{
		_flags |= FLAGS_IDENT;
		return true;
	}
	
	if (strncmp_P(message, PSTR(PMTK_AWAKE), strlen_P(PSTR(PMTK_AWAKE))) == 0)
	{
		_flags |= FLAGS_AWAKE;
		return true;
	}

	if (strncmp_P(message, PSTR(PMTK_SET_NMEA_UPDATE_1HZ_OK), strlen_P(PSTR(PMTK_SET_NMEA_UPDATE_1HZ_OK))) == 0)
	{
		_flags |= FLAGS_1HZ_OK;
		return true;
	}
	
	if (strncmp_P(message, PSTR(PMTK_STANDBY_SUCCESS), strlen_P(PSTR(PMTK_STANDBY_SUCCESS))) == 0)
	{
		_flags |= FLAGS_STANDBYOK;
		return true;
	}
	
	if (strncmp_P(message, PSTR(PMTK_VERSION_HEADER), strlen_P(PSTR(PMTK_VERSION_HEADER))) == 0)
	{
		_flags |= FLAGS_VERSION;
		// save the version string
		memset(version, 0, sizeof(version));

		char undsc = 0;
		for (int i = 0; i<(int)sizeof(version); i++)
		{
			char data = message[i+(int)strlen_P(PSTR(PMTK_VERSION_HEADER))];
			if (data == '_')
			{
				if (undsc == 0)
				{
					data = '.';
					undsc++;
				}
				else
					break;
			}
			version[i] = data;
			if (data == 0)
				break;
		}

		return true;
	}
	
	if (strncmp_P(message, PSTR(PMTK_SET_NMEA_OUTPUT_OK), strlen_P(PSTR(PMTK_SET_NMEA_OUTPUT_OK))) == 0)
	{
		_flags |= FLAGS_OUTPUT_CHG;
		return true;
	}

	if (strncmp_P(message, PSTR(PGACK_INIT_COMPLETE), strlen_P(PSTR(PGACK_INIT_COMPLETE))) == 0)
	{
		_flags |= FLAGS_INIT_OK;
		return true;
	}

	return false;
}


// ----------------------------------------------------------------------------
// Checks flag against the current flag(s)
bool hasStatus(flags_t flag)
{

	return (_flags & flag) != 0;
}


// ----------------------------------------------------------------------------
// Get the current flags
flags_t currentFlags(void)
{

	return _flags;
}


// ----------------------------------------------------------------------------
// 
void resetReceiveBuffer(void)
{
	memset(message, 0, sizeof(message));
	messageIdx = 0;
	_hasMessage = false;
}


// ----------------------------------------------------------------------------
// 
bool writeData(char data)
{
	if (messageIdx >= (int)sizeof(message))
	{
		setErrLed();
		return false;
	}

	message[messageIdx++] = data;
	return true;
}


// ----------------------------------------------------------------------------
// Processes the next byte from the GPS
void processReceivedData(char data)
{
	switch (data)
	{
		// check for end-of-message character from the GPS
		case 0:
		case '\r':
			break;
		case '\n':
			writeData(0);

			// flag message ready
			_hasMessage = true;
			break;

		// check for start-of-message character from the GPS
		// and reset the buffer
		case '$':
			resetReceiveBuffer();
			writeData(data);
			break;

		// add the data to the buffer
		default:
			writeData(data);
			break;
	}
}

