#ifndef __GPS_LOGGER_H__
#define __GPS_LOGGER_H__


#include <avr/io.h>
#include <avr/wdt.h>
#include <avr/power.h>
#include <avr/sleep.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <string.h>
#include <stdio.h>

#include <LUFA/Platform/Platform.h>
#include <LUFA/Drivers/Board/Board.h>
#include <LUFA/Drivers/Board/LEDs.h>
#include <LUFA/Drivers/Peripheral/Serial.h>
#include <LUFA/Drivers/Peripheral/SPI.h>
#include <LUFA/Drivers/Peripheral/TWI.h>

#include <Board/hal.h>
#include <Drivers/AdafruitGps.h>
#include <Drivers/flash.h>
#include <Drivers/TC_driver.h>
#include <Drivers/at25df321.h>
#include <Drivers/LufaUsb.h>


/* Enable C linkage for C++ Compilers: */
#if defined(__cplusplus)
	extern "C" {
#endif


#if defined(__cplusplus)
	}
#endif



#endif
