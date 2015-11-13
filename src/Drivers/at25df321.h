#ifndef __AT25DF321__H_
#define __AT25DF321__H_


#include <avr/io.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <LUFA/Drivers/Peripheral/SPI.h>

#include "../gpslogger.h"

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



// Initializes the flash driver & IO hardware
void FlashDriver_Init(SPI_t* spi, PORT_t* spi_port, uint8_t cs);

// device protection methods
void write_enable(uint8_t chip_cs);
void write_disable(uint8_t chip_cs);
void global_unprotect(uint8_t chip_cs);
void global_protect(uint8_t chip_cs);

// device validity checks
bool is_valid(uint8_t chip_cs);

// power saving options
void power_down(uint8_t chip_cs);
void resume(uint8_t chip_cs);

// device status methods
bool is_write_enabled(uint8_t chip_cs);
bool is_write_protected(uint8_t chip_cs);
bool device_is_ready(uint8_t chip_cs);
bool programming_error(uint8_t chip_cs);

// erase methods
void erase_block_4k(uint8_t chip_cs, uint32_t address);
void erase_block_32k(uint8_t chip_cs, uint32_t address);
void erase_block_64k(uint8_t chip_cs, uint32_t address);
void erase_chip(uint8_t chip_cs);

// persistence methods
void begin_write(uint8_t chip_cs, uint32_t address);
void write_byte(char data);
void end_write(uint8_t chip_cs);
void write_block(uint8_t chip_cs, uint32_t address, const char * pbuff, int count);

// reading methods
void begin_read(uint8_t chip_cs, uint32_t address);
void end_read(uint8_t chip_cs);
char read_byte(void);
int read_block(uint8_t chip_cs, uint32_t address, char* pbuff, int count);

#endif
