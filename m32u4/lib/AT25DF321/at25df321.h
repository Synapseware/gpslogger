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

#define AT25DF321_BLK_MASK_4K			0xFFFFF000
#define AT25DF321_BLK_MASK_32K			0xFFFFC000
#define AT25DF321_BLK_MASK_64K			0xFFFF8000

#define AT25DF321_ADDRESS_MASK			0x3FFFFF
//---------------------------------------------------


typedef BLOCK_SIZE_t uint8_t;
#define BLOCK_SIZE_4k			1
#define BLOCK_SIZE_32k			2
#define BLOCK_SIZE_64k			3
#define BLOCK_SIZE_ALL			4


class FlashDriver
{
public:
	FlashDriver(uint8_t deviceAddress, SPISettings* spiSettings, SPIClass* spiClass);

	bool isValid(void);

	void suspend(void);
	void resume(void);

	void globalUnprotect(void);
	void globalProtect(void);
	void protectSector(uint8_t sector);
	void unprotectSector(uint8_t sector);

	bool isWriteEnabled(void);
	bool isWriteProtected(void);
	bool isDeviceReady(void);

	bool programmingError(void);

	bool erase(BLOCK_SIZE_t size);

	void write(uint32_t address, char data);
	void write(uint32_t address, const char*, int);

	char read(void);
	int read(uint32_t address, char*, int);


private:
	char SPI_ReceiveByte(void);
	void SPI_SendByte(char data);

	uint8_t readStatusRegister(void);
	void setGlobalProtectState(uint8_t protect);

	void writeEnable(void);
	void beginWrite(uint32_t address);
	void endWrite(void);

	void beginRead(uint32_t address);
	void endRead(void);

	SPISettings* _spiSettings;
	SPIClass* _spiClass;
	uint8_t _deviceAddress;
};


#endif
