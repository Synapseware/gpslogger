#ifndef __AT25DF321__H_
#define __AT25DF321__H_


#include <avr/io.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <LUFA/Drivers/Peripheral/SPI.h>


#define	FLASH_TOTAL_SIZE		4194304UL
#define	FLASH_PAGE_SIZE			256UL
#define	FLASH_PAGE_COUNT		(FLASH_TOTAL_SIZE / FLASH_PAGE_SIZE)



/** Initializes the flash driver & IO hardware */
void FlashDriver_Init(SPI_t* spi, PORT_t* spi_port, uint8_t cs);

/** Writes a block of bytes to the flash device */
int write_block(uint8_t chip_cs, uint32_t address, int count, const char * pbuff);

/** Reads a block of bytes from the flash device */
int read_block(uint8_t chip_cs, uint32_t address, int count, char* pbuff);

#endif
