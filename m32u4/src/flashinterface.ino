

/** TODO:  Improve this so it does a b-search and finds the first free address within a page.  */
int32_t FindFirstFreeAddress(void)
{
	/*
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
	*/

	return 0;
}

