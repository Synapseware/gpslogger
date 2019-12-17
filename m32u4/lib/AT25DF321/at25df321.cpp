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
	#define SPI_SCK		PIN7_bm
	#define SPI_MOSI		PIN5_bm
	#define SPI_MISO		PIN6_bm

#endif



// -------------------------------------------------------------------------------------------------------------------------
// Initializes the AT24DF321 flash driver
FlashDriver::FlashDriver(uint8_t deviceAddress, SPISettings* spiSettings, SPIClass* spiClass)
{
	_deviceAddress = deviceAddress;
	_spiSettings = spiSettings;
	_spiClass = spiClass;

	// Setup GPIO pins
	SPI_DDR |= (SPI_CS_bm | SPI_SCK | SPI_SCK);
	SPI_MISO &= ~(SPI_MISO_bm);

	SPCR |= (0<<SPIE) | // 
			(1<<SPE)  | // 
			(0<<DORD) | // 
			(1<<MSTR) | // 
			(0<<CPOL) | // 
			(0<<CPHA) | //  
			(0<<SPR1) | // Fosc / 2
			(0<<SPR0);  // Fosc / 2

	SPSR |=	(1<<SPI2X); // Fosc / 2	

	//SPDR;
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
// Enable writes by setting the write enable bit
void FlashDriver::writeEnable(void)
{
	// write 06h to the chip to enable the write mode
	SPI_PORT &= ~(SPI_CS_bm);
	SPI_SendByte(AT25DF321_WRITE_ENABLE);
	SPI_PORT |= (SPI_CS_bm);
}

// -------------------------------------------------------------------------------------------------------------------------
// Disables writes by clearing the write enable bit
void FlashDriver::writeDisable(void)
{
	// write 04h to the chip to disable the write mode
	SPI_PORT &= ~(SPI_CS_bm);
	SPI_SendByte(AT25DF321_WRITE_DISABLE);
	SPI_PORT |= (SPI_CS_bm);
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

	SPI_SendByte(AT25DF321_READ_MFG_ID);
	mfgid	= SPI_ReceiveByte();
	devid1	= SPI_ReceiveByte();
	extinfo |= SPI_ReceiveByte();
	extinfo |= SPI_ReceiveByte();

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
	SPI_PORT &= ~(SPI_CS_bm);
	SPI_SendByte(AT25DF321_DEEP_PWRDN);
	SPI_PORT |= (SPI_CS_bm);
}

// -------------------------------------------------------------------------------------------------------------------------
// required to wake the device from a deep power down
void FlashDriver::resume(void)
{
	// write 06h to the chip to enable the write mode
	SPI_PORT &= ~(SPI_CS_bm);
	SPI_SendByte(AT25DF321_RESUME);
	SPI_PORT |= (SPI_CS_bm);
}

// -------------------------------------------------------------------------------------------------------------------------
// Globally allow sector unprotection
void FlashDriver::globalUnprotect(void)
{
	// 
	setGlobalProtectState( AT25DF321_SREG_GLOBAL_UNPROTECT);
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
	SPI_SendByte(AT25DF321_UNPROTECT_SECTOR);
	SPI_SendByte(sector & 0x3F);
	SPI_SendByte(0);
	SPI_SendByte(0);
	SPI_PORT |= (SPI_CS_bm);
}

// -------------------------------------------------------------------------------------------------------------------------
// Sets a sector as unprotected
void FlashDriver::protectSector(uint8_t sector)
{
	write_enable();

	SPI_PORT &= ~(SPI_CS_bm);
	SPI_SendByte(AT25DF321_PROTECT_SECTOR);
	SPI_SendByte(sector & 0x3F);
	SPI_SendByte(0);
	SPI_SendByte(0);
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
		case BLOCK_SIZE_4k:
			mode = AT25DF321_BLOCK_ERASE_4k;
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
	SPI_SendByte(mode);
	if (mode != AT25DF321_CHIP_ERASE)
	{
		SPI_SendByte((address >> 16) & 0x3F);
		SPI_SendByte(address >> 8);
		SPI_SendByte(address & 0xFF);
	}

	writeDisable();

	return true;
}
	


// -------------------------------------------------------------------------------------------------------------------------
// Reads a byte of data from the current read location, and increments the read pointer
char FlashDriver::read(void)
{
	// call the SPI read byte method
	return SPI_ReceiveByte();
}

// -------------------------------------------------------------------------------------------------------------------------
// reads 'count' bytes into 'pbuff' array and returns the number of bytes read
int FlashDriver::read(uint32_t address, char * pbuff, int count)
{
	beginRead(address);

	int index = 0;
	while (index < count)
	{
		// clock in the data from the chip
		pbuff[index++] = SPI_ReceiveByte();
	}

	end_read(chip_cs);

	return index;
}

// -------------------------------------------------------------------------------------------------------------------------
// Send a byte to the flash as part of a begin-write-end transaction
void FlashDriver::write(char data)
{

	SPI_SendByte(data);
}

// -------------------------------------------------------------------------------------------------------------------------
// Writes a block of data to the chip.  Block can be 1 to 256 bytes wide.
void FlashDriver::write(uint32_t address, const char * pbuff, int count)
{
	beginWrite(address);

	int index = 0;
	while(index < count)
	{
		SPI_SendByte(pbuff[index++]);
	}

	endWrite();
}


// -------------------------------------------------------------------------------------------------------------------------
// 
// Private methods
// 
// -------------------------------------------------------------------------------------------------------------------------


// -------------------------------------------------------------------------------------------------------------------------
void FlashDriver::beginRead(uint32_t address)
{
	SPI_PORT &= ~(SPI_CS_bm);

	SPI_SendByte(AT25DF321_READ_ARRAY_FAST);
	SPI_SendByte((address >> 16) & 0x3F);
	SPI_SendByte((address >> 8) & 0xFF);
	SPI_SendByte(address & 0xFF);

	// since we are using 0x0B clock mode, write a dummy byte (per datasheet)
	SPI_SendByte(0);
}

// -------------------------------------------------------------------------------------------------------------------------
void FlashDriver::endRead(void)
{

	SPI_PORT |= (SPI_CS_bm);
}


// -------------------------------------------------------------------------------------------------------------------------
// Enable writes by setting the write enable bit
void FlashDriver::writeEnable(void)
{
	// write 06h to the chip to enable the write mode
	SPI_PORT &= ~(SPI_CS_bm);
	SPI_SendByte(AT25DF321_WRITE_ENABLE);
	SPI_PORT |= (SPI_CS_bm);
}


// -------------------------------------------------------------------------------------------------------------------------
// Sets up the flash for a write operation
void FlashDriver::beginWrite(uint32_t address)
{
	writeEnable();

	SPI_PORT &= ~(SPI_CS_bm);

	address &= AT25DF321_ADDRESS_MASK;

	// send the address	
	SPI_PORT &= ~(SPI_CS_bm);
	SPI_SendByte(AT25DF321_BYTE_PAGE_PGM);
	SPI_SendByte((address >> 16) & 0x3F);
	SPI_SendByte((address >> 8) & 0xFF);
	SPI_SendByte(address & 0xFF);
}


// -------------------------------------------------------------------------------------------------------------------------
// Ends a write transaction
void FlashDriver::endWrite(void)
{
	//
	SPI_PORT |= (SPI_CS_bm);
}



// -------------------------------------------------------------------------------------------------------------------------
// returns the status register bytes
uint8_t FlashDriver::readStatusRegister(void)
{
	SPI_PORT &= ~(SPI_CS_bm);
	SPI_SendByte(AT25DF321_READ_STATUS);
	uint8_t status = SPI_ReceiveByte();
	SPI_PORT |= (SPI_CS_bm);

	return status;
}


void FlashDriver::readStatusRegister(uint8_t protect)
{
	// enable writes
	writeEnable(void);

	// issue global unprotect
	SPI_PORT &= ~(SPI_CS_bm);
	SPI_SendByte(AT25DF321_WRITE_STATUS);				// allow setting of the SPRL bit and the global protect/unprotect flags
	SPI_SendByte(protect);
	SPI_PORT |= (SPI_CS_bm);
}

char FlashDriver::SPI_ReceiveByte(void)
{
	// block until complete
	while((SPSR & (1<<SPIF)) != 0);

	return SPDR;
}

void FlashDriver::SPI_SendByte(char data)
{
	// block until complete
	while((SPSR & (1<<SPIF)) != 0);

	SPDR = data;
}

