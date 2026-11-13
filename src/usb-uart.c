/*
             LUFA Library
     Copyright (C) Dean Camera, 2021.

  dean [at] fourwalledcubicle [dot] com
           www.lufa-lib.org
*/

/*
  Copyright 2021  Dean Camera (dean [at] fourwalledcubicle [dot] com)

  Permission to use, copy, modify, distribute, and sell this
  software and its documentation for any purpose is hereby granted
  without fee, provided that the above copyright notice appear in
  all copies and that both that the copyright notice and this
  permission notice and warranty disclaimer appear in supporting
  documentation, and that the name of the author not be used in
  advertising or publicity pertaining to distribution of the
  software without specific, written prior permission.

  The author disclaims all warranties with regard to this
  software, including all implied warranties of merchantability
  and fitness.  In no event shall the author be liable for any
  special, indirect or consequential damages or any damages
  whatsoever resulting from loss of use, data or profits, whether
  in an action of contract, negligence or other tortious action,
  arising out of or in connection with the use or performance of
  this software.
*/

/** \file
 *
 *  USB-UART adapter firmware for the Teensy 2.0 (ATmega32U4).
 *
 *  This is the Arduino Uno's ATmega16U2 firmware (Arduino-usbserial, itself LUFA's
 *  USBtoSerial project plus activity LEDs) ported to the 32U4 and to LUFA 210130.
 *  Differences from Arduino-usbserial:
 *   - the USART is never written to blocking, so the USB tasks keep running while
 *     the far end holds CTS off;
 *   - optional RTS/CTS hardware flow control (HW_FLOW_CONTROL);
 *   - one activity LED for both directions, the Teensy's only LED (Board.h);
 *   - no DTR-driven reset line and no 57600 baud special case, which exist on the
 *     Uno only to serve the ATmega328 behind the 16U2.
 */

#include "usb-uart.h"

/** Circular buffer to hold data from the host before it is sent to the device via the serial port. */
static RingBuffer_t USBtoUSART_Buffer;

/** Underlying data buffer for \ref USBtoUSART_Buffer, where the stored bytes are located. */
static uint8_t      USBtoUSART_Buffer_Data[128];

/** Circular buffer to hold data from the serial port before it is sent to the host. */
static RingBuffer_t USARTtoUSB_Buffer;

/** Underlying data buffer for \ref USARTtoUSB_Buffer, where the stored bytes are located. */
static uint8_t      USARTtoUSB_Buffer_Data[128];

/** Timer 0 overflows remaining for the activity LED pulse. Used by the main loop only. */
static uint8_t LEDPulseTicksRemaining;

/** LUFA CDC Class driver interface configuration and state information. This structure is
 *  passed to all CDC Class driver functions, so that multiple instances of the same class
 *  within a device can be differentiated from one another.
 */
USB_ClassInfo_CDC_Device_t VirtualSerial_CDC_Interface =
	{
		.Config =
			{
				.ControlInterfaceNumber         = INTERFACE_ID_CDC_CCI,
				.DataINEndpoint                 =
					{
						.Address                = CDC_TX_EPADDR,
						.Size                   = CDC_TXRX_EPSIZE,
						.Banks                  = 1,
					},
				.DataOUTEndpoint                =
					{
						.Address                = CDC_RX_EPADDR,
						.Size                   = CDC_TXRX_EPSIZE,
						.Banks                  = 1,
					},
				.NotificationEndpoint           =
					{
						.Address                = CDC_NOTIFICATION_EPADDR,
						.Size                   = CDC_NOTIFICATION_EPSIZE,
						.Banks                  = 1,
					},
			},
	};


/** Main program entry point. This routine contains the overall program flow, including initial
 *  setup of all components and the main program loop.
 */
int main(void)
{
	SetupHardware();

	RingBuffer_InitBuffer(&USBtoUSART_Buffer, USBtoUSART_Buffer_Data, sizeof(USBtoUSART_Buffer_Data));
	RingBuffer_InitBuffer(&USARTtoUSB_Buffer, USARTtoUSB_Buffer_Data, sizeof(USARTtoUSB_Buffer_Data));

	GlobalInterruptEnable();

	for (;;)
	{
		/* Only try to read in bytes from the CDC interface if the transmit buffer is not full.
		 * When it is full the OUT endpoint is left unread, so the host is NAKed and waits. */
		if (!(RingBuffer_IsFull(&USBtoUSART_Buffer)))
		{
			int16_t ReceivedByte = CDC_Device_ReceiveByte(&VirtualSerial_CDC_Interface);

			/* Store received byte into the USART transmit buffer */
			if (!(ReceivedByte < 0))
			  RingBuffer_Insert(&USBtoUSART_Buffer, ReceivedByte);
		}

		uint16_t BufferCount = RingBuffer_GetCount(&USARTtoUSB_Buffer);
		if (BufferCount)
		{
			Endpoint_SelectEndpoint(VirtualSerial_CDC_Interface.Config.DataINEndpoint.Address);

			/* Check if a packet is already enqueued to the host - if so, we shouldn't try to send more data
			 * until it completes as there is a chance nothing is listening and a lengthy timeout could occur */
			if (Endpoint_IsINReady())
			{
				/* Never send more than one bank size less one byte to the host at a time, so that we don't block
				 * while a Zero Length Packet (ZLP) to terminate the transfer is sent if the host isn't listening */
				uint8_t BytesToSend = MIN(BufferCount, (CDC_TXRX_EPSIZE - 1));

				LED_Set(1);
				LEDPulseTicksRemaining = LED_PULSE_TICKS;

				/* Read bytes from the USART receive buffer into the USB IN endpoint */
				while (BytesToSend--)
				{
					/* Try to send the next byte of data to the host, abort if there is an error without dequeuing */
					if (CDC_Device_SendByte(&VirtualSerial_CDC_Interface,
											RingBuffer_Peek(&USARTtoUSB_Buffer)) != ENDPOINT_READYWAIT_NoError)
					{
						break;
					}

					/* Dequeue the already sent byte from the buffer now we have confirmed that no transmission error occurred */
					RingBuffer_Remove(&USARTtoUSB_Buffer);
				}
			}
		}

		/* The receive ISR switches itself off when the USART receive buffer is full (see the ISR).
		 * Switch it back on once there is room again. */
		if (!(UCSR1B & (1 << RXCIE1)) && !(RingBuffer_IsFull(&USARTtoUSB_Buffer)))
		{
			ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
			{
				if (UCSR1B & (1 << RXEN1))
				  UCSR1B |= (1 << RXCIE1);
			}
		}

		/* Load the next byte from the USART transmit buffer into the USART, but only if its data register is
		 * free. This must not block: with flow control on, a byte waits in the data register for as long as
		 * the far end holds CTS off, and the USB tasks below have to keep running meanwhile. */
		if (Serial_IsSendReady() && !(RingBuffer_IsEmpty(&USBtoUSART_Buffer)))
		{
			Serial_SendByte(RingBuffer_Remove(&USBtoUSART_Buffer));

			LED_Set(1);
			LEDPulseTicksRemaining = LED_PULSE_TICKS;
		}

		/* Time out the activity LED pulse on each Timer 0 overflow */
		if (TIFR0 & (1 << TOV0))
		{
			TIFR0 = (1 << TOV0);

			if (LEDPulseTicksRemaining && !(--LEDPulseTicksRemaining))
			  LED_Set(0);
		}

		CDC_Device_USBTask(&VirtualSerial_CDC_Interface);
		USB_USBTask();
	}
}

/** Configures the board hardware and chip peripherals for the demo's functionality. */
void SetupHardware(void)
{
	/* Disable watchdog if enabled by bootloader/fuses */
	MCUSR &= ~(1 << WDRF);
	wdt_disable();

	/* Disable clock division */
	clock_prescale_set(clock_div_1);

	/* Hardware Initialization */
	Board_LED_Init();

#if defined(HW_FLOW_CONTROL)
	/* Until the host opens the port and the USART is switched on, hold RTS high (negated) as a plain
	 * output so the far end does not send, and pull CTS up so an unconnected CTS reads as "stop". */
	RTS_PORT |= RTS_MASK;
	RTS_DDR  |= RTS_MASK;
	CTS_DDR  &= ~CTS_MASK;
	CTS_PORT |= CTS_MASK;

	/* Hand both pins to the USART. CTSEN: the transmitter starts a character only while CTS is low; it
	 * samples CTS when a byte is written and in the middle of the last stop bit of the character being
	 * sent. RTSEN: RTS goes high when the USART's own two-byte receive buffer is full. */
	UCSR1D = ((1 << CTSEN) | (1 << RTSEN));
#endif

	USB_Init();

	/* Start Timer 0 (clk/256, overflow every 4.096 ms) as the timebase for the LED pulse */
	TCCR0B = (1 << CS02);
}

/** Event handler for the library USB Configuration Changed event. */
void EVENT_USB_Device_ConfigurationChanged(void)
{
	CDC_Device_ConfigureEndpoints(&VirtualSerial_CDC_Interface);
}

/** Event handler for the library USB Control Request reception event. */
void EVENT_USB_Device_ControlRequest(void)
{
	CDC_Device_ProcessControlRequest(&VirtualSerial_CDC_Interface);
}

/** ISR to manage the reception of data from the serial port, placing received bytes into a circular buffer
 *  for later transmission to the host.
 *
 *  When the circular buffer is full the byte is left in the USART and this interrupt is switched off; the
 *  main loop switches it back on once the host has taken some data. Meanwhile the USART's own two-byte
 *  receive buffer fills, and with HW_FLOW_CONTROL the USART then raises RTS to stop the far end. Without
 *  flow control the USART overruns and data is lost, as it would be on any adapter.
 */
ISR(USART1_RX_vect, ISR_BLOCK)
{
	if (USB_DeviceState != DEVICE_STATE_Configured)
	{
		/* Nobody to pass it on to: discard */
		(void)UDR1;
		return;
	}

	if (RingBuffer_IsFull(&USARTtoUSB_Buffer))
	{
		UCSR1B &= ~(1 << RXCIE1);
		return;
	}

	RingBuffer_Insert(&USARTtoUSB_Buffer, UDR1);
}

/** Event handler for the CDC Class driver Line Encoding Changed event.
 *
 *  \param[in] CDCInterfaceInfo  Pointer to the CDC class interface configuration structure being referenced
 */
void EVENT_CDC_Device_LineEncodingChanged(USB_ClassInfo_CDC_Device_t* const CDCInterfaceInfo)
{
	uint8_t ConfigMask = 0;

	switch (CDCInterfaceInfo->State.LineEncoding.ParityType)
	{
		case CDC_PARITY_Odd:
			ConfigMask = ((1 << UPM11) | (1 << UPM10));
			break;
		case CDC_PARITY_Even:
			ConfigMask = (1 << UPM11);
			break;
	}

	if (CDCInterfaceInfo->State.LineEncoding.CharFormat == CDC_LINEENCODING_TwoStopBits)
	  ConfigMask |= (1 << USBS1);

	switch (CDCInterfaceInfo->State.LineEncoding.DataBits)
	{
		case 6:
			ConfigMask |= (1 << UCSZ10);
			break;
		case 7:
			ConfigMask |= (1 << UCSZ11);
			break;
		case 8:
			ConfigMask |= ((1 << UCSZ11) | (1 << UCSZ10));
			break;
	}

	/* Keep the TX line held high (idle) while the USART is reconfigured */
	PORTD |= (1 << 3);

	/* Must turn off USART before reconfiguring it, otherwise incorrect operation may occur */
	UCSR1B = 0;
	UCSR1A = 0;
	UCSR1C = 0;

	/* Set the new baud rate before configuring the USART */
	UBRR1  = SERIAL_2X_UBBRVAL(CDCInterfaceInfo->State.LineEncoding.BaudRateBPS);

	/* Reconfigure the USART in double speed mode for a wider baud rate range at the expense of accuracy */
	UCSR1C = ConfigMask;
	UCSR1A = (1 << U2X1);
	UCSR1B = ((1 << RXCIE1) | (1 << TXEN1) | (1 << RXEN1));

	/* Release the TX line after the USART has been reconfigured */
	PORTD &= ~(1 << 3);
}
