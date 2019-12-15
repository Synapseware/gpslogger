#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/delay.h>

volatile char data = 0;


/* Configure the USART for 8/1/N @ 9600 BUAD */
void setupUsart(void)
{
	#define BAUD 38400
	#include <util/setbaud.h>
	UBRRH = UBRRH_VALUE;
	UBRRL = UBRRL_VALUE;
	#if USE_2X
	UCSRA |= (1 << U2X);
	#else
	UCSRA &= ~(1 << U2X);
	#endif

	UCSRA = 0xFF;			// Clear all flags

	UCSRB = (1<<RXCIE)	|	// RX Receive Interrupt
			(0<<TXCIE)	|	// 
			(0<<UDRIE)	|	// 
			(1<<RXEN)	|	// RX Enable
			(1<<TXEN)	|	// TX Enable
			(0<<UCSZ2);		// 8bit words

	UCSRC = (0<<UMSEL)	|	// Async operation
			(0<<UPM1)	|	// No parity
			(0<<UPM0)	|	// No parity
			(0<<USBS)	|	// 1 stop bit
			(1<<UCSZ1)	|	// 8bit words
			(1<<UCSZ0)	|	// 8bit words
			(0<<UCPOL);		// 

	// Setup IO pins for serial communication
	DDRD &= ~(1<<PD0);
	DDRD |= (1<<PD1);
}

/** Setup the 16bit timer1 for 1s intervals */
void setupTimer1(void)
{
	/* Control Register A - TCCR1A */
	TCCR1A	=	(0<<COM1A1)	|
				(0<<COM1A0)	|
				(0<<COM1B1)	|
				(0<<COM1B0)	|
				(0<<WGM11)	|	// CTC
				(0<<WGM10);

	/* Timer/Counter1 Control Register B – TCCR1B */
	TCCR1B	=	(0<<ICNC1)	|
				(0<<ICES1)	|
				(0<<WGM13)	|
				(1<<WGM12)	|	// CTC
				(1<<CS12)	|	// clk/256
				(0<<CS11)	|
				(0<<CS10);

	/* Output Compare Register 1 A – OCR1AH and OCR1AL */
	OCR1A	=	F_CPU / 256 - 1;	// 16bit access

	/* Output Compare Register 1 B - OCR1BH and OCR1BL */
	OCR1B	=	500;	// 16bit access

	/* Timer/Counter Interrupt Mask Register – TIMSK */
	TIMSK	|=	(0<<TOIE1)	|
				(1<<OCIE1A)	|
				(1<<OCIE1B)	|
				(0<<ICIE1);

	/* Timer/Counter Interrupt Flag Register – TIFR */
	TIFR	|=	(0<<TOV1)	|
				(0<<OCF1A)	|
				(0<<OCF1B)	|
				(0<<ICF1);
}

/** Setup the hardware */
void setup(void)
{
	DDRB = 0xFF;

	DDRD |= (1<<PD6);

	PORTB = 0xFF;
	_delay_ms(300);

	setupUsart();
	setupTimer1();

	sei();
}

/** main */
int main(void)
{
	setup();

	while(1)
	{
		//PORTB = data;
		PORTB = 0xFF;
	}

	return 0;
}

/** Timer1 - Compare A interrupt handler */
ISR(TIMER1_COMPA_vect)
{
	// turn the LED on
	PORTD |= (1<<PD6);
}

/** Timer1 - Compare B interrupt handler */
ISR(TIMER1_COMPB_vect)
{
	// turn the LED off
	PORTD &= ~(1<<PD6);
}

/* Character received from USART */
ISR(USART_RX_vect)
{
	// read the data
	data = UDR;
}
