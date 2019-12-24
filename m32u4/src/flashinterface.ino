

/*
Hardware interface for AT25DF321:
Since the Atmega32u4 is a 5v device, a TSX0108e logic-level bi-directional translator is used.
The following ports map the SPI interface to the flash device:

32u4	Pin		Tsx		Flash	Color
-----	----	----	-----	-----
SS(CS)	PB0		7		1		Purple
SCK		PB1		5		6		Blue
MOSI	PB2		6		5		Yellow
MISO	PB3		8		2		Green


*/



uint32_t _writeAddress = 0;


uint32_t currentWriteAddress(void)
{
	return _writeAddress;
}


/** TODO:  Improve this so it does a b-search and finds the first free address within a page.  */
int32_t findFirstFreeAddress(void)
{
	Serial.println(F("Finding first free address"));
	setDbgLed();

	// Wait for device to not be busy
	if (flash->busy())
	{
		Serial.println(F("Device is busy - waiting"));
		setErrLed();

		// wait for ready state
		while (!flash->busy());
		clearErrLed();
	}

	Serial.println(F("Opening flash device for read at position 0"));
	if (!flash->open(MODE_READ, 0))
	{
		Serial.println(F("Failed to open device for reading!"));
		clearDbgLed();
		return -1;
	}

	// start checking the first byte of each page.  If the page is free, check the whole page before it.
	uint32_t address = 0;
	while (address < FLASH_TOTAL_SIZE)
	{
		//Serial.print(F("  checking byte at address ")); Serial.println(address);
		flash->seek(address);
		int data = flash->read();
		if (data == -1)
		{
			Serial.print(F("Failed reading the first byte of ")); Serial.println(address);
			setErrLed();
			clearDbgLed();
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
		//Serial.print(F("  search within previous page ")); Serial.println(address);
		flash->seek(address);

		// found a page that appears to be empty, check the page before it to get the first free address
		uint8_t count = 0;
		while (++count != 0)
		{
			data = flash->read();
			if (data == 0xFF)
				break;
		}

		// found our first free address
		_writeAddress = flash->position();
		break;
	}

	//Serial.print(F("Found the first free address at ")); Serial.println(_writeAddress);
	flash->close();
	clearDbgLed();

	return _writeAddress;
}
