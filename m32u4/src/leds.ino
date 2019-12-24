


// ----------------------------------------------------------------------------
// Toggles the LED when the fix pin changes
void toggleFixLed(void)
{
	static bool lastState = false;
	bool currentState = (GPS_PINX & GPS_FIX_bm) != 0;

	if (lastState == currentState)
		return;

	if (currentState)
		setStaLed();
	else
		clearStaLed();

	lastState = currentState;
}


// ----------------------------------------------------------------------------
// Turns on the error LED
void setErrLed(void)
{

	ERR_LED_PORT |= (ERR_LED_bm);
}
void clearErrLed(void)
{

	ERR_LED_PORT &= ~(ERR_LED_bm);
}
void setDbgLed(void)
{

	DBG_LED_PORT |= (DBG_LED_bm);
}
void clearDbgLed(void)
{

	DBG_LED_PORT &= ~(DBG_LED_bm);
}
void setStaLed(void)
{

	STA_LED_PORT |= (STA_LED_bm);
}
void clearStaLed(void)
{

	STA_LED_PORT &= ~(STA_LED_bm);
}
void configureLeds(void)
{
	ERR_LED_DDR |= (ERR_LED_bm);
	DBG_LED_DDR |= (DBG_LED_bm);
	STA_LED_DDR |= (STA_LED_bm);

	clearErrLed();
	clearDbgLed();
	clearStaLed();
}
