/*
             LUFA Library
     Copyright (C) Dean Camera, 2014.

  dean [at] fourwalledcubicle [dot] com
           www.lufa-lib.org
*/

/*
  Copyright 2014  Dean Camera (dean [at] fourwalledcubicle [dot] com)

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
 *  \brief LUFA Custom Board Hardware Information Driver (Template)
 *
 *  This is a stub driver header file, for implementing custom board
 *  layout hardware with compatible LUFA board specific drivers. If
 *  the library is configured to use the BOARD_USER board mode, this
 *  driver file should be completed and copied into the "/Board/" folder
 *  inside the application's folder.
 *
 *  This stub is for the board-specific component of the LUFA Board Hardware
 *  information driver.
 */

#ifndef __BOARD_USER_H__
#define __BOARD_USER_H__

	  /* Includes: */
		// TODO: Add any required includes here

    /* Enable C linkage for C++ Compilers: */
		#if defined(__cplusplus)
			extern "C" {
		#endif

    /* Preprocessor Checks: */
		#if !defined(__INCLUDE_FROM_BOARD_H)
			#error Do not include this file directly. Include LUFA/Drivers/Board/Board.h instead.
		#endif


    // GPS
    // Table 32-4. Port D - alternate functions.
    // GPS RX goes to XMega TX and GPS TX goes to XMega RX
    // PD2 => RXD0
    // PD3 => TXD0
    #define GPS_UART      USARTD0
    #define GPS_port      PORTD
    #define GPS_port_ISR  PORTD_INT0_vect
    #define GPS_PPS_pin   PIN1_bm
    #define GPS_TX_pin    PIN2_bm
    #define GPS_RX_pin    PIN3_bm
    #define GPS_EN_pin    PIN4_bm
    #define GPS_FIX_pin   PIN5_bm
    #define GPS_UART_ISR  USARTD0_RXC_vect
    #define GPS_BAT_port  PORTA
    #define GPS_BAT_pin   PIN1_bm

    // Flash device
    #define SPI_DEVICE    SPIC
    #define SPI_PORT      PORTC
    #define SPI_CS        PIN4_bm
    #define SPI_MOSI      PIN5_bm
    #define SPI_MISO      PIN6_bm
    #define SPI_SCK       PIN7_bm

    // Built-in LED
    #define LED_PORT      PORTE
    #define LED_PIN       PIN1_bm

    // LED setup
    #define INFO_LED_port PORTB
    #define INFO_LED_pin  PIN0_bm
    #define DIAG_LED_port PORTB
    #define DIAG_LED_pin  PIN1_bm
    #define ERR_LED_port  PORTB
    #define ERR_LED_pin   PIN2_bm

    // I2C Interface for display
    #define LCD_PORT      PORTC
    #define LCD_TWI       TWIC
    #define LCD_SDA       PIN0_bm
    #define LCD_SCL       PIN1_bm
    //#define LCD_CTRL      PIN0CTRL

    // LCD info
    // Using a PCF8574AT (default I2C address is 0x3F) I2C expander
    #define LCD_TWI_ADDR  0x7E
    #define LCD_BAUD      100000

    // USB info
    #define USB_TASK_ISR  TCC1_CCA_vect


	/* Public Interface - May be used in end-application: */
		/* Macros: */
			/** Indicates the board has a hardware Buttons mounted if defined. */
//			#define BOARD_HAS_BUTTONS

			/** Indicates the board has a hardware Dataflash mounted if defined. */
//			#define BOARD_HAS_DATAFLASH

			/** Indicates the board has a hardware Joystick mounted if defined. */
//			#define BOARD_HAS_JOYSTICK

			/** Indicates the board has a hardware LEDs mounted if defined. */
//			#define BOARD_HAS_LEDS

	/* Disable C linkage for C++ Compilers: */
		#if defined(__cplusplus)
			}
		#endif

#endif

/** @} */

