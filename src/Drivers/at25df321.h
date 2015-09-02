#ifndef __AT25DF321__H_
#define __AT25DF321__H_


#include <avr/io.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <LUFA/Drivers/Peripheral/SPI.h>


#define	FLASH_TOTAL_SIZE				4194304UL
#define	FLASH_PAGE_SIZE					256UL
#define	FLASH_PAGE_COUNT		(FLASH_TOTAL_SIZE / FLASH_PAGE_SIZE)


//---------------------------------------------------
// Read commands
#define AT25DF321_READ_ARRAY			0x0B
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

// Status register commands
#define AT25DF321_READ_STATUS			0x05
#define AT25DF321_WRITE_STATUS			0x01

// Misc
#define AT25DF321_READ_MFG_ID			0x9F
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

#define AT25DF321_MGF_ID				0x1F
#define AT25DF321_DEVICDE_ID			0x47

#define AT25DF321_BLK_MASK_4K			0xFFFFF000
#define AT25DF321_BLK_MASK_32K			0xFFFFF000
#define AT25DF321_BLK_MASK_64K			0xFFFF0000

//---------------------------------------------------



/** Initializes the flash driver & IO hardware */
void FlashDriver_Init(SPI_t* spi, PORT_t* spi_port, uint8_t cs);

void write_enable(uint8_t chip_cs);
void write_disable(uint8_t chip_cs);
void global_unprotect(uint8_t chip_cs);

/** Writes a block of bytes to the flash device */
int write_block(uint8_t chip_cs, uint32_t address, int count, const char * pbuff);

void begin_write(uint8_t chip_cs, uint32_t address);
void write_byte(char data);
void end_write(uint8_t chip_cs);

/** Reads a block of bytes from the flash device */
int read_block(uint8_t chip_cs, uint32_t address, int count, char* pbuff);

#endif
