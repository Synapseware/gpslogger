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


SPIClass::SPIClass(SPI_t* device)
{
	_spi = device;
}

uint8_t SPIClass::transfer(uint8_t data)
{
	_spi->DATA = data;
	asm volatile("nop");
	while(!(_spi->STATUS & SPI_IF_bm));
	return _spi->DATA;
}

// -------------------------------------------------------------------------------------------------------------------------
// Initializes the AT24DF321 flash driver
FlashDriver::FlashDriver(SPIClass* spi, SELECTDEVICEFUNC_t selCs)
{
	_spi = spi;
	_selCs = selCs;
	_mode = MODE_READ;
	_position = 0;
}

// -------------------------------------------------------------------------------------------------------------------------
// returns a 1 if the chip is an at25df321, 0 if not
bool FlashDriver::valid(void)
{
	open();

	uint8_t
		mfgid	= 0,
		devid1	= 0,
		extinfo	= 0;

	_spi->transfer(AT25DF321_READ_MFG_ID);
	mfgid	= _spi->transfer(0);
	devid1	= _spi->transfer(0);
	extinfo |= _spi->transfer(0);
	extinfo |= _spi->transfer(0);

	close();

	return mfgid == AT25DF321_MGF_ID &&
		devid1 == AT25DF321_DEVICDE_ID &&
		extinfo == 0;
}

// -------------------------------------------------------------------------------------------------------------------------
// returns true if the WEL of the status register is set
bool FlashDriver::enabled(void)
{
	// checks WEL bit of the status register
	return ((readStatusRegister() & AT25DF321_SREGMSK_WEL) != 0);
}

// -------------------------------------------------------------------------------------------------------------------------
// returns true if the WPP of the status register is set
bool FlashDriver::disabled(void)
{
	// check the WPP bit
	return ((readStatusRegister() & AT25DF321_SREGMSK_WPP) != 0);
}

// -------------------------------------------------------------------------------------------------------------------------
// returns true if the RDY/BSY bit is clear
bool FlashDriver::busy(void)
{
	// checks the RDY/BSY bit of the status register
	return ((readStatusRegister() & AT25DF321_SREGMSK_RDYBSY) != 0);
}

// -------------------------------------------------------------------------------------------------------------------------
// Enable writes by setting the write enable bit in the device control register
void FlashDriver::enable(void)
{
	// write 06h to the chip to enable the write mode
	writeSingle(AT25DF321_WRITE_ENABLE);
}

// -------------------------------------------------------------------------------------------------------------------------
// Disables writes by clearing the write enable bit in the device control register
void FlashDriver::disable(void)
{
	// write 04h to the chip to disable the write mode
	writeSingle(AT25DF321_WRITE_DISABLE);
}

// -------------------------------------------------------------------------------------------------------------------------
// returns true if the EPE bit of is set
bool FlashDriver::error(void)
{
	// if bit 5 is clear, no error was detected so return true
	return ((readStatusRegister() & AT25DF321_SREGMSK_EPE) != 0);
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
	enable();

	open();
	_spi->transfer(AT25DF321_UNPROTECT_SECTOR);
	_spi->transfer(sector & 0x3F);
	_spi->transfer(0);
	_spi->transfer(0);
	close();
}

// -------------------------------------------------------------------------------------------------------------------------
// Sets a sector as unprotected
void FlashDriver::protectSector(uint8_t sector)
{
	enable();

	open();
	_spi->transfer(AT25DF321_PROTECT_SECTOR);
	_spi->transfer(sector & 0x3F);
	_spi->transfer(0);
	_spi->transfer(0);
	close();
}

// -------------------------------------------------------------------------------------------------------------------------
// Erases a block of flash at the specified address, or the entire block.
// Note: Address is AND'd to align on 4k chunks
bool FlashDriver::erase(BLOCK_SIZE_t blockSize, uint32_t address)
{
	uint8_t mode = 0;
	switch (blockSize)
	{
		case BLOCK_SIZE_256:
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

	enable();

	// assert the CS pin again
	open();

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

	close();

	disable();

	return true;
}

// -------------------------------------------------------------------------------------------------------------------------
// Opens the device for a specific operation, and uses the current seek position
bool FlashDriver::open(MODE_t mode)
{
	// open the device and use the current position
	return open(mode, _position);
}

// -------------------------------------------------------------------------------------------------------------------------
// Opens an SPI transaction with the device for the given mode, and seeks to the given position
bool FlashDriver::open(MODE_t mode, uint32_t position)
{
	if (!(mode == MODE_READ || mode == MODE_WRITE))
		return false;

	// close any existing SPI transaction if we're switching modes
	if (mode != _mode)
	{
		close();

		// close any existing transaction
		while(!busy());
	}

	_mode = mode;
	_position = position;

	// seek into a specific position
	seek(_position);

	return true;
}

// -------------------------------------------------------------------------------------------------------------------------
// Releases the CS pin of the device
void FlashDriver::close(void)
{
	// terminate any previous operation
	_selCs(CS_MODE_DESELECT);
}

// -------------------------------------------------------------------------------------------------------------------------
// Sets up the device to read from a specific location
void FlashDriver::seek(uint32_t position)
{
	_position = position;

	if (_selCs(CS_MODE_READ) == 0)
	{
		// terminate any previous operation
		close();

		// wait for the device to become ready
		while(!busy());
	}

	// commence reading/writing from the current seek location
	begin();
}

// -------------------------------------------------------------------------------------------------------------------------
// Returns the current I/O position
uint32_t FlashDriver::position(void)
{

	return _position;
}

// -------------------------------------------------------------------------------------------------------------------------
// reads count bytes into the buffer and returns the number of bytes read
int FlashDriver::read(char* buffer, int count)
{
	if (_mode != MODE_READ)
		return -1;

	int index = 0;
	while (index < count)
	{
		// clock in the data from the chip
		buffer[index++] = _spi->transfer(0);
	}

	_position += count;

	return count;
}

// -------------------------------------------------------------------------------------------------------------------------
// Reads a byte of data from the current read location, and increments the read pointer
int FlashDriver::read(void)
{
	if (_mode != MODE_READ)
		return -1;

	_position++;

	// call the SPI read byte method
	return _spi->transfer(0);
}

// -------------------------------------------------------------------------------------------------------------------------
// Writes a string to the flash, stopping at the null terminator (or count).
int FlashDriver::write(const char* buffer, int count)
{
	if (_mode != MODE_WRITE)
		return -1;

	int index = 0;
	uint8_t page = _position & 0xFF;

	while (index < count)
	{
		_spi->transfer(buffer[index++]);
		_position++;
		page++;

		if (page != 0)
		{
			// close this SPI transaction
			close();

			// wait for programming to complete
			while (!busy());

			// initiate another write sequence
			seek(_position);
		}
	}

	return count;
}

// -------------------------------------------------------------------------------------------------------------------------
// Send a byte to the flash as part of a begin-write-end transaction
int FlashDriver::write(char data)
{
	if (_mode != MODE_WRITE)
		return -1;

	// caller has to call begin(address) first!
	_spi->transfer(data);
	_position++;

	if ((_position & 0x0000FF) == 0)
	{
		// end the SPI transaction
		close();

		// wait for the transaction to finish
		while(!busy());

		// start a new SPI transaction
		seek(_position);
	}

	return _position;
}


// -------------------------------------------------------------------------------------------------------------------------
// 
// Private methods
// 
// -------------------------------------------------------------------------------------------------------------------------


// -------------------------------------------------------------------------------------------------------------------------
// Sets up the device to read or write from the given address
void FlashDriver::begin(void)
{
	_position &= AT25DF321_ADDRESS_MASK;

	// send the address	
	uint8_t type = _mode == MODE_READ
		? AT25DF321_READ_ARRAY_FAST
		: AT25DF321_BYTE_PAGE_PGM;

	open();
	_spi->transfer(type);
	_spi->transfer((_position >> 16) & 0x3F);	// MSB address byte
	_spi->transfer((_position >> 8) & 0xFF);	// ---
	_spi->transfer(_position & 0xFF);			// LSB address byte

	// if reading (and using 0x0B command), clock out a dummy byte to start the read sequence
	if (_mode == MODE_READ)
	{
		_spi->transfer(0);
	}
}

// -------------------------------------------------------------------------------------------------------------------------
// Writes a single byte to the device via an atomic SPI transaction, used for sending commands
void FlashDriver::writeSingle(char data)
{
	open();
	_spi->transfer(data);
	close();
}

// -------------------------------------------------------------------------------------------------------------------------
// returns the status register bytes
uint8_t FlashDriver::readStatusRegister(void)
{
	open();
	_spi->transfer(AT25DF321_READ_STATUS);
	uint8_t status = _spi->transfer(0);
	close();

	return status;
}

// -------------------------------------------------------------------------------------------------------------------------
// 
void FlashDriver::setGlobalProtectState(uint8_t protect)
{
	// issue global unprotect
	open();
	_spi->transfer(AT25DF321_WRITE_STATUS);				// allow setting of the SPRL bit and the global protect/unprotect flags
	_spi->transfer(protect);
	close();
}

// -------------------------------------------------------------------------------------------------------------------------
// Asserts the devices CS pin
void FlashDriver::open(void)
{
	_selCs(CS_MODE_SELECT);
}
