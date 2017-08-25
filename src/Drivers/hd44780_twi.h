/*
             LUFA Library
     Copyright (C) Dean Camera, 2017.

  dean [at] fourwalledcubicle [dot] com
           www.lufa-lib.org
*/

/*
  Copyright 2017  Dean Camera (dean [at] fourwalledcubicle [dot] com)
  Copyright 2012  Simon Foster (simon.foster [at] inbox [dot] com)

  Permission to use, copy, modify, distribute, and sell this
  software and its documentation for any purpose is hereby granted
  without fee, provided that the above copyright notice appear in
  all copies and that both that the copyright notice and this
  permission notice and warranty disclaimer appear in supporting
  documentation, and that the name of the author not be used in
  advertising or publicity pertaining to distribution of the
  software without specific, written prior permission.

  The author disclaims all warranties with regard to this
  software, including all implied warranties of merchantability
  and fitness.  In no event shall the author be liable for any
  special, indirect or consequential damages or any damages
  whatsoever resulting from loss of use, data or profits, whether
  in an action of contract, negligence or other tortious action,
  arising out of or in connection with the use or performance of
  this software.
*/

/** \file
 *
 *  Header file for HD44780.c.
 */

#ifndef _HD44780_H_
#define _HD44780_H_

	/* Includes: */
		#include <avr/io.h>
		#include <util/delay.h>
		#include <avr/power.h>

	/* Macros: */
		#define RS                  (1 << 4)
		#define ENABLE              (1 << 7)

		#define HI4_MASK            0xF0
		#define LO4_MASK            0x0F

		#define ALL_BITS            (RS | ENABLE | LO4_MASK)

		#define HI4(c)              ((c & HI4_MASK) >> 4)
		#define LO4(c)              ((c & LO4_MASK) >> 0)

    #define CMD_DISPLAY_CLR     0x01
    #define CMD_CURSOR_HOME     0x02
    #define CMD_DISPLAY_ON      0x0C
    #define LCD_PIN_RS          0
    #define LCD_PIN_RW          1
    #define LCD_PIN_EN          2
    #define LCD_PIN_LIGHT       3
    #define LCD_PIN_D4          4
    #define LCD_PIN_D5          5
    #define LCD_PIN_D6          6
    #define LCD_PIN_D7          7

    #define LCD_WRITE_COMMAND   (1<<LCD_PIN_RS)
    #define LCD_WRITE_ENABLE    (1<<LCD_PIN_RW)
    #define LCD_ENABLE          (1<<LCD_PIN_EN)
    #define LCD_BACKLIGHT       (1<<LCD_PIN_LIGHT)
    #define LCD_DATA_0          (1<<LCD_PIN_D4)
    #define LCD_DATA_1          (1<<LCD_PIN_D5)
    #define LCD_DATA_2          (1<<LCD_PIN_D6)
    #define LCD_DATA_3          (1<<LCD_PIN_D7)

    #define LCD_STATE_MASK      0x0F
    #define LCD_DATA_MASK       0xF0

    #define LCD_COMMAND_SHIFT   4

	/* Function Prototypes: */
		void HD44780_Initialize(void);
		void HD44780_WriteData(const uint8_t c);
		void HD44780_WriteCommand(const uint8_t c);

#endif
