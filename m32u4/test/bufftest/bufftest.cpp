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


#define strnlen_P		strnlen
#define strlcpy_P		strlcpy
#define BUFFER_SIZE		120

int _txBufferIdx;
int _txBufferSize;
char _txBuffer[BUFFER_SIZE];


void resetBuffer(void)
{
	memset(_txBuffer, 0, BUFFER_SIZE);
	_txBufferIdx = -1;
	_txBufferSize = 0;
}


bool sendCommand(const char* command)
{	
	// if the buffer size isn't large enough to accomdate the command,
	// return a failure and reset
	int size = strnlen_P(command, BUFFER_SIZE);

	// Don't allow any more commands to be sent
	if (size + _txBufferSize > BUFFER_SIZE - 3)
	{
		return false;
	}

	// copy the command to the transmit buffer
	_txBufferSize += strlcpy_P(_txBuffer + _txBufferSize, command, BUFFER_SIZE);
	if (_txBufferSize > BUFFER_SIZE - 3)
	{
		_txBufferIdx = -1;
		_txBufferSize = 0;
		return false;
	}

	// Make sure the command is properly terminated
	if (command[size-2] != '\r' && command[size-1] != '\n')
	{
		_txBuffer[_txBufferSize++] = '\r';
		_txBuffer[_txBufferSize++] = '\n';
		_txBuffer[_txBufferSize] = 0;		// safety
	}

	return true;
}

void beginWrite(uint32_t address)
{
	cout << "Begin write at " << address;
}
void endWrite(void)
{
	cout << "  end write" << endl;
}

bool isDeviceReady(void)
{
	return true;
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

	beginWrite(address);

	while (index < count)
	{
		char data = buffer[index++];
		if (data == 0)
			break;

		page++;
		if (page == 0)
		{
			endWrite();

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
			while (!isDeviceReady());

			beginWrite(address);
		}

		// write data to SPI
	}

	endWrite();

	cout << endl;

	return true;
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
	resetBuffer();

	string rmcOnly("$PMTK314,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0*29");
	string setRate("$PMTK220,1000*1F");

	bool result = sendCommand(rmcOnly.c_str());
	result = sendCommand(setRate.c_str());

	cout << "Send command result: " << (result ? "true" : "false") << endl;
	cout << "Command buffer:" << endl;
	cout << "  Command size: " << rmcOnly.size() << endl;
	cout << "   Buffer size: " << _txBufferSize << endl;
	cout << "  Buffer index: " << _txBufferIdx << endl;
	cout << "-----------------------------------" << endl;
	cout << _txBuffer << endl;
	cout << "-----------------------------------" << endl;

	cout << endl << endl;
	cout << "Write string test" << endl;
	writeString(256 * 15, rmcOnly.c_str(), rmcOnly.length());

	string logPath("/Users/rmccallum/src/synapseware/gpslogger/m328p/data/LOGS/20150103.LOG");
	cout << endl;
	cout << "Loading NMEA log data from " << logPath << endl;

	wordexp_t p;
	string logData = loadLogData(logPath);
	cout << "  log data contains " << logData.length() << " characters" << endl;

	writeString(256 * 23 + 103, logData.c_str(), logData.length());

	return 0;
}
