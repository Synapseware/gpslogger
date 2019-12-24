#include "FlashDriver.h"

using namespace std;


// Default constructor
FlashDriver::FlashDriver(uint8_t deviceAddress)
{
	ofstream image;
	image.open("./flash.bin", ios::binary);
	if (!image)
	{
		std::fill_n(std::ostreambuf_iterator<char>(image), FLASH_TOTAL_SIZE, 0xFF);
		image.flush();
	}
	image.close();
}

FlashDriver::~FlashDriver(void)
{
	if (_out.is_open())
		_out.close();
	if (_in.is_open())
		_in.close();
}



// ----------------------------------------------------------------------------
// STUBS
// ----------------------------------------------------------------------------


bool FlashDriver::isValid(void)
{
	ifstream check("./flash.bin");
	return check.tellg() == FLASH_TOTAL_SIZE;
}
void FlashDriver::suspend(void)
{

	cout << " suspending flash" << endl;
}
void FlashDriver::resume(void)
{

	cout << " resuming flash" << endl;
}
void FlashDriver::globalUnprotect(void)
{

	cout << " globalUnprotect" << endl;
}
void FlashDriver::globalProtect(void)
{

	cout << " globalProtect" << endl;
}
void FlashDriver::protectSector(uint8_t sector)
{

	cout << " protecting sector " << hex << sector << dec << endl;
}
void FlashDriver::unprotectSector(uint8_t sector)
{

	cout << " unprotecting sector " << hex << sector << dec << endl;
}
bool FlashDriver::isWriteEnabled(void)
{

	return true;
}
uint8_t FlashDriver::readStatusRegister(void)
{

	return 0;
}
void FlashDriver::setGlobalProtectState(uint8_t protect)
{

	cout << " Setting global protect: " << hex << protect << dec << endl;
}
bool FlashDriver::isDeviceReady(void)
{

	return true;
}

// ----------------------------------------------------------------------------
// I/O operations
// ----------------------------------------------------------------------------


void FlashDriver::beginWrite(uint32_t address)
{
	if (_out.is_open())
	{
		_out.flush();
		_out.close();
	}

	_out.open("./flash.bin", ios::in | ios::out | ios::binary);
	_out.seekp(address);

	_initialWriteAddress = _writeAddress = address;

	cout << "  begging write from " << hex << uppercase << address << dec << endl;
}

void FlashDriver::write(char data)
{
	_writeAddress++;
	_out.put(data);
	cout << hex << data << dec;
}
void FlashDriver::write(uint32_t address, const char* buffer, int count)
{
	beginWrite(address);

	_out.write(buffer, count);
	_writeAddress += count;

	endWrite();
}
void FlashDriver::writeString(uint32_t address, const char* buffer, int count)
{
	int length = strlen(buffer);
	if (length < count)
		count = length;
	write(address, buffer, count);
}

void FlashDriver::endWrite(void)
{
	cout << endl;
	cout << " wrote " << (_writeAddress - _initialWriteAddress) << " bytes" << endl;

	_out.flush();
	_out.close();
}


void FlashDriver::beginRead(uint32_t address)
{
	_in.open("./flash.bin");
	_in.seekg(address);

	_initialReadAddress = _readAddress = address;
	cout << "  begging read from " << hex << uppercase << address << dec << endl;
}

char FlashDriver::read(void)
{
	_readAddress++;
	return (char) _in.get();
}

int FlashDriver::read(uint32_t address, char* buffer, int count)
{
	beginRead(address);

	_in.read(buffer, count);
	_readAddress += count;

	endRead();

	return count;
}
int FlashDriver::readString(uint32_t address, char* buffer, int count)
{
	int length = read(address, buffer, count);
	char* pchr = (char*) memchr(buffer, 0xFF, length);
	*pchr = 0;
	return strlen(buffer);
}
void FlashDriver::endRead(void)
{
	_in.close();

	cout << endl;
	cout << "  read " << (_readAddress - _initialReadAddress) << " bytes " << endl;
}


