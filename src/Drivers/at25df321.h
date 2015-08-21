#ifndef __AT25DF321__H_
#define __AT25DF321__H_


#include <avr/io.h>
#include <stdio.h>
#include <stdlib.h>
#include <LUFA/Drivers/Peripheral/SPI.h>

/** Initializes the flash driver & IO hardware */
void Initialize_FlashDriver(SPI_t* spi, PORT_t* spi_port, uint8_t cs);

/** Writes a block of bytes to the flash device */
int write_block(uint8_t chip_cs, uint32_t address, int count, const char * pbuff);

/** Reads a block of bytes from the flash device */
int read_block(uint8_t chip_cs, uint32_t address, int count, char* pbuff);

void global_unprotect(uint8_t chip_cs);

#endif
