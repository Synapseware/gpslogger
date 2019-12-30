#ifndef __FLASH_H__
#define __FLASH_H__

#include <avr/io.h>
#include <Board/hal.h>
#include <Drivers/at25df321.h>

/* Enable C linkage for C++ Compilers: */
#if defined(__cplusplus)
	extern "C" {
#endif


	/**  Finds the first free address within the flash module */
	int32_t FindFirstFreeAddress(void);


#if defined(__cplusplus)
	}
#endif

#endif
