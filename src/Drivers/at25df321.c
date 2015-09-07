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
bool is_write_enabled(uint8_t chip_cs)
{
	// if bit 5 is clear, no error was detected so return true
	if ((read_status_register(chip_cs) & AT25DF321_SREGMSK_WEL) != 0)
		return true;

	return false;
}

// -------------------------------------------------------------------------------------------------------------------------
// returns a 1 if the device is ready for erase or program operations
bool device_is_ready(uint8_t chip_cs)
{
	// if bit 0 is cleared then the device is ready
	if ((read_status_register(chip_cs) & AT25DF321_SREGMSK_RDYBSY) == 0)
		return true;

	// device is not ready
	return false;
}

// -------------------------------------------------------------------------------------------------------------------------
// returns 1 if the device encountered an erase or program error
bool programming_error(uint8_t chip_cs)
{
	// if bit 5 is clear, no error was detected so return true
	if ((read_status_register(chip_cs) & AT25DF321_SREGMSK_EPE) != 0)
		return true;

	// return false to indicate an error
	return false;
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
bool is_valid(uint8_t chip_cs)
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
	// read current status
	uint8_t status = read_status_register(chip_cs);

	// clear the bits for global-unprotect
	status &= ~(AT25DF321_SREGMSK_EPE | AT25DF321_SREGMSK_WPP | AT25DF321_SREGMSK_SWP1 | AT25DF321_SREGMSK_SWP0);

	// enable writes
	write_enable(chip_cs);

	// issue global unprotect
	_spi_port->OUTCLR = chip_cs;
	SPI_SendByte(_spi_dev, AT25DF321_WRITE_STATUS);				// allow setting of the SPRL bit and the global protect/unprotect flags
	SPI_SendByte(_spi_dev, status);
	_spi_port->OUTSET = chip_cs;
}

// -------------------------------------------------------------------------------------------------------------------------
// Sets a sector as unprotected
void unprotect_sector(uint8_t chip_cs, uint8_t sector)
{
	write_enable(chip_cs);

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
	write_enable(chip_cs);

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
	write_enable(chip_cs);

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
void erase_block_32k(uint8_t chip_cs, uint32_t address)
{
	// 0x52
	erase_block(
		chip_cs,
		AT25DF321_BLOCK_ERASE_32k,
		address & AT25DF321_BLK_MASK_32K	// blank out the bottom 15 bits
	);
}

// -------------------------------------------------------------------------------------------------------------------------
// 
void erase_block_64k(uint8_t chip_cs, uint32_t address)
{
	// 0xD8
	erase_block(
		chip_cs,
		AT25DF321_BLOCK_ERASE_64k,
		address & AT25DF321_BLK_MASK_64K	// blank out the bottom 16 bits
	);
}

// -------------------------------------------------------------------------------------------------------------------------
// erases the complete chip
void erase_chip(uint8_t chip_cs)
{
	write_enable(chip_cs);

	_spi_port->OUTCLR = chip_cs;
	SPI_SendByte(_spi_dev, AT25DF321_CHIP_ERASE);
	_spi_port->OUTSET = chip_cs;
}

// -------------------------------------------------------------------------------------------------------------------------
void begin_read(uint8_t chip_cs, uint32_t address)
{
	_spi_port->OUTCLR = chip_cs;

	SPI_SendByte(_spi_dev, AT25DF321_READ_ARRAY_FAST);
	SPI_SendByte(_spi_dev, (address >> 16) & 0x3F);
	SPI_SendByte(_spi_dev, (address >> 8) & 0xFF);
	SPI_SendByte(_spi_dev, address & 0xFF);

	// since we are using 0x0B clock mode, write a dummy byte (per datasheet)
	SPI_SendByte(_spi_dev, 0);
}

// -------------------------------------------------------------------------------------------------------------------------
char read_byte(void)
{

	return SPI_ReceiveByte(_spi_dev);
}

// -------------------------------------------------------------------------------------------------------------------------
void end_read(uint8_t chip_cs)
{

	_spi_port->OUTSET = chip_cs;
}

// -------------------------------------------------------------------------------------------------------------------------
// reads 'count' bytes into 'pbuff' array and returns the number of bytes read
int read_block(uint8_t chip_cs, uint32_t address, char * pbuff, int count)
{
	begin_read(chip_cs, address);

	int index = 0;
	while (index < count)
	{
		// clock in the data from the chip
		pbuff[index++] = SPI_ReceiveByte(_spi_dev);
	}

	end_read(chip_cs);

	return index;
}

// -------------------------------------------------------------------------------------------------------------------------
// Sets up the flash for a write operation
void begin_write(uint8_t chip_cs, uint32_t address)
{
	write_enable(chip_cs);

	_spi_port->OUTCLR = chip_cs;

	address &= AT25DF321_ADDRESS_MASK;

	// send the address	
	_spi_port->OUTCLR = chip_cs;
	SPI_SendByte(_spi_dev, AT25DF321_BYTE_PAGE_PGM);
	SPI_SendByte(_spi_dev, (address >> 16) & 0x3F);
	SPI_SendByte(_spi_dev, (address >> 8) & 0xFF);
	SPI_SendByte(_spi_dev, address & 0xFF);
}

// -------------------------------------------------------------------------------------------------------------------------
// Send a byte to the flash as part of a begin-write-end transaction
void write_byte(char data)
{

	SPI_SendByte(_spi_dev, data);
}

// -------------------------------------------------------------------------------------------------------------------------
// Ends a write transaction
void end_write(uint8_t chip_cs)
{
	//
	_spi_port->OUTSET = chip_cs;
}

// -------------------------------------------------------------------------------------------------------------------------
// Writes a block of data to the chip.  Block can be 1 to 256 bytes wide.
void write_block(uint8_t chip_cs, uint32_t address, const char * pbuff, int count)
{
	begin_write(chip_cs, address);

	int index = 0;
	while(index < count)
	{
		SPI_SendByte(_spi_dev, pbuff[index++]);
	}

	end_write(chip_cs);
}
