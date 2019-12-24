#include "flashtest.h"

using namespace std;


#define strnlen_P		strnlen
#define strlcpy_P		strlcpy
#define BUFFER_SIZE		120


FlashDriver* flash = new FlashDriver(0);


void setErrLed(void)
{
	cout << RED << "ERR LED ON" << RESET << endl;
}
void clearErrLed(void)
{
	cout << RED <<"ERR LED OFF" << RESET << endl;
}
void setDbgLed(void)
{
	cout << YELLOW << "Debug LED on" << RESET << endl;
}
void clearDbgLed(void)
{
	cout << YELLOW << "Debug LED off" << RESET << endl;
}


bool writeString(uint32_t address, const char* buffer, int count)
{
	if (count == 0 || strlen(buffer) == 0 || buffer == NULL)
		return false;

	int index = 0;
	uint8_t page = 0;
	uint8_t start = 256 - (address % 256);

	printf("\r\nStart offset is %i\r\n", (256- start));
	//cout << endl << "Start offset is " << start << endl;

	flash->beginWrite(address);

	while (index < count)
	{
		char data = buffer[index++];
		if (data == 0)
			break;

		page++;
		if (page == 0)
		{
			flash->endWrite();

			if (start != 0)
			{
				address += start;
				start = 0;
			}
			else
			{
				address += 256;
			}

			// wait for programming to complete
			while (!flash->isDeviceReady());

			flash->beginWrite(address);
		}

		// write data to SPI
	}

	flash->endWrite();

	cout << endl;

	return true;
}


/*
int isPageFree(uint32_t address)
{
	int freeFrom = 0;
	uint8_t i = 0;
	address &= AT25DF321_BLK_MASK_PAGE;
	flash->beginRead(address);
	while(1)
	{
		data = flash->read();
		if (data != 0xFF)
		{
			freeFrom = address + i;
			break;
		}

		i++;
		if (i == 0)
		{
			freeFrom = address;
			break;
		}
	}
	flash->endRead();

	return freeFrom;
}

// TODO:  Improve this so it does a b-search and finds the first free address within a page.
uint32_t findFirstFreeAddress(void)
{
	if (!flash->isDeviceReady())
	{
		setErrLed();

		// wait for ready state
		while (!flash->isDeviceReady());

		clearErrLed();
	}

	setDbgLed();

	uint32_t address = 0;
	uint32_t freefrom = 0;
	int matches = 0;

	// short-cut: if the first byte in flash is 0xFF, assume an empty device
	char data = flash->read(0);
	if (data == 0xFF)
		return 0;

	// start checking the first byte of each page.  If the page is free, check the whole page before it.
	while (address < FLASH_TOTAL_SIZE)
	{
		address += 256;
		data = flash->read(address);
		if (data != 0xFF)
			continue;

		// found a page that appears to be empty.
		if (isPageFree(address) == address)
		{
			// search the previous page for the first free position
			freefrom = isPageFree(address - 256);
			return freefrom;
		}
	}
}
*/

string loadLogData(string path)
{
	string contents;
	string line;
	ifstream log(path);
	if (log.is_open())
	{
		cout << " opened " << path << " for reading" << endl;
		while (getline(log, line))
		{
			contents += line + "\r\n";
		}

		log.close();
		cout << "  loaded " << contents.length() << " characters from file." << endl;
		return contents;
	}
	else
	{
		cout << "  failed to open log file " << path << endl;
		return NULL;
	}
}


void fillFlashImage(string path)
{
	ofstream image;
	image.open(path, ios::binary | ios::trunc);
	cout << " filling " << path << endl;
	std::fill_n(std::ostreambuf_iterator<char>(image), 4 * 1024 * 1024, 0xFF);
	image.flush();
	image.close();
}


// Generates a 4MB flash image file, optionally with data.
bool generateFlashImage(string path, const char* nmeaData)
{
	ofstream flash(path);
	if (flash.is_open())
	{
		for (int i = 0; i < 4 * 1024 * 1024; i++)
		{
			if (nmeaData != NULL && nmeaData[i] != 0)
			{
				flash.put(nmeaData[i]);
			}
			else
			{
				flash.put(0xFF);
			}
		}
		flash.close();
	}

	return true;
}

void writeDataToFlashAtIndex(string path, int index, const char* buffer)
{
	fstream image;
	image.open(path, ios::in | ios::out | ios::binary);
	if (image)
	{
		cout << GREEN << " " << path << " is good" << RESET << endl;
	}
	else
	{
		cout << RED << " " << path << " was not found!" << RESET << endl;
		fillFlashImage(path);
	}

	int ioIndex = index & 0x3FFF00;
	char existing[256];
	memset(existing, 0xFF, sizeof(existing));

	// copy the input buffer to the destination buffer
	strncpy(existing, buffer, sizeof(existing));

	image.seekp(ioIndex);
	image.write(existing, sizeof(existing));

	image.close();
}


int main(int argc, char* argv[])
{
	string logPath("./20150103.LOG");
	cout << endl;
	cout << "Loading NMEA log data from " << logPath << endl;

	wordexp_t p;
	string logData = loadLogData(logPath);
	cout << "  log data contains " << logData.length() << " characters" << endl;

	//writeString(256 * 23 + 103, logData.c_str(), logData.length());
	setErrLed();
	setDbgLed();
	clearErrLed();
	clearDbgLed();
	string flashImagePath("./FLASH.IMG");
	if (generateFlashImage(flashImagePath, logData.c_str()))
		cout << "generated binary flash image file (4MB)" << endl;
	else
	{
		cout << "failed to generate flash image file!" << endl;
		return -1;
	}

	fillFlashImage("./flash.bin");
	//writeDataToFlashAtIndex("./test-image.bin", 5 * 256, logData.c_str());
	//writeDataToFlashAtIndex("./test-image.bin", 1 * 256, logData.c_str());
	//writeDataToFlashAtIndex("./test-image.bin", 19 * 256, logData.c_str());
	//writeDataToFlashAtIndex("./test-image.bin", 27 * 256, logData.c_str());

	flash->write(2 * 256, logData.c_str(), logData.length());

	return 0;
}


