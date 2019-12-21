#ifndef __AT25DF321__H_
#define __AT25DF321__H_


#include <avr/io.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

#include <SPI.h>

#define	FLASH_TOTAL_SIZE				4194304UL
#define	FLASH_PAGE_SIZE					256UL
#define FLASH_PAGE_MASK					255
#define	FLASH_PAGE_COUNT				(FLASH_TOTAL_SIZE / FLASH_PAGE_SIZE)


//---------------------------------------------------
// Read commands
#define AT25DF321_READ_ARRAY_FAST		0x0B
#define AT25DF321_READ_ARRAY_SLOW		0x03

// Program and erase commands
#define AT25DF321_BLOCK_ERASE_4K		0x20
#define AT25DF321_BLOCK_ERASE_32k		0x52
#define AT25DF321_BLOCK_ERASE_64k		0xD8
#define AT25DF321_CHIP_ERASE			0x60
#define AT25DF321_CHIP_ERASE_ALT		0xC7
#define AT25DF321_BYTE_PAGE_PGM			0x02

// Protection commands
#define AT25DF321_WRITE_ENABLE			0x06
#define AT25DF321_WRITE_DISABLE			0x04
#define AT25DF321_PROTECT_SECTOR		0x36
#define AT25DF321_UNPROTECT_SECTOR		0x39
#define AT25DF321_READ_SECT_PROT_REG	0x3C

// Global protect/unprotect SREG values
#define AT25DF321_SREG_GLOBAL_UNPROTECT	0x00
#define AT25DF321_SREG_GLOBAL_PROTECT	0x7F


// Status register commands
#define AT25DF321_READ_STATUS			0x05
#define AT25DF321_WRITE_STATUS			0x01

// Power saving
#define AT25DF321_DEEP_PWRDN			0xB9
#define AT25DF321_RESUME				0xAB


// Status register masks
#define AT25DF321_SREGMSK_SPRL_bp		7
#define AT25DF321_SREGMSK_SPRL			(1<<7)
#define AT25DF321_SREGMSK_RES_bp		6
#define AT25DF321_SREGMSK_RES			(1<<6)
#define AT25DF321_SREGMSK_EPE_bp		5
#define AT25DF321_SREGMSK_EPE			(1<<5)
#define AT25DF321_SREGMSK_WPP_bp		4
#define AT25DF321_SREGMSK_WPP			(1<<4)
#define AT25DF321_SREGMSK_SWP1_bp		3
#define AT25DF321_SREGMSK_SWP1			(1<<3)
#define AT25DF321_SREGMSK_SWP0_bp		2
#define AT25DF321_SREGMSK_SWP0			(1<<2)
#define AT25DF321_SREGMSK_WEL_bp		1
#define AT25DF321_SREGMSK_WEL			(1<<1)
#define AT25DF321_SREGMSK_RDYBSY_bp		0
#define AT25DF321_SREGMSK_RDYBSY		(1<<0)

// Manufacturer ID read & verify data
#define AT25DF321_READ_MFG_ID			0x9F
#define AT25DF321_MGF_ID				0x1F
#define AT25DF321_DEVICDE_ID			0x47

#define AT25DF321_BLK_MASK_PAGE			0xFFFFFF00
#define AT25DF321_BLK_MASK_4K			0xFFFFF000
#define AT25DF321_BLK_MASK_32K			0xFFFFC000
#define AT25DF321_BLK_MASK_64K			0xFFFF8000

#define AT25DF321_ADDRESS_MASK			0x3FFFFF
//---------------------------------------------------


typedef uint8_t BLOCK_SIZE_t;
#define BLOCK_SIZE_256			1
#define BLOCK_SIZE_4k			2
#define BLOCK_SIZE_32k			3
#define BLOCK_SIZE_64k			4
#define BLOCK_SIZE_ALL			5


class FlashDriver
{
public:
	FlashDriver(SPIClass* spi);

	// valid device check
	bool isValid(void);

	// power saving
	void suspend(void);
	void resume(void);

	// protections
	void globalUnprotect(void);
	void globalProtect(void);
	void protectSector(uint8_t sector);
	void unprotectSector(uint8_t sector);

	// device state
	bool isWriteEnabled(void);
	bool isWriteProtected(void);
	bool isDeviceReady(void);

	// check for previous programming error
	bool programmingError(void);

	// various erase functions (4k, 32k, 64k, chip)
	bool erase(BLOCK_SIZE_t size, uint32_t address);

	// try stream-like operations
	void open(void);
	void close(void);

	// read & write seek (device does not support dual IO stream)
	void seekg(uint32_t address);
	void seekp(uint32_t address);

	// writes
	void write(const char*, int);
	void write(char data);

	// reads
	int read(char*, int);
	char read(void);


private:
	void beginWrite(uint32_t address);
	void writeEnable(void);
	void writeDisable(void);
	void endWrite(void);

	void writeSingle(char);
	uint32_t write(uint32_t address, const char*, int, bool isString);

	void beginRead(uint32_t address);
	void endRead(void);

	uint8_t readStatusRegister(void);

	void setGlobalProtectState(uint8_t protect);


	SPIClass* _spi;
	uint32_t _seekg;
	uint32_t _seekp;
};


#endif
