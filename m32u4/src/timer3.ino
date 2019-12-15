

#define TD_DIV  (F_CPU / 1024)
#define TD_PART (TD_DIV / 10)


bool _secondsTick = false;


void setupTimer3(void)
{
	// Timer/Counter3 Control Register A
    TCCR3A  =   (0<<COM3A1) |
                (0<<COM3A0) |
                (0<<COM3B1) |
                (0<<COM3B0) |
                (0<<COM3C1) |
                (0<<COM3C0) |
                (0<<WGM31)  |
                (0<<WGM30);

    // Timer/Counter3 Control Register B
    TCCR3B  =   (0<<ICNC3)  |
                (0<<ICES3)  |
                (0<<WGM33)  |
                (0<<WGM32)  |
                (1<<CS32)   |
                (0<<CS31)   |
                (0<<CS30);

    // Timer/Counter3 Control Register C
    TCCR3C  =   (0<<FOC3A);

    // Timer/Counter3
    TCNT3   =   0;

    // Output Compare Register 3 A
    // Setup a 1 second counter (16MHz / 1024) = 15625
    OCR3A   =   (TD_PART);

    // Output Compare Register 3 B
    OCR3B   =   0;

    // Output Compare Register 3 C
    OCR3C   =   0;

    // Timer/Counter3 Interrupt Mask Register
    TIMSK3  =   (0<<ICIE3)  |
                (0<<OCIE3C) |
                (0<<OCIE3B) |
                (1<<OCIE3A) |
                (1<<TOIE3);

	// Enable the built in LED and turn it off
	EXT_LED_DDR |= (EXT_LED_bm);
	EXT_LED_PORT &= ~(EXT_LED_bm);
}


ISR(TIMER3_COMPA_vect)
{
    EXT_LED_PORT &= ~(EXT_LED_bm);
}

ISR(TIMER3_OVF_vect)
{
    _secondsTick = 1;
    EXT_LED_PORT |= (EXT_LED_bm);
}


