#include "at25df321.h"


/* Enable C linkage for C++ Compilers: */
#if defined(__cplusplus)
	extern "C" {
#endif

	SPI_t*		_spi_dev;
	PORT_t*		_spi_port;


	// -------------------------------------------------------------------------------------------------------------------------
	// Initializes the AT24DF321 flash driver
	void FlashDriver_Init(SPI_t* spi, PORT_t* spi_port, uint8_t cs)
	{
		_spi_dev	= spi;
		_spi_port	= spi_port;

		// Make sure interrupts are disabled
		_spi_dev->INTCTRL = SPI_INTLVL_OFF_gc;

		uint8_t outpins = PIN4_bm | PIN5_bm | PIN7_bm;

		// setup pins
		_spi_port->OUTSET = cs;
		_spi_port->DIRCLR = PIN6_bm;
		_spi_port->DIRSET = outpins;
	}

	// -------------------------------------------------------------------------------------------------------------------------
	// returns the status register bytes
	static uint8_t read_status_register(uint8_t chip_cs)
	{
		_spi_port->OUTCLR = chip_cs;
		SPI_SendByte(_spi_dev, AT25DF321_READ_STATUS);
		uint8_t status = SPI_ReceiveByte(_spi_dev);
		_spi_port->OUTSET = chip_cs;

		return status;
	}

	// -------------------------------------------------------------------------------------------------------------------------
	// returns true if the WEL of the status register is set
	bool is_write_enabled(uint8_t chip_cs)
	{
		// checks WEL bit of the status register
		return ((read_status_register(chip_cs) & AT25DF321_SREGMSK_WEL) != 0);
	}

	// -------------------------------------------------------------------------------------------------------------------------
	// returns true if the WPP of the status register is set
	bool is_write_protected(uint8_t chip_cs)
	{
		// check the WPP bit
		return ((read_status_register(chip_cs) & AT25DF321_SREGMSK_WPP) != 0);
	}

	// -------------------------------------------------------------------------------------------------------------------------
	// returns true if the RDY/BSY bit is clear
	bool device_is_ready(uint8_t chip_cs)
	{
		// checks the RDY/BSY bit of the status register
		return ((read_status_register(chip_cs) & AT25DF321_SREGMSK_RDYBSY) == 0);
	}

	// -------------------------------------------------------------------------------------------------------------------------
	// returns true if the EPE bit of is set
	bool programming_error(uint8_t chip_cs)
	{
		// if bit 5 is clear, no error was detected so return true
		return ((read_status_register(chip_cs) & AT25DF321_SREGMSK_EPE) != 0);
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
		TurnOnDiagLed();
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

		TurnOffDiagLed();
		return mfgid == AT25DF321_MGF_ID &&
			devid1 == AT25DF321_DEVICDE_ID &&
			extinfo == 0;
	}

	// -------------------------------------------------------------------------------------------------------------------------
	// puts the device into deep-power down mode
	void power_down(uint8_t chip_cs)
	{
		// write 06h to the chip to enable the write mode
		_spi_port->OUTCLR = chip_cs;
		SPI_SendByte(_spi_dev, AT25DF321_DEEP_PWRDN);
		_spi_port->OUTSET = chip_cs;
	}

	// -------------------------------------------------------------------------------------------------------------------------
	// required to wake the device from a deep power down
	void resume(uint8_t chip_cs)
	{
		// write 06h to the chip to enable the write mode
		_spi_port->OUTCLR = chip_cs;
		SPI_SendByte(_spi_dev, AT25DF321_RESUME);
		_spi_port->OUTSET = chip_cs;
	}

	static void _set_global_protect_state(uint8_t chip_cs, uint8_t protect)
	{
		// enable writes
		write_enable(chip_cs);

		// issue global unprotect
		_spi_port->OUTCLR = chip_cs;
		SPI_SendByte(_spi_dev, AT25DF321_WRITE_STATUS);				// allow setting of the SPRL bit and the global protect/unprotect flags
		SPI_SendByte(_spi_dev, protect);
		_spi_port->OUTSET = chip_cs;
	}

	// -------------------------------------------------------------------------------------------------------------------------
	// Globally allow sector unprotection
	void global_unprotect(uint8_t chip_cs)
	{
		// 
		_set_global_protect_state(chip_cs, AT25DF321_SREG_GLOBAL_UNPROTECT);
	}

	// -------------------------------------------------------------------------------------------------------------------------
	// Set the global protect flags
	void global_protect(uint8_t chip_cs)
	{
		// 
		_set_global_protect_state(chip_cs, AT25DF321_SREG_GLOBAL_PROTECT);
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

#if defined(__cplusplus)
	}
#endif

