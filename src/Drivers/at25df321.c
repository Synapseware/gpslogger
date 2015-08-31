#include "at25df321.h"


SPI_t*		_spi_dev;
PORT_t*		_spi_port;
uint8_t		_chip_cs;

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
void FlashDriver_Init(SPI_t* spi, PORT_t* spi_port, uint8_t cs)
{
	_spi_dev	= spi;
	_spi_port	= spi_port;
	_chip_cs	= cs;

	// enable output on the CS pin
	_spi_port->OUTSET = cs;
	_spi_port->DIRCLR = PIN6_bm;
	_spi_port->DIRSET = PIN7_bm | PIN5_bm | PIN4_bm;
}

// -------------------------------------------------------------------------------------------------------------------------
// reads the status register byte and returns it
uint8_t read_status_register(uint8_t chip_cs)
{
	_spi_port->OUTCLR = chip_cs;
	SPI_SendByte(_spi_dev, AT25DF321_READ_STATUS);
	uint8_t status = SPI_ReceiveByte(_spi_dev);
	_spi_port->OUTSET = chip_cs;

	return status;
}

// -------------------------------------------------------------------------------------------------------------------------
// returns a 1 if the device is ready for erase or program operations
uint8_t device_is_ready(uint8_t chip_cs)
{
	// if bit 0 is cleared then the device is ready
	if ((read_status_register(chip_cs) & AT25DF321_SREGMSK_RDYBSY) == 0)
		return 1;

	// device is not ready
	return 0;
}

// -------------------------------------------------------------------------------------------------------------------------
// returns 1 if the device encountered an erase or program error
uint8_t write_succeeded(uint8_t chip_cs)
{
	// if bit 5 is clear, no error was detected so return true
	if ((read_status_register(chip_cs) & AT25DF321_SREGMSK_EPE) == 0)
		return 1;

	// return false to indicate an error
	return 0;
}

// -------------------------------------------------------------------------------------------------------------------------
// Enable writes by setting the write enable bit
void write_enable(uint8_t chip_cs)
{
	// write 06h to the chip to enable the write mode
	_spi_port->OUTCLR = chip_cs;
	SPI_SendByte(_spi_dev, AT25DF321_WRITE_ENABLE);
	_spi_port->OUTSET = chip_cs;
}

// -------------------------------------------------------------------------------------------------------------------------
// Disable writes by clearing the write enable bit
void write_disable(uint8_t chip_cs)
{
	// write 04h to the chip to disable the write mode
	_spi_port->OUTCLR = chip_cs;
	SPI_SendByte(_spi_dev, AT25DF321_WRITE_DISABLE);
	_spi_port->OUTSET = chip_cs;
}

// -------------------------------------------------------------------------------------------------------------------------
// returns a 1 if the chip is an at25df321, 0 if not
int8_t is_valid(uint8_t chip_cs)
{
	_spi_port->OUTCLR = chip_cs;

	uint8_t
		mfgid	= 0,
		devid1	= 0,
		extinfo	= 0;

	SPI_SendByte(_spi_dev, AT25DF321_READ_MFG_ID);
	mfgid	= SPI_ReceiveByte(_spi_dev);
	devid1	= SPI_ReceiveByte(_spi_dev);
	extinfo |= SPI_ReceiveByte(_spi_dev);
	extinfo |= SPI_ReceiveByte(_spi_dev);

	_spi_port->OUTSET = chip_cs;

	return mfgid == AT25DF321_MGF_ID &&
		devid1 == AT25DF321_DEVICDE_ID &&
		extinfo == 0;
}

// -------------------------------------------------------------------------------------------------------------------------
// Globally allow sector unprotection
void global_unprotect(uint8_t chip_cs)
{
	_spi_port->OUTCLR = chip_cs;
	SPI_SendByte(_spi_dev, AT25DF321_WRITE_STATUS);				// allow setting of the SPRL bit and the global protect/unprotect flags
	SPI_SendByte(
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
	SPI_SendByte(_spi_dev, AT25DF321_UNPROTECT_SECTOR);
	SPI_SendByte(_spi_dev, sector & 0x3F);
	SPI_SendByte(_spi_dev, 0);
	SPI_SendByte(_spi_dev, 0);
	_spi_port->OUTSET = chip_cs;
}

// -------------------------------------------------------------------------------------------------------------------------
// Sets a sector as unprotected
void protect_sector(uint8_t chip_cs, uint8_t sector)
{
	_spi_port->OUTCLR = chip_cs;
	SPI_SendByte(_spi_dev, AT25DF321_PROTECT_SECTOR);
	SPI_SendByte(_spi_dev, sector & 0x3F);
	SPI_SendByte(_spi_dev, 0);
	SPI_SendByte(_spi_dev, 0);
	_spi_port->OUTSET = chip_cs;
}

// -------------------------------------------------------------------------------------------------------------------------
// helper function which performs the actual erase operation
void erase_block(uint8_t chip_cs, uint8_t mode, uint32_t address)
{
	_spi_port->OUTCLR = chip_cs;

	SPI_SendByte(_spi_dev, mode);
	SPI_SendByte(_spi_dev, (address >> 16) & 0x3F);
	SPI_SendByte(_spi_dev, address >> 8);
	SPI_SendByte(_spi_dev, address & 0xFF);

	_spi_port->OUTSET = chip_cs;
}

// -------------------------------------------------------------------------------------------------------------------------
// 
void erase_block_4k(uint8_t chip_cs, uint32_t address)
{
	// 0x20
	erase_block(
		chip_cs,
		AT25DF321_BLOCK_ERASE_4K,
		address & AT25DF321_BLK_MASK_4K		// blank out the bottom 12 bits
	);
}

// -------------------------------------------------------------------------------------------------------------------------
// 
int erase_block_32k(uint8_t chip_cs, uint32_t address)
{
	// 0x52
	erase_block(
		chip_cs,
		AT25DF321_BLOCK_ERASE_32k,
		address & AT25DF321_BLK_MASK_32K	// blank out the bottom 15 bits
	);

	return write_succeeded(chip_cs);
}

// -------------------------------------------------------------------------------------------------------------------------
// 
int erase_block_64k(uint8_t chip_cs, uint32_t address)
{
	// 0xD8
	erase_block(
		chip_cs,
		AT25DF321_BLOCK_ERASE_64k,
		address & AT25DF321_BLK_MASK_64K	// blank out the bottom 16 bits
	);

	return write_succeeded(chip_cs);
}

// -------------------------------------------------------------------------------------------------------------------------
// erases the complete chip
int erase_chip(uint8_t chip_cs)
{
	_spi_port->OUTCLR = chip_cs;
	SPI_SendByte(_spi_dev, AT25DF321_CHIP_ERASE);
	_spi_port->OUTSET = chip_cs;

	return write_succeeded(chip_cs);
}

// -------------------------------------------------------------------------------------------------------------------------
// reads 'count' bytes into 'pbuff' array and returns the number of bytes read
int read_block(uint8_t chip_cs, uint32_t address, int count, char * pbuff)
{
	_spi_port->OUTCLR = chip_cs;

	SPI_SendByte(_spi_dev, AT25DF321_READ_ARRAY);	// high-speed SPI clock because it's good for all clock values
	SPI_SendByte(_spi_dev, (address >> 16) & 0x3F);
	SPI_SendByte(_spi_dev, (address >> 8) & 0xFF);
	SPI_SendByte(_spi_dev, address & 0xFF);
	SPI_SendByte(_spi_dev, 0);	// since we are using 0x0B clock mode, write a dummy byte (per datasheet)

	int index = 0;
	while (index < count)
	{
		// clock in the data from the chip
		pbuff[index++] = SPI_ReceiveByte(_spi_dev);
	}
	_spi_port->OUTSET = chip_cs;

	return index;
}

// -------------------------------------------------------------------------------------------------------------------------
// Writes a block of data to the chip.  Block can be 1 to 256 bytes wide.
int write_block(uint8_t chip_cs, uint32_t address, int count, const char * pbuff)
{
	//write_enable(chip_cs);

	_spi_port->OUTCLR = chip_cs;
	
	// issue the write enable command
	SPI_SendByte(_spi_dev, AT25DF321_WRITE_ENABLE);

	// send the address	
	SPI_SendByte(_spi_dev, AT25DF321_BYTE_PAGE_PGM);
	SPI_SendByte(_spi_dev, (address >> 16) & 0x3F);
	SPI_SendByte(_spi_dev, (address >> 8) & 0xFF);
	SPI_SendByte(_spi_dev, address & 0xff);

	int index = 0;
	while(index < count)
	{
		SPI_SendByte(_spi_dev, pbuff[index++]);
	}

	_spi_port->OUTSET = chip_cs;

	return write_succeeded(chip_cs);
}
