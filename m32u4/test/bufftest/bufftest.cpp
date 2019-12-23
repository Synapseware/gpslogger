#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fstream>
#include <string.h>
#include <termios.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <wordexp.h>

using namespace std;

uint32_t _seekp = 0;

void beginWrite(uint32_t address)
{
	printf("\n  begin write: %#08x\n", address);
}

void close(void)
{
	printf("\n  close at: %#08x\n", _seekp);
}

bool isDeviceReady(void)
{
	return true;
}

void seekp(uint32_t address)
{
	_seekp = address;
	printf("  output seek to %#08x (%i)\n", address, address);
}

bool write(const char* buffer, int count)
{
	if (count == 0 || strlen(buffer) == 0 || buffer == NULL)
		return false;

	int index = 0;
	uint8_t page = _seekp & 0x0000FF;

	printf("  buffer length: %i\n", count);

	while (index < count)
	{
		// write data to SPI
		printf(" %#04x", buffer[index++]);
		_seekp++;
		page++;

		// check if we've crossed a page boundary
		if (page == 0)
		{
			close();

			// wait for programming to complete
			while (!isDeviceReady());

			// start a new write sequence
			beginWrite(_seekp);
		}
	}

	close();

	printf("\n  completed write at %#08x (%i)\n", _seekp, _seekp);;

	return true;
}

void write(char data)
{
	_seekp++;
	printf(" %#04x", data);

	// we've crossed a page boundary - start a new SPI transaction
	if ((_seekp & 0x0000FF) == 0)
	{
		close();

		while (!isDeviceReady());

		beginWrite(_seekp);
	}
}

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
			contents += line;
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


int main(int argc, char* argv[])
{
	string rmcOnly("$PMTK314,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0*29");
	string setRate("$PMTK220,1000*1F");

	cout << endl << endl;
	cout << "Write to Flash:" << endl;

	// seek to a location in flash
	const char* prmcOnly = rmcOnly.c_str();
	seekp(255);
	write(prmcOnly, rmcOnly.length());
	write("\r\n", 2);

	cout << endl << endl;
	cout << "Trying character writes..." << endl;

	seekp(1023);
	for (int i = 0; i < rmcOnly.length(); i++)
	{
		write(prmcOnly[i]);
	}
	close();

	cout << endl << "Done writing characters" << endl;

	/*
	string logPath("./20150103.LOG");
	cout << endl;
	cout << "Loading NMEA log data from " << logPath << endl;

	wordexp_t p;
	string logData = loadLogData(logPath);
	cout << "  log data contains " << logData.length() << " characters" << endl;

	// seek to a new record
	seekp(256 * 23 + 103);
	write(logData.c_str(), logData.length());
	*/
	return 0;
}
