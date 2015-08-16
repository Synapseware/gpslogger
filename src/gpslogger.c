#include "gpslogger.h"


volatile int8_t _secondsTick = 0;


/** LUFA CDC Class driver interface configuration and state information. This structure is
 *  passed to all CDC Class driver functions, so that multiple instances of the same class
 *  within a device can be differentiated from one another.
 */
USB_ClassInfo_CDC_Device_t VirtualSerial_CDC_Interface =
	{
		.Config =
			{
				.ControlInterfaceNumber   = INTERFACE_ID_CDC_CCI,
				.DataINEndpoint           =
					{
						.Address          = CDC_TX_EPADDR,
						.Size             = CDC_TXRX_EPSIZE,
						.Banks            = 1,
					},
				.DataOUTEndpoint =
					{
						.Address          = CDC_RX_EPADDR,
						.Size             = CDC_TXRX_EPSIZE,
						.Banks            = 1,
					},
				.NotificationEndpoint =
					{
						.Address          = CDC_NOTIFICATION_EPADDR,
						.Size             = CDC_NOTIFICATION_EPSIZE,
						.Banks            = 1,
					},
			},
	};

/** Standard file stream for the CDC interface when set up, so that the virtual CDC COM port can be
 *  used like any regular character stream in the C APIs.
 */
static FILE USBSerialStream;

/** Configure timer for 1 second delay with interrupts */
void Timer_Initialize(void)
{
	/*
	// configure timer/counter0 (clk/64)
	TC0_t.CTRLA = TC_CLKSEL_t.TC_CLKSEL_DIV1024_gc;

	// set up for FRQ
	TC0_t.CTRLB = TC_WGMODE_FRQ_gc;

	// Clear remaining control registers
	TC0_t.CTRLC = 0;
	TC0_t.CTRLD = TC_EVACT_OFF_gc | TC_EVSEL_OFF_gc;
	TC0_t.CTRLE = TC_BYTEM_NORMAL_gc;

	// Configure interrupts
	TC0_t.INTCTRLA = TC_ERRINTLVL_OFF_gc | TC_OVFINTLVL_OFF_gc;

	// Compare/Capture A Interrupt Level
	TC0_t.INTCTRLB = TC_CCAINTLVL_enum.TC_CCAINTLVL_HI_gc;

	// configure the period
	TC0_t.PER   = F_CPU / 1024;
	*/

	/* Set period/TOP value. */
	TC_SetPeriod( &TCC0, F_CPU / 1024 );

	/* Select clock source. */
	TC0_ConfigClockSource( &TCC0, TC_CLKSEL_DIV1024_gc );

	/* Setup interrupts for Compare/Capture A */
	TC0_SetCCAIntLevel( &TCC0, TC_CCAINTLVL_HI_gc );
}


/** Main program entry point. This routine contains the overall program flow, including initial
 *  setup of all components and the main program loop.
 */
int main(void)
{
	SetupHardware();

	/* Create a regular character stream for the interface so that it can be used with the stdio.h functions */
	CDC_Device_CreateStream(&VirtualSerial_CDC_Interface, &USBSerialStream);

	GlobalInterruptEnable();

	// to receive data from the CDC:
	// CDC_Device_ReceiveByte(&VirtualSerial_CDC_Interface);

	char * msg = "Hello.\r\n";
	fputs(msg, &USBSerialStream);

	CDC_Device_Flush(&VirtualSerial_CDC_Interface);

	for (;;)
	{
		/* Must throw away unused bytes from the host, or it will lock up while waiting for the device */
		CDC_Device_ReceiveByte(&VirtualSerial_CDC_Interface);

		/* Required calls */		
		CDC_Device_USBTask(&VirtualSerial_CDC_Interface);
		USB_USBTask();
	}
}

/** Configures the board hardware and chip peripherals for the application's functionality. */
void SetupHardware(void)
{

	/* Start the PLL to multiply the 2MHz RC oscillator to 32MHz and switch the CPU core to run from it */
	XMEGACLK_StartPLL(CLOCK_SRC_INT_RC2MHZ, 2000000, F_CPU);
	XMEGACLK_SetCPUClockSource(CLOCK_SRC_PLL);

	/* Start the 32MHz internal RC oscillator and start the DFLL to increase it to 48MHz using the USB SOF as a reference */
	XMEGACLK_StartInternalOscillator(CLOCK_SRC_INT_RC32MHZ);
	XMEGACLK_StartDFLL(CLOCK_SRC_INT_RC32MHZ, DFLL_REF_INT_USBSOF, F_USB);

	PMIC.CTRL = PMIC_LOLVLEN_bm | PMIC_MEDLVLEN_bm | PMIC_HILVLEN_bm;

	/* Hardware Initialization */
	USB_Init();

	// initialize user hardware here
	(LED_PORT)->OUTCLR = LED_PIN;
	(LED_PORT)->DIRSET = LED_PIN;

	Timer_Initialize();
}

/** Event handler for the library USB Connection event. */
void EVENT_USB_Device_Connect(void)
{
	//LEDs_SetAllLEDs(LEDMASK_USB_ENUMERATING);
	//(LED_PORT)->OUTSET = LED_PIN;
}

/** Event handler for the library USB Disconnection event. */
void EVENT_USB_Device_Disconnect(void)
{
	//LEDs_SetAllLEDs(LEDMASK_USB_NOTREADY);
	(LED_PORT)->OUTCLR = LED_PIN;
}

/** Event handler for the library USB Configuration Changed event. */
void EVENT_USB_Device_ConfigurationChanged(void)
{
	bool ConfigSuccess = true;

	ConfigSuccess &= CDC_Device_ConfigureEndpoints(&VirtualSerial_CDC_Interface);

	LEDs_SetAllLEDs(ConfigSuccess ? LEDMASK_USB_READY : LEDMASK_USB_ERROR);

	if (ConfigSuccess)
		(LED_PORT)->OUTSET = LED_PIN;
}

/** Event handler for the library USB Control Request reception event. */
void EVENT_USB_Device_ControlRequest(void)
{
	CDC_Device_ProcessControlRequest(&VirtualSerial_CDC_Interface);
}

/** Interrupt handler for TCC0 Compare/Capture A */
ISR(TCC0_CCA_vect)
{
	_secondsTick = 1;

	// toggle the onboard LED
	(LED_PORT)->OUTTGL = LED_PIN;
}
