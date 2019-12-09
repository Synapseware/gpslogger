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

#include "Board/hal.h"
#include "Drivers/LCD.h"
#include "Drivers/AdafruitGps.h"
#include "Drivers/TC_driver.h"
#include "Drivers/at25df321.h"
//#include "Drivers/rtc_driver.h"
#include "Drivers/hd44780_twi.h"
#include "Drivers/LufaUsb.h"
#include "Drivers/NMEAParser.h"

// USB configuration
#define USB_TIMER_PERIOD	(F_CPU / 256 / 125)
#define USB_CONNECTED_DELAY	5000
#define USB_DISCONN_DELAY	15625

// GPS constants
#define GPS_INTERVAL	120
#define GPS_TIMEOUT		60
#define GPS_FIX_MISSES	5





#endif
