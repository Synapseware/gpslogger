#ifndef __LCD_H__
#define __LCD_H__

#include <avr/io.h>
#include <stdio.h>
#include <stdbool.h>
#include <LUFA/Drivers/Peripheral/TWI.h>

#include "Drivers/hd44780_twi.h"

typedef struct LCD_TWI_Interface
{
	TWI_t * TWI;
	int32_t Baud;
	uint8_t Address;
	PORT_t * Port;
	uint8_t Pins;
	register8_t Control;
} LCD_TWI_Interface_t;


extern LCD_TWI_Interface_t lcdInterface;


void LCD_Configure(const LCD_TWI_Interface_t*);

void LCD_PrintString_P(const char*);

void LCD_BacklightOn(void);
void LCD_BacklightOff(void);

#endif
