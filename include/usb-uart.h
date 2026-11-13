/*
  Header for usb-uart.c. Derived from Arduino-usbserial.h (the Arduino Uno's
  ATmega16U2 firmware) and LUFA's USBtoSerial.h; see usb-uart.c for the licence.
*/

#ifndef _USB_UART_H_
#define _USB_UART_H_

	/* Includes: */
		#include <avr/io.h>
		#include <avr/wdt.h>
		#include <avr/interrupt.h>
		#include <avr/power.h>
		#include <util/atomic.h>

		#include "Descriptors.h"
		#include "Board.h"

		#include <LUFA/Drivers/Peripheral/Serial.h>
		#include <LUFA/Drivers/Misc/RingBuffer.h>
		#include <LUFA/Drivers/USB/USB.h>
		#include <LUFA/Platform/Platform.h>

	/* Macros: */
		/** Length of an activity LED pulse, in Timer 0 overflows (4.096 ms each at 16 MHz). */
		#define LED_PULSE_TICKS   3

	/* Function Prototypes: */
		void SetupHardware(void);

		void EVENT_USB_Device_ConfigurationChanged(void);
		void EVENT_USB_Device_ControlRequest(void);

		void EVENT_CDC_Device_LineEncodingChanged(USB_ClassInfo_CDC_Device_t* const CDCInterfaceInfo);

#endif
