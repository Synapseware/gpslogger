#include "LCD.h"

LCD_TWI_Interface_t * _pTwiInterface = NULL;
uint8_t		_lcdState			= 0;


uint8_t HD44780_ReadState(void)
{
	uint8_t state;
	if (TWI_StartTransmission(_pTwiInterface->TWI, _pTwiInterface->Address | TWI_ADDRESS_READ, 100) == TWI_ERROR_NoError)
	{
		TWI_ReceiveByte(_pTwiInterface->TWI, &state, true);
		TWI_StopTransmission(_pTwiInterface->TWI);
	}

	_lcdState = state & LCD_STATE_MASK;

	return _lcdState;
}


void HD44780_Clear(void)
{
	uint8_t data =	(CMD_DISPLAY_CLR << LCD_COMMAND_SHIFT) |
					LCD_WRITE_COMMAND |
					_lcdState;

	if (TWI_StartTransmission(_pTwiInterface->TWI, _pTwiInterface->Address | TWI_ADDRESS_WRITE, 100) == TWI_ERROR_NoError)
	{
		TWI_SendByte(_pTwiInterface->TWI, data);
		TWI_StopTransmission(_pTwiInterface->TWI);
	}
}


void HD44780_BacklightSet(bool backlightOn)
{
	// clear the backlight flag
	if (backlightOn)
		_lcdState |= LCD_BACKLIGHT;
	else
		_lcdState &= ~LCD_BACKLIGHT;

	if (TWI_StartTransmission(_pTwiInterface->TWI, _pTwiInterface->Address | TWI_ADDRESS_WRITE, 100) == TWI_ERROR_NoError)
	{
		TWI_SendByte(_pTwiInterface->TWI, backlightOn ? LCD_BACKLIGHT : 0);
		TWI_StopTransmission(_pTwiInterface->TWI);
	}
}


void LCD_Configure(const LCD_TWI_Interface_t* interface)
{
	_pTwiInterface = (LCD_TWI_Interface_t *) interface;

	// enable pull-up on SDA & SCL
	PORTCFG.MPCMASK = interface->Pins;
	//interface->Port->LCD_CTRL = PORT_OPC_WIREDANDPULL_gc;
	PORTC_PIN0CTRL = PORT_OPC_WIREDANDPULL_gc;

	// set SCL & SDA as output
	interface->Port->DIRSET = interface->Pins;

	// Initialize the TWI interface
	TWI_Init(&interface->TWI, TWI_BAUD_FROM_FREQ(interface->Baud));

	HD44780_ReadState();
	HD44780_Clear();
	HD44780_BacklightSet(true);
}


void LCD_PrintString_P(const char* str)
{
	if (TWI_StartTransmission(&_pTwiInterface->TWI, _pTwiInterface->Address | TWI_ADDRESS_WRITE, 100) == TWI_ERROR_NoError)
	{
		char nibble;
		while (true)
		{
			char data = pgm_read_byte(str++);
			if (!data)
				break;

			// high nibble first
			nibble =	(data & LCD_DATA_MASK) |
						(_lcdState & LCD_STATE_MASK) |
						LCD_WRITE_ENABLE;
			TWI_SendByte(_pTwiInterface->TWI, nibble);

			// low nibble last
			nibble =	(data << 4) |
						(_lcdState & LCD_STATE_MASK) |
						LCD_WRITE_ENABLE;
			TWI_SendByte(_pTwiInterface->TWI, nibble);
		}

		TWI_StopTransmission(_pTwiInterface->TWI);
	}
}


void LCD_BacklightOn(void)
{
	HD44780_Backlight(true);
}


void LCD_BacklightOff(void)
{
	HD44780_Backlight(false);
}

