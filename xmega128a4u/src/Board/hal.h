#ifndef __HAL_H__
#define __HAL_H__

#include <avr/io.h>
#include <avr/wdt.h>
#include <avr/power.h>
#include <avr/sleep.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <gpslogger.h>
#include <Drivers/AdafruitGps.h>
#include <Drivers/flash.h>
#include <Drivers/TC_driver.h>
#include <LUFA/Drivers/Board/Board.h>
#include <LUFA/Drivers/Peripheral/Serial.h>
#include <Drivers/LufaUsb.h>


/** Enable C linkage for C++ Compilers: */
#if defined(__cplusplus)
	extern "C" {
#endif

	/** Configures the CPU */
	void ConfigureCPU(void);

	/** Configure timers */
	void InitializeTimers(void);

	/** Configure the ADC to read the GPS backup battery voltage */
	void InitializeADC(void);

	/** Configure the SPI interface */
	void InitializeSPI(void);

	/** Configures the pins that the GPS module needs to communicate with the
	microcontroller */
	void ConfigureGpsPins(void);

	/** Configures the USART that the microcontroller uses to communicate with
	the GPS module */
	void ConfigureGpsUart(void);

	/** Enables the GPS module */
	void EnableGpsModule(void);

	/** Disables the GPS module */
	void DisableGpsModule(void);

	/** Send data to the UART */
	void WriteToGpsUart_P(const char* data);

	/** Halts program execution while the FIX pin is asserted */
	void WaitForFixLineLow(void);

	/** Halts program execution while the FIX pin is not asserted */
	void WaitForFixLineHigh(void);

	/** Configures the LEDs status, diagnostics, debugging, etc. */
	void ConfigureLeds(void);
	void TurnOnBuiltInLed(void);
	void TurnOffBuiltInLed(void);
	void TurnOnInfoLed(void);
	void TurnOffInfoLed(void);
	void TurnOnDiagLed(void);
	void TurnOffDiagLed(void);
	void TurnOnErrLed(void);
	void TurnOffErrLed(void);
	void TurnOffLeds(void);

#if defined(__cplusplus)
}
#endif

#endif
