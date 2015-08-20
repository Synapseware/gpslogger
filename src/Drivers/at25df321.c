#include "at25df321.h"


volatile	SPI_t*		_spi_dev;
volatile	PORT_t*		_spi_port;
volatile	uint8_t		_chip_cs;

/*
	uint8_t read_status_register(void);
	uint8_t device_is_ready(void);
	uint8_t write_succeeded(void);
	void write_enable(void);

	void erase_block(uint8_t mode, uint32_t address);
	void erase_block_4k(uint32_t address);
	int erase_block_32k(uint32_t address);
	int erase_block_64k(uint32_t address);

	void read_page(uint32_t address);
*/

// -------------------------------------------------------------------------------------------------------------------------
// Initializes the AT24DF321 flash driver
void Initialize_FlashDriver(SPI_t* spi, PORT_t* spi_port, uint8_t cs)
{
	_spi_dev	= spi;
	_spi_port	= spi_port;
	_chip_cs	= cs;

	// enable output on the CS pin
	_spi_port->OUTSET = cs;
	_spi_port->DIR = cs;
}

// -------------------------------------------------------------------------------------------------------------------------
// reads the status register byte and returns it
uint8_t read_status_register(uint8_t chip_cs)
{
	_spi_port->OUTCLR = chip_cs;
	SPI_TransferByte(_spi_dev, 0x05);
	uint8_t status = SPI_TransferByte(_spi_dev, 0);	// write a dummy byte so we can clock in the data from the device
	_spi_port->OUTSET = chip_cs;

	return status;
}

// -------------------------------------------------------------------------------------------------------------------------
// returns a 1 if the device is ready for erase or program operations
uint8_t device_is_ready(uint8_t chip_cs)
{
	// if bit 0 is cleared then the device is ready
	if ((read_status_register(chip_cs) & 0x01) == 0)
		return 1;

	// device is not ready
	return 0;
}

// -------------------------------------------------------------------------------------------------------------------------
// returns 1 if the device encountered an erase or program error
uint8_t write_succeeded(uint8_t chip_cs)
{
	// if bit 5 is clear, no error was detected so return true
	if ((read_status_register(chip_cs) & (1<<5)) == 0)
		return 1;

	// return false to indicate an error
	return 0;
}

// -------------------------------------------------------------------------------------------------------------------------
// Enable writes by setting the write enable bit
inline void write_enable(uint8_t chip_cs)
{
	// write 06h to the chip to enable the write mode
	_spi_port->OUTCLR = chip_cs;
	SPI_TransferByte(_spi_dev, 0x06);
	_spi_port->OUTSET = chip_cs;
}

// -------------------------------------------------------------------------------------------------------------------------
// Disable writes by clearing the write enable bit
inline void write_disable(uint8_t chip_cs)
{
	// write 04h to the chip to disable the write mode
	_spi_port->OUTCLR = chip_cs;
	SPI_TransferByte(_spi_dev, 0x04);
	_spi_port->OUTSET = chip_cs;
}

// -------------------------------------------------------------------------------------------------------------------------
// returns a 1 if the chip is an at25df321, 0 if not
int8_t is_valid(uint8_t chip_cs)
{
	_spi_port->OUTCLR = chip_cs;

	uint8_t
		mfgid	= 0,
		devid1	= 0;

	SPI_TransferByte(_spi_dev, 0x9f);
	mfgid	= SPI_TransferByte(_spi_dev, 0);
	devid1	= SPI_TransferByte(_spi_dev, 0);
	SPI_TransferByte(_spi_dev, 0);
	SPI_TransferByte(_spi_dev, 0);

	_spi_port->OUTSET = chip_cs;

	return mfgid == 0x1F && devid1 == 0x47;
}

// -------------------------------------------------------------------------------------------------------------------------
// Globally allow sector unprotection
void global_unprotect(uint8_t chip_cs)
{
	_spi_port->OUTCLR = chip_cs;
	SPI_TransferByte(_spi_dev, 0x01);				// allow setting of the SPRL bit and the global protect/unprotect flags
	SPI_transferByte(
		_spi_dev,
		(0<<7) |					// SPRL bit.  0 = unlocked (default), 1 = locked
		(0<<5) |					// bits 5:2 must be cleared
		(0<<4) |
		(0<<3) |
		(0<<2)
	);
	_spi_port->OUTSET = chip_cs;
}

// -------------------------------------------------------------------------------------------------------------------------
// Sets a sector as unprotected
void unprotect_sector(uint8_t chip_cs, uint8_t sector)
{
	_spi_port->OUTCLR = chip_cs;
	SPI_TransferByte(_spi_dev, 0x39);
	SPI_TransferByte(_spi_dev, sector & 0x3F);
	SPI_TransferByte(_spi_dev, 0);
	SPI_TransferByte(_spi_dev, 0);
	_spi_port->OUTSET = chip_cs;
}

// -------------------------------------------------------------------------------------------------------------------------
// Sets a sector as unprotected
void protect_sector(uint8_t chip_cs, uint8_t sector)
{
	_spi_port->OUTCLR = chip_cs;
	SPI_TransferByte(_spi_dev, 0x36);
	SPI_TransferByte(_spi_dev, sector & 0x3F);
	SPI_TransferByte(_spi_dev, 0);
	SPI_TransferByte(_spi_dev, 0);
	_spi_port->OUTSET = chip_cs;
}

// -------------------------------------------------------------------------------------------------------------------------
// helper function which performs the actual erase operation
void erase_block(uint8_t chip_cs, uint8_t mode, uint32_t address)
{
	_spi_port->OUTCLR = chip_cs;

	SPI_TransferByte(_spi_dev, mode);
	SPI_TransferByte(_spi_dev, (address >> 16) & 0x3F);
	SPI_TransferByte(_spi_dev, address >> 8);
	SPI_TransferByte(_spi_dev, address & 0xFF);

	_spi_port->OUTSET = chip_cs;
}

// -------------------------------------------------------------------------------------------------------------------------
// 
void erase_block_4k(uint8_t chip_cs, uint32_t address)
{
	// 0x20
	erase_block(chip_cs, 0x20, address & 0xFFFFF000);	// blank out the bottom 12 bits
}

// -------------------------------------------------------------------------------------------------------------------------
// 
int erase_block_32k(uint8_t chip_cs, uint32_t address)
{
	// 0x52
	erase_block(chip_cs, 0x52, address & 0xFFFFF000);	// blank out the bottom 15 bits

	return write_succeeded(chip_cs);
}

// -------------------------------------------------------------------------------------------------------------------------
// 
int erase_block_64k(uint8_t chip_cs, uint32_t address)
{
	// 0xD8
	erase_block(chip_cs, 0xD8, address & 0xFFFF0000);	// blank out the bottom 16 bits

	return write_succeeded(chip_cs);
}

// -------------------------------------------------------------------------------------------------------------------------
// erases the complete chip
int erase_chip(uint8_t chip_cs)
{
	_spi_port->OUTCLR = chip_cs;
	SPI_TransferByte(_spi_dev, 0x60);
	_spi_port->OUTSET = chip_cs;

	return write_succeeded(chip_cs);
}

// -------------------------------------------------------------------------------------------------------------------------
// Writes a block of data to the chip.  Block can be 1 to 256 bytes wide.
int write_block(uint8_t chip_cs, uint32_t address, int count, const char * pbuff)
{
	_spi_port->OUTCLR = chip_cs;
	SPI_TransferByte(_spi_dev, 0x02);
	SPI_TransferByte(_spi_dev, (address >> 16) & 0x3F);
	SPI_TransferByte(_spi_dev, address >> 8);
	SPI_TransferByte(_spi_dev, address & 0xff);

	int index = 0;
	while(index < count)
	{
		SPI_TransferByte(_spi_dev, pbuff[index++]);
	}

	_spi_port->OUTSET = chip_cs;

	return write_succeeded(chip_cs);
}

// -------------------------------------------------------------------------------------------------------------------------
// reads 'count' bytes into 'pbuff' array and returns the number of bytes read
int read_block(uint8_t chip_cs, uint32_t address, int count, char * pbuff)
{
	_spi_port->OUTCLR = chip_cs;

	SPI_TransferByte(_spi_dev, 0x03);	// high-speed SPI clock because it's good for all clock values
	SPI_TransferByte(_spi_dev, (address >> 16) & 0x3F);
	SPI_TransferByte(_spi_dev, address >> 8);
	SPI_TransferByte(_spi_dev, address & 0xFF);
	//SPI_TransferByte(_spi_dev, 0);	// since we are using 0x0B clock mode, write a dummy byte (per datasheet)

	int index = 0;
	while (index < count)
	{
		// clock in the data from the chip
		pbuff[index++] = SPI_TransferByte(_spi_dev, 0);
	}
	_spi_port->OUTSET = chip_cs;

	return index;
}
