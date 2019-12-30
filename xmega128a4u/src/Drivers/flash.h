#ifndef __FLASH_H__
#define __FLASH_H__

#include <avr/io.h>
#include <Board/hal.h>
#include <Drivers/at25df321.h>

int32_t FindFirstFreeAddress(void);

#endif
