/*
  Pin assignments for the Teensy 2.0 (ATmega32U4) as a USB-UART adapter.

  USART1:  TXD = PD3 (Teensy pin 8),  RXD = PD2 (Teensy pin 7)

  With HW_FLOW_CONTROL defined (env teensy2_flowctl):
    CTS input  = PD5  (Teensy pin 23, a pad inside the board outline, not on the edge rows)
    RTS output = PB7  (Teensy pin 4)
  Both are active low, and both are the 32U4's own USART1 flow control pins, so the
  USART hardware acts on them without firmware in the loop.

  Activity LED: the Teensy's one LED, PD6 (Teensy pin 11), active high. It pulses
  for data in either direction.
*/

#ifndef _BOARD_H_
#define _BOARD_H_

#include <avr/io.h>

#define LED_PORT     PORTD
#define LED_DDR      DDRD
#define LED_MASK     (1 << 6)

#if defined(HW_FLOW_CONTROL)
	#define CTS_PORT     PORTD
	#define CTS_DDR      DDRD
	#define CTS_MASK     (1 << 5)

	#define RTS_PORT     PORTB
	#define RTS_DDR      DDRB
	#define RTS_MASK     (1 << 7)
#endif

static inline void LED_Set(const uint8_t On)
{
	if (On)
	  LED_PORT |= LED_MASK;
	else
	  LED_PORT &= ~LED_MASK;
}

static inline void Board_LED_Init(void)
{
	LED_Set(0);
	LED_DDR |= LED_MASK;
}

#endif
