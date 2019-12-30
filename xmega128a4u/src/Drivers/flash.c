#include "flash.h"

/* Enable C linkage for C++ Compilers: */
#if defined(__cplusplus)
	extern "C" {
#endif



	/** Finds the first free page in the dataflash chip */
	int32_t FindFirstFreeAddress(void)
	{
		if (!device_is_ready(SPI_CS))
		{
			TurnOnErrLed();
		}

		TurnOnInfoLed();

		int32_t address = 0;
		int32_t freefrom = -1;
		int matches = 0;

		begin_read(SPI_CS, address);
		while (address < FLASH_TOTAL_SIZE && address > -1)
		{
			// look for the first 0xFF followed by 0xFF's to the end
			char data = read_byte();

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
		end_read(SPI_CS);

		TurnOffInfoLed();

		return freefrom;
	}



#if defined(__cplusplus)
	}
#endif

