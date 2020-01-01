#ifndef __FLASH_H__
#define __FLASH_H__

#include <avr/io.h>
#include <Drivers/hal.h>
#include <Drivers/at25df321.h>


/**  Finds the first free address within the flash module */
int32_t FindFirstFreeAddress(FlashDriver*);


#endif
