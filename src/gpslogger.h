#ifndef __GPS_LOGGER_H__
#define __GPS_LOGGER_H__


#include <avr/io.h>
#include <avr/wdt.h>
#include <avr/power.h>
#include <avr/interrupt.h>
#include <string.h>
#include <stdio.h>

#include "Descriptors.h"

#include <LUFA/Drivers/Peripheral/Serial.h>
#include <LUFA/Drivers/Board/LEDs.h>
#include <LUFA/Drivers/Board/Board.h>
#include <LUFA/Drivers/Peripheral/XMEGA/Serial_XMEGA.h>
#include <LUFA/Drivers/USB/USB.h>
#include <LUFA/Platform/Platform.h>


#include "Drivers/AdafruitGps.h"
#include "Drivers/TC_driver.h"


#define LED_PORT	&PORTE
#define LED_PIN		(1 << 1)

#define LED_RED		(1<<1)
#define LED_YLW		(1<<0)



/* Function Prototypes: */
void SetupHardware(void);

void EVENT_USB_Device_Connect(void);
void EVENT_USB_Device_Disconnect(void);
void EVENT_USB_Device_ConfigurationChanged(void);
void EVENT_USB_Device_ControlRequest(void);


#endif
