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
#include <LUFA/Drivers/USB/USB.h>

#include "hal.h"
#include "Drivers/AdafruitGps.h"
#include "Drivers/TC_driver.h"
#include "Drivers/at25df321.h"
#include "Drivers/rtc_driver.h"
#include "Drivers/hd44780_twi.h"
#include "Descriptors.h"
#include "NMEAParser.h"

// USB configuration
#define USB_TIMER_MS_DELAY	50
#define USB_TIMER_PERIOD	(F_CPU / 256 / USB_TIMER_MS_DELAY)
#define USB_CONNECTED_DELAY	5000
#define USB_DISCONN_DELAY	15625

// GPS constants
#define GPS_INTERVAL	120
#define GPS_TIMEOUT		60
#define GPS_FIX_MISSES	5

// LCD info
// Using a PCF8574AT (default I2C address is 0x3F) I2C expander
#define LCD_BAUD		100000
#define LCD_TWI_ADDR	0x7E

void HandleLatestGpsMessage(const char* message, int length);

void EVENT_USB_Device_ControlRequest(void);
void EVENT_USB_Device_ConfigurationChanged(void);
//void CDC_ProcessControlStateChange(void);
//void EVENT_USB_Device_Connect(void);
//void EVENT_USB_Device_Disconnect(void);


#endif
