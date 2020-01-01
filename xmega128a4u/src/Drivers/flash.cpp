#include "flash.h"

/** Finds the first free page in the dataflash chip */
int32_t FindFirstFreeAddress(FlashDriver* flash)
{
	if (!flash->busy())
	{
		TurnOnErrLed();
	}

	TurnOnInfoLed();

	int32_t address = 0;
	int32_t freefrom = -1;
	int matches = 0;

	flash->open(MODE_READ, address);
	while (address < FLASH_TOTAL_SIZE && address > -1)
	{
		// look for the first 0xFF followed by 0xFF's to the end
		char data = flash->read();

		if (0xFF == data)
		{
			matches++;
			if (freefrom < 0)
				freefrom = address;
		}
		else
		{
			matches = 0;
			freefrom = -1;
		}

		address++;

		// 255 0xFF's is most likely a free page...
		if (matches > 255)
			break;
	}
	
	flash->close();

	TurnOffInfoLed();

	return freefrom;
}
