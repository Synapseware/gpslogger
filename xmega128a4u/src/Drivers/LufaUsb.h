#ifndef __LUFAUSB_H__
#define __LUFAUSB_H__

#include <avr/io.h>
#include <stdio.h>
#include <stdbool.h>
#include "Descriptors.h"
#include <LUFA/Platform/Platform.h>
#include <LUFA/Drivers/Board/Board.h>
#include <LUFA/Drivers/Board/LEDs.h>
#include <LUFA/Drivers/Peripheral/Serial.h>
#include <LUFA/Drivers/Peripheral/SPI.h>
#include <LUFA/Drivers/Peripheral/TWI.h>
#include <LUFA/Drivers/USB/USB.h>
#include <LUFA/Drivers/USB/Class/Common/CDCClassCommon.h>


extern USB_ClassInfo_CDC_Device_t VirtualSerial_CDC_Interface;


void USB_Initialize(void);
bool USB_IsHostConnected(void);
bool USB_IsHostConfigured(void);
uint16_t USB_BytesReceived(void);
uint16_t USB_ReceiveByte(void);
void USB_DoTasks(void);

void USB_PrintString_P(const char* message);
void USB_PrintString(const char* message);
void USB_PrintFlush(void);

void EVENT_USB_Device_Connect(void);
void EVENT_USB_Device_Disconnect(void);
void EVENT_CDC_Device_ControLineStateChanged(USB_ClassInfo_CDC_Device_t* const CDCInterfaceInfo);
void EVENT_USB_Device_ConfigurationChanged(void);
void EVENT_USB_Device_ControlRequest(void);



#endif
