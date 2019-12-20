


bool rewritePage()




int isPageFree(uint32_t address)
{
	int freeFrom = 0;
	uint8_t i = 0;
	address &= AT25DF321_BLK_MASK_PAGE;
	flash->beginRead(address);
	while(1)
	{
		char data = flash->read();
		if (data != 0xFF)
		{
			freeFrom = address + i;
			break;
		}

		i++;
		if (i == 0)
		{
			freeFrom = address;
			break;
		}
	}
	flash->endRead();

	return freeFrom;
}


uint32_t findFirstFreeAddress_bsearch(void)
{
	uint16_t pages = FLASH_PAGE_COUNT;
}


/** TODO:  Improve this so it does a b-search and finds the first free address within a page.  */
uint32_t findFirstFreeAddress(void)
{
	if (!flash->isDeviceReady())
	{
		setErrLed();

		// wait for ready state
		while (!flash->isDeviceReady());

		clearErrLed();
	}

	setDbgLed();

	uint32_t address = 0;
	uint32_t freefrom = 0;
	int matches = 0;

	// short-cut: if the first byte in flash is 0xFF, assume an empty device
	char data = flash->read(0);
	if (data == 0xFF)
		return 0;

	// start checking the first byte of each page.  If the page is free, check the whole page before it.
	while (address < FLASH_TOTAL_SIZE)
	{
		address += 256;
		data = flash->read(address);
		if (data != 0xFF)
			continue;

		// found a page that appears to be empty.
		if (isPageFree(address) == address)
		{
			// search the previous page for the first free position
			freefrom = isPageFree(address - 256);
			return freefrom;
		}
	}



	flash->beginRead(address);
	while (address < FLASH_TOTAL_SIZE)
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
			freefrom = 0;
		}

		address++;

		// 255 0xFF's is most likely a free page...
		if (matches > 255)
			break;
	}

	flash->endRead();

	clearDbgLed();

	return freefrom;
}

