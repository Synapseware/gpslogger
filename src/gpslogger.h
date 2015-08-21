#ifndef __GPS_LOGGER_H__
#define __GPS_LOGGER_H__


#include <avr/io.h>
#include <avr/wdt.h>
#include <avr/power.h>
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


#include "Drivers/AdafruitGps.h"
#include "Drivers/TC_driver.h"
#include "Drivers/at25df321.h"


#define LED_PORT		(&PORTE)
#define LED_PIN			PIN1_bm


#define GPS_UART_port	(&PORTE)
#define GPS_RX_pin		PIN2_bm
#define GPS_TX_pin		PIN3_bm
#define GPS_USART		(&USARTE0)


#define GPS_EN_port		(&PORTC)
#define GPS_EN_pin		PIN1_bm


#define SPI_DEVICE		SPID
#define SPI_PORT		(&PORTD)
#define SPI_CS			PIN4_bm



/* Function Prototypes: */
void SetupHardware(void);

void EVENT_USB_Device_Connect(void);
void EVENT_USB_Device_Disconnect(void);
void EVENT_USB_Device_ConfigurationChanged(void);
void EVENT_USB_Device_ControlRequest(void);


#endif
