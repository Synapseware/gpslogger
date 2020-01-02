#include "flash.h"

/** Finds the first free page in the dataflash chip */
int32_t FindFirstFreeAddress(FlashDriver* flash)
{
	USB_PrintString_P(PSTR("Finding first free address\r\n"));
	TurnOnInfoLed();

	if (flash->busy())
	{
		USB_PrintString_P(PSTR("Device is busy - waiting\r\n"));
		TurnOnErrLed();

		while (!flash->busy());
		TurnOffErrLed();
	}

	USB_PrintString_P(PSTR("Opening flash device for read at position 0\r\n"));
	if (!flash->open(FLASH_IO_MODE_READ, 0))
	{
		USB_PrintString_P(PSTR("Failed to open device for reading!\r\n"));
		TurnOffInfoLed();
		return -1;
	}

	// start checking the first byte of each page.  If the page is free, check the whole page before it.
	uint32_t address = 0;
	while (address < FLASH_TOTAL_SIZE)
	{
		flash->seek(address);
		int data = flash->read();
		if (data == -1)
		{
			char msg[64];
			sprintf_P(msg, PSTR("Failed reading the first byte of %i\r\n"), address);
			USB_PrintString(msg);
			TurnOnErrLed();
			TurnOffDiagLed();
			return -1;
		}

		// If the first byte is not 0xFF, assume the page is not empty and skip to the next page
		if (data != 0xFF)
		{
			address += 256;
			continue;
		}

		// Since the first byte is not 0xFF, assume the page is not empty, and move back to the previous page
		// and begin searching for the first free byte within that page
		address -= 256;
		//USB_PrintString_P(PSTR("  search within previous page ")); Serial.println(address);
		flash->seek(address);

		// found a page that appears to be empty, check the page before it to get the first free address
		uint8_t count = 0;
		while (++count != 0)
		{
			data = flash->read();
			if (data == 0xFF)
				break;
		}

		break;
	}

	flash->close();
	TurnOffDiagLed();

	return flash->position();
}
