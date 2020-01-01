#include "LufaUsb.h"



/* Enable C linkage for C++ Compilers: */
#if defined(__cplusplus)
	extern "C" {
#endif

	static bool		_hostConnected		= false;
	static bool		_hostConfigured		= false;

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


	void USB_Initialize(void)
	{
		_hostConnected = false;
		_hostConfigured = false;

		USB_Init();

		/* Create a regular character stream for the interface so that it can be used with the stdio.h functions */
		CDC_Device_CreateStream(&VirtualSerial_CDC_Interface, &USBSerialStream);
		//CDC_Device_CreateBlockingStream(&VirtualSerial_CDC_Interface, &USBSerialStream);

		// wait for host to connect
		while(true)
		{
			if (_hostConnected && _hostConfigured)
				break;
			_delay_ms(250);
		}

		// introduce a slight pause to allow
		// host to finalize connection
		_delay_ms(3000);
	}


	bool USB_IsHostConnected(void)
	{
		return _hostConnected;
	}


	bool USB_IsHostConfigured(void)
	{
		return _hostConfigured;
	}


	uint16_t USB_BytesReceived(void)
	{
		return CDC_Device_BytesReceived(&VirtualSerial_CDC_Interface);
	}


	uint16_t USB_ReceiveByte(void)
	{
		return CDC_Device_ReceiveByte(&VirtualSerial_CDC_Interface);
	}


	void USB_DoTasks(void)
	{
		CDC_Device_USBTask(&VirtualSerial_CDC_Interface);
		USB_USBTask();
	}


	void USB_PrintString_P(const char* message)
	{
		if (!_hostConnected || NULL == message)
			return;

		CDC_Device_SendString_P(&VirtualSerial_CDC_Interface, message);
		USB_PrintFlush();
	}
	void USB_PrintString(const char* message)
	{
		if (!_hostConnected || NULL == message)
			return;

		CDC_Device_SendString(&VirtualSerial_CDC_Interface, message);
		USB_PrintFlush();
	}
	void USB_PrintFlush(void)
	{
		if (!_hostConnected)
			return;
		CDC_Device_Flush(&VirtualSerial_CDC_Interface);
	}


	/** USB Connect */
	void EVENT_USB_Device_Connect(void)
	{

		// No-Op
	}

	/** USB Disconnect */
	void EVENT_USB_Device_Disconnect(void)
	{

		// No-Op
	}

	/** Process control line state changes */
	void EVENT_CDC_Device_ControLineStateChanged(USB_ClassInfo_CDC_Device_t* const CDCInterfaceInfo)
	{
		uint8_t mask = (CDC_CONTROL_LINE_OUT_DTR | CDC_CONTROL_LINE_OUT_RTS);
		if (CDCInterfaceInfo->State.ControlLineStates.HostToDevice & mask)
		{
			_hostConnected = true;
		}
		else
		{
			_hostConnected = false;
		}
	}

	/** Event handler for the library USB Configuration Changed event. */
	void EVENT_USB_Device_ConfigurationChanged(void)
	{
		// configure the CDC interface
		_hostConfigured = CDC_Device_ConfigureEndpoints(&VirtualSerial_CDC_Interface);
		if (_hostConfigured)
		{
			// configuration successful
		}
	}

	/** Event handler for the library USB Control Request reception event. */
	void EVENT_USB_Device_ControlRequest(void)
	{
		// 
		CDC_Device_ProcessControlRequest(&VirtualSerial_CDC_Interface);
	}



#if defined(__cplusplus)
	}
#endif
