#include "at25df321.h"


#ifdef __AVR_ATmega32U4__

	#define SPI_PORT		PORTB
	#define SPI_DDR			DDRB
	#define SPI_CS			PB0
	#define SPI_CS_bm		(1<<SPI_CS)
	#define SPI_SCK			PB1
	#define SPI_SCK_bm		(1<<SPI_SCK)
	#define SPI_MOSI		PB2
	#define SPI_MOSI_bm		(1<<SPI_MOSI)
	#define SPI_MISO		PB3
	#define SPI_MISO_bm		(1<<SPI_MISO)

#elif __AVR_ATxmega128A4U__

	// need to implement support for different MCU's
	#define SPI_PORT		PORT_t
	#define SPI_CS			PIN4_bm
	#define SPI_SCK			PIN7_bm
	#define SPI_MOSI		PIN5_bm
	#define SPI_MISO		PIN6_bm

#endif



// -------------------------------------------------------------------------------------------------------------------------
// Initializes the AT24DF321 flash driver
FlashDriver::FlashDriver(SPIClass* spi)
{
	_spi = spi;

	_seekg = 0;
	_seekp = 0;
}

// -------------------------------------------------------------------------------------------------------------------------
// returns true if the WEL of the status register is set
bool FlashDriver::isWriteEnabled(void)
{
	// checks WEL bit of the status register
	return ((readStatusRegister() & AT25DF321_SREGMSK_WEL) != 0);
}

// -------------------------------------------------------------------------------------------------------------------------
// returns true if the WPP of the status register is set
bool FlashDriver::isWriteProtected(void)
{
	// check the WPP bit
	return ((readStatusRegister() & AT25DF321_SREGMSK_WPP) != 0);
}

// -------------------------------------------------------------------------------------------------------------------------
// returns true if the RDY/BSY bit is clear
bool FlashDriver::isDeviceReady(void)
{
	// checks the RDY/BSY bit of the status register
	return ((readStatusRegister() & AT25DF321_SREGMSK_RDYBSY) == 0);
}

// -------------------------------------------------------------------------------------------------------------------------
// returns true if the EPE bit of is set
bool FlashDriver::programmingError(void)
{
	// if bit 5 is clear, no error was detected so return true
	return ((readStatusRegister() & AT25DF321_SREGMSK_EPE) != 0);
}

// -------------------------------------------------------------------------------------------------------------------------
// returns a 1 if the chip is an at25df321, 0 if not
bool FlashDriver::isValid(void)
{
	SPI_PORT &= ~(SPI_CS_bm);

	uint8_t
		mfgid	= 0,
		devid1	= 0,
		extinfo	= 0;

	_spi->transfer(AT25DF321_READ_MFG_ID);
	mfgid	= _spi->transfer(0);
	devid1	= _spi->transfer(0);
	extinfo |= _spi->transfer(0);
	extinfo |= _spi->transfer(0);

	SPI_PORT |= (SPI_CS_bm);

	return mfgid == AT25DF321_MGF_ID &&
		devid1 == AT25DF321_DEVICDE_ID &&
		extinfo == 0;
}

// -------------------------------------------------------------------------------------------------------------------------
// puts the device into deep-power down mode
void FlashDriver::suspend(void)
{
	// write 06h to the chip to enable the write mode
	writeSingle(AT25DF321_DEEP_PWRDN);
}

// -------------------------------------------------------------------------------------------------------------------------
// required to wake the device from a deep power down
void FlashDriver::resume(void)
{
	// write 06h to the chip to enable the write mode
	writeSingle(AT25DF321_RESUME);
}

// -------------------------------------------------------------------------------------------------------------------------
// Globally allow sector unprotection
void FlashDriver::globalUnprotect(void)
{
	// 
	setGlobalProtectState(AT25DF321_SREG_GLOBAL_UNPROTECT);
}

// -------------------------------------------------------------------------------------------------------------------------
// Set the global protect flags
void FlashDriver::globalProtect(void)
{
	// 
	setGlobalProtectState(AT25DF321_SREG_GLOBAL_PROTECT);
}

// -------------------------------------------------------------------------------------------------------------------------
// Sets a sector as unprotected
void FlashDriver::unprotectSector(uint8_t sector)
{
	writeEnable();

	SPI_PORT &= ~(SPI_CS_bm);
	_spi->transfer(AT25DF321_UNPROTECT_SECTOR);
	_spi->transfer(sector & 0x3F);
	_spi->transfer(0);
	_spi->transfer(0);
	SPI_PORT |= (SPI_CS_bm);
}

// -------------------------------------------------------------------------------------------------------------------------
// Sets a sector as unprotected
void FlashDriver::protectSector(uint8_t sector)
{
	writeEnable();

	SPI_PORT &= ~(SPI_CS_bm);
	_spi->transfer(AT25DF321_PROTECT_SECTOR);
	_spi->transfer(sector & 0x3F);
	_spi->transfer(0);
	_spi->transfer(0);
	SPI_PORT |= (SPI_CS_bm);
}

// -------------------------------------------------------------------------------------------------------------------------
// Erases a block of flash at the specified address, or the entire block.
// Note: Address is AND'd to align on 4k chunks
bool FlashDriver::erase(BLOCK_SIZE_t blockSize, uint32_t address)
{
	uint8_t mode = 0;
	switch (blockSize)
	{
		case AT25DF321_BLK_MASK_PAGE:
			address &= AT25DF321_BLK_MASK_PAGE;
			break;
		case BLOCK_SIZE_4k:
			mode = AT25DF321_BLOCK_ERASE_4K;
			address &= AT25DF321_BLK_MASK_4K;
			break;
		case BLOCK_SIZE_32k:
			mode = AT25DF321_BLOCK_ERASE_32k;
			address &= AT25DF321_BLK_MASK_32K;
			break;
		case BLOCK_SIZE_64k:
			mode = AT25DF321_BLOCK_ERASE_64k;
			address &= AT25DF321_BLK_MASK_64K;
			break;
		case BLOCK_SIZE_ALL:
			mode = AT25DF321_CHIP_ERASE;
			address = 0;
			break;
		default:
			return false;
	}

	writeEnable();

	// assert the CS pin again
	SPI_PORT &= ~(SPI_CS_bm);

	// send the mode byte, and optionall the address bytes if
	// not a whole-chip erase
	_spi->transfer(mode);
	if (mode != AT25DF321_CHIP_ERASE)
	{
		_spi->transfer((address >> 16) & 0x3F);
		_spi->transfer(address >> 8);
		_spi->transfer(address & 0xFF);
	}

	// clock out a pages worth of 0xFF's to erase it
	if (blockSize == AT25DF321_BLK_MASK_PAGE)
	{
		uint8_t i = 0;
		while (1)
		{
			_spi->transfer(0xFF);
			if (!i++)
				break;
		}
	}

	SPI_PORT |= (SPI_CS_bm);

	writeDisable();

	return true;
}


// -------------------------------------------------------------------------------------------------------------------------
// Asserts the devices CS pin
void FlashDriver::open(void)
{
	// initiate an SPI transaction by asserting the CS pin
	SPI_PORT &= ~(SPI_CS_bm);
}

// -------------------------------------------------------------------------------------------------------------------------
// Releases the CS pin of the device
void FlashDriver::close(void)
{
	// terminate any previous operation
	SPI_PORT |= (SPI_CS_bm);
}

// -------------------------------------------------------------------------------------------------------------------------
// Sets up the device to read from a specific location
void FlashDriver::seekg(uint32_t address)
{
	if ((SPI_PORT & ~(SPI_CS_bm)) == 0)
	{
		// terminate any previous operation
		close();

		// wait for the device to become ready
		while(!isDeviceReady());
	}

	_seekg = address;

	// begin an SPI transaction
	open();

	_spi->transfer(AT25DF321_READ_ARRAY_FAST);
	_spi->transfer((address >> 16) & 0x3F);	// MSB address byte
	_spi->transfer((address >> 8) & 0xFF);	// ---
	_spi->transfer(address & 0xFF);			// LSB address write

	// since we are using 0x0B clock mode, write a dummy byte (per datasheet)
	_spi->transfer(0);
}


// -------------------------------------------------------------------------------------------------------------------------
// Sets up the device to write to a specific location
void FlashDriver::seekp(uint32_t address)
{
	if ((SPI_PORT & ~(SPI_CS_bm)) == 0)
	{
		// terminate any previous operation
		close()

		// wait for the device to become ready
		while(!isDeviceReady());
	}

	writeEnable();

	_seekp = address;

	// send the address	
	SPI_PORT &= ~(SPI_CS_bm);
	_spi->transfer(AT25DF321_BYTE_PAGE_PGM);
	_spi->transfer((address >> 16) & 0x3F);	// MSB address byte
	_spi->transfer((address >> 8) & 0xFF);	// ---
	_spi->transfer(address & 0xFF);			// LSB address byte
}

// -------------------------------------------------------------------------------------------------------------------------
// Writes a string to the flash, stopping at the null terminator (or count).
void FlashDriver::write(const char* buffer, int count)
{
	int index = 0;
	uint8_t page = 0;
	uint8_t start = 256 - (_seekp % 256);

	while (index < count)
	{
		char data = buffer[index++];

		page++;
		if (page == 0)
		{
			endWrite();

			// make sure to complete a full page write before moving on,
			// so we don't lose data or overwrite existing data by causing
			// the devices internal buffer to overflow.
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

		_seekp++;
		_spi->transfer(data);
	}

	endWrite();
}

// -------------------------------------------------------------------------------------------------------------------------
// Send a byte to the flash as part of a begin-write-end transaction
void FlashDriver::write(char data)
{
	// caller has to call beginWrite(address) first!
	_spi->transfer(data);
	_seekp++;
}


// -------------------------------------------------------------------------------------------------------------------------
// Reads a byte of data from the current read location, and increments the read pointer
char FlashDriver::read(void)
{
	_seekg++;

	// call the SPI read byte method
	return (char) _spi->transfer(0);
}


// -------------------------------------------------------------------------------------------------------------------------
// reads count bytes into the buffer and returns the number of bytes read
int FlashDriver::read(char* buffer, int count)
{
	int index = 0;
	while (index < count)
	{
		// clock in the data from the chip
		buffer[index++] = _spi->transfer(0);
		_seekg++;
	}

	return index;
}


// -------------------------------------------------------------------------------------------------------------------------
// 
// Private methods
// 
// -------------------------------------------------------------------------------------------------------------------------


// -------------------------------------------------------------------------------------------------------------------------
// Sets up the flash for a write operation and leaves the CS line asserted
void FlashDriver::beginWrite(uint32_t address)
{
	writeEnable();

	SPI_PORT &= ~(SPI_CS_bm);

	address &= AT25DF321_ADDRESS_MASK;

	// send the address	
	SPI_PORT &= ~(SPI_CS_bm);
	_spi->transfer(AT25DF321_BYTE_PAGE_PGM);
	_spi->transfer((address >> 16) & 0x3F);	// MSB address byte
	_spi->transfer((address >> 8) & 0xFF);	// ---
	_spi->transfer(address & 0xFF);			// LSB address byte
}

// -------------------------------------------------------------------------------------------------------------------------
// Enable writes by setting the write enable bit in the device control register
void FlashDriver::writeEnable(void)
{
	// write 06h to the chip to enable the write mode
	writeSingle(AT25DF321_WRITE_ENABLE);
}

// -------------------------------------------------------------------------------------------------------------------------
// Disables writes by clearing the write enable bit in the device control register
void FlashDriver::writeDisable(void)
{
	// write 04h to the chip to disable the write mode
	writeSingle(AT25DF321_WRITE_DISABLE);
}

// -------------------------------------------------------------------------------------------------------------------------
// Ends a write transaction and de-asserts the CS line
void FlashDriver::endWrite(void)
{
	writeDisable();
	//
	SPI_PORT |= (SPI_CS_bm);
}

// -------------------------------------------------------------------------------------------------------------------------
// Writes a single byte to the device via an atomic SPI transaction
void FlashDriver::writeSingle(char data)
{
	SPI_PORT &= ~(SPI_CS_bm);
	_spi->transfer(data);
	SPI_PORT |= (SPI_CS_bm);
}

// -------------------------------------------------------------------------------------------------------------------------
// Initiates a read-array command and leaves the CS line asserted
void FlashDriver::beginRead(uint32_t address)
{
	SPI_PORT &= ~(SPI_CS_bm);

	_spi->transfer(AT25DF321_READ_ARRAY_FAST);
	_spi->transfer((address >> 16) & 0x3F);	// MSB address byte
	_spi->transfer((address >> 8) & 0xFF);	// ---
	_spi->transfer(address & 0xFF);			// LSB address write

	// since we are using 0x0B clock mode, write a dummy byte (per datasheet)
	_spi->transfer(0);
}

// -------------------------------------------------------------------------------------------------------------------------
// Ends the read operation by de-asserting the CS line
void FlashDriver::endRead(void)
{

	SPI_PORT |= (SPI_CS_bm);
}

// -------------------------------------------------------------------------------------------------------------------------
// returns the status register bytes
uint8_t FlashDriver::readStatusRegister(void)
{
	SPI_PORT &= ~(SPI_CS_bm);
	_spi->transfer(AT25DF321_READ_STATUS);
	uint8_t status = _spi->transfer(0);
	SPI_PORT |= (SPI_CS_bm);

	return status;
}

// -------------------------------------------------------------------------------------------------------------------------
// 
void FlashDriver::setGlobalProtectState(uint8_t protect)
{
	// enable writes
	writeEnable();

	// issue global unprotect
	SPI_PORT &= ~(SPI_CS_bm);
	_spi->transfer(AT25DF321_WRITE_STATUS);				// allow setting of the SPRL bit and the global protect/unprotect flags
	_spi->transfer(protect);
	SPI_PORT |= (SPI_CS_bm);
}
