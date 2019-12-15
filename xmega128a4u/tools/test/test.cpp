#include "test.h"


#define	FLASH_TOTAL_SIZE				4194304UL
#define	FLASH_PAGE_SIZE					256UL
#define FLASH_PAGE_MASK					255
#define	FLASH_PAGE_COUNT				(FLASH_TOTAL_SIZE / FLASH_PAGE_SIZE)
#define SPI_CS							3

// NMEA sentence types
#define PMTK_GPGGA_HEADER				"$GPGGA"
#define PMTK_GPRMC_HEADER				"$GPRMC"
#define PMTK_HEADER						"$PMTK"
#define PMTK_VERSION_HEADER				"$PMTK705,"

#define PSTR( val )						(val)
#define strlcpy_P(dst, src, st)			(strncpy(dst, src, st))
const int buff_size						= 16384;

static bool			_hasFix				= false;
static bool			_saveNmea			= false;
static bool			_recordPosition		= true;
static char			_nmea[256];
static char			_gpsVersion[96];
static int			_recordingTimeout	= 0;
static int			_gpsFixTimeout		= 90;
static int			_flashWriteAddr		= 0;


FILE * _flashIn = NULL;



void begin_write(char cs, int address)
{

	cout << "BEGIN WRITE to " << address << endl;
}


void write_byte(char data)
{
	switch (data)
	{
		case '\r':
			cout << "\\r";
			break;
		case '\n':
			cout << "\\n";
			break;
		case 0:
			cout << "\\0";
			break;
		default:
			cout << data;
			break;
	}
}


void end_write(char cs)
{
	cout << endl;
}


bool is_write_enabled(char cs)
{
	return false;
}

uint8_t device_is_ready(char cs)
{
	return true;
}

void begin_read(char cs, int address)
{
	cout << "BEGIN READ from " << address << endl;
	_flashIn = fopen(".flash", "r");
	fseek(_flashIn, address, SEEK_SET);
}

int read_byte(char cs)
{
	return fgetc(_flashIn);
}

void end_read(char cs)
{
	cout << "END READ" << endl;
	fclose(_flashIn);
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

		char* p = (char*)strchr(message, ',') + 1;

		/*
		float timef = atof(p);
		uint32_t time = timef;
		int hour = time / 10000;
		int minute = (time % 10000) / 100;
		int seconds = (time % 100);
		int milliseconds = fmod(timef, 1.0) * 1000;
		*/

		p = (char*)strchr(p, ',') + 1;

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
	while (address < FLASH_TOTAL_SIZE && address > -1)
	{
		// look for the first 0xFF followed by 0xFF's to the end
		int data = read_byte(SPI_CS);

		if (0xFF == data)
		{
			matches++;
			if (freefrom < 0)
				freefrom = address;
		}
		else if (-1 == data)
		{
			break;
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

	cout << "  address: " << address << endl;
	cout << "  freefrom: " << freefrom << endl;
	cout << "  matches: " << matches << endl;

	return freefrom;
}


void saveNmea(void)
{
	strcpy(_nmea, "$GPRMC,072450.000,A,4078.8163,N,10921.9609,W,0.14,355.98,170815,,,A*7A\r\n");

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

	cout << "NMEA sentence length: " << strlen(_nmea) << endl;
	cout << "Bytes written: " << _flashWriteAddr << endl;
}


void generateNewFlashImage(void)
{
	FILE * file = fopen(".flash", "w");
	for (int i = 0; i < FLASH_TOTAL_SIZE; i++)
	{
		fputc(0xFF, file);
	}
	fclose(file);
}


void findFirstFree(void)
{
	cout << "Finding first free address in FLASH" << endl;

	/*
	FILE * f = fopen(".flash", "r");
	int c;
	while ((c = fgetc(f)) > -1)
	{
		cout << (char) c;
	}
	cout << endl;
	fclose(f);
	*/

	int32_t addr = FindFirstFreeAddress();

	cout << "First address: " << addr << endl;
}


void generateNmeaFile(void)
{
	FILE * nmeaIn = fopen("stockholm_walk.nmea", "r");
	FILE * flash = fopen(".flash", "w");

	fseek(nmeaIn, 0, SEEK_END);   // non-portable
    long size = ftell(nmeaIn);
    fseek(nmeaIn, 0, SEEK_SET);

    cout << " flash: " << FLASH_TOTAL_SIZE << endl;
    cout << "  nmea: " << size << endl;
    cout << "  fill: " << (FLASH_TOTAL_SIZE - size) << endl;

	long address = 0;

	// populate flash file with NMEA data
	char * buff = (char*)malloc(buff_size * sizeof(char));
	while (true)
	{
		long len = fread(buff, 1, buff_size, nmeaIn);
		if (len < 1)
		{
			break;
		}
		fwrite(buff, 1, len, flash);

		address += len;
		cout << "Writing to " << address << endl;
	}
	free(buff);
	buff = NULL;

	cout << "Copied " << address << " bytes from NMEA source file." << endl;

	// fill flash with 0xFF's
	while (address < FLASH_TOTAL_SIZE)
	{
		fputc(0xFF, flash);
		address++;
	}

	cout << "Total flash size written: " << address << endl;

	fclose(nmeaIn);
	fclose(flash);
}


void staticVariableTest(void)
{
	static int foo = 0;
	int bar = 0;

	char msg[256];

	bar += 3;
	foo += 3;

	sprintf(msg, "foo: %-8i  bar: %-8i", foo, bar);

	cout << msg << endl;
}
void runStaticVariableTest(void)
{
	for (int i = 0; i < 25; i++)
	{
		staticVariableTest();
	}
}


void teststrcmp(void)
{
	char s1[16];
	char s2[16];

	sprintf(s1, "cats");
	sprintf(s2, "cats");
	cout << "s1 == s2? " << strcmp(s1, s2) << endl;

	sprintf(s1, "s1");
	sprintf(s2, "s2");
	cout << "s1 == s2? " << strcmp(s1, s2) << endl;

	s1[0] = 0;
	cout << "s1 == s2? " << strcmp(s1, s2) << endl;

	s2[0] = 0;
	cout << "s1 == s2? " << strcmp(s1, s2) << endl;

}


/** main */
int main(int argc, char* argv[])
{
	char *method = argv[1];

	for (int i = 1; i < argc; i++)
	{
		if (0 == strncmp("-m", argv[i], 2))
			method = argv[++i];
	}

	if (0 == strncmp("new-flash", method, 9))
	{
		generateNewFlashImage();
		return 0;
	}

	if (0 == strncmp("gennmea", method, 7))
	{
		generateNmeaFile();
		return 0;
	}

	if (0 == strncmp("savenmea", method, 8))
	{
		saveNmea();
		return 0;
	}

	if (0 == strncmp("firstfree", method, 9))
	{
		findFirstFree();
		return 0;
	}

	if (0 == strncmp("static", method, 5))
	{
		runStaticVariableTest();
		return 0;
	}

	if (0 == strncmp("strcmp", method, 6))
	{
		teststrcmp();
		return 0;
	}

	cout << "Unknown method: " << method << endl;
}
