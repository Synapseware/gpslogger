#ifndef __GPS_LOGGER_H__
#define __GPS_LOGGER_H__


#include <avr/io.h>
#include <avr/wdt.h>
#include <avr/power.h>
#include <avr/sleep.h>
#include <avr/interrupt.h>
#include <string.h>
#include <stdio.h>

#include "Descriptors.h"
#include "NMEAParser.h"

#include <LUFA/Drivers/Peripheral/Serial.h>
#include <LUFA/Drivers/Peripheral/SPI.h>
#include <LUFA/Drivers/Board/LEDs.h>
#include <LUFA/Drivers/Board/Board.h>
#include <LUFA/Drivers/USB/USB.h>
#include <LUFA/Platform/Platform.h>

#include "hal.h"
#include "Drivers/AdafruitGps.h"
#include "Drivers/TC_driver.h"
#include "Drivers/at25df321.h"
#include "Drivers/rtc_driver.h"


// USB configuration
#define USB_TIMER_MS_DELAY	50
#define USB_TIMER_PERIOD	(F_CPU / 256 / USB_TIMER_MS_DELAY)
#define USB_CONNECTED_DELAY	100
#define USB_DISCONN_DELAY	15625

// GPS constants
#define GPS_INTERVAL	120
#define GPS_TIMEOUT		60
#define GPS_FIX_MISSES	5

void HandleLatestGpsMessage(const char* message, int length);

void CDC_ProcessControlStateChange(void);
void EVENT_USB_Device_Connect(void);
void EVENT_USB_Device_Disconnect(void);
void EVENT_USB_Device_ConfigurationChanged(void);
void EVENT_USB_Device_ControlRequest(void);


#endif
