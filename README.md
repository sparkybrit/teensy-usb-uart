# teensy-usb-uart

Firmware that turns a Teensy 2.0 (ATmega32U4) into a USB-UART adapter with RTS/CTS hardware flow
control. It is the Arduino Uno's ATmega16U2 firmware (`Arduino-usbserial`, built on LUFA) ported
to the 32U4 and to LUFA 210130. Why it exists is in [CLAUDE.md](CLAUDE.md).

## Layout

| Path | What |
|---|---|
| `platformio.ini` | Two build environments, below. No Arduino or Teensyduino framework: bare avr-gcc. |
| `boards/teensy2_bare.json` | The Teensy 2.0 as a board for PlatformIO's `atmelavr` platform. |
| `src/usb-uart.c` | The bridge: main loop, USART receive interrupt, baud rate changes. |
| `src/Descriptors.c`, `include/Descriptors.h` | USB descriptors (CDC ACM serial port). |
| `include/Board.h` | Pin assignments: LED, CTS, RTS. |
| `include/LUFAConfig.h` | LUFA compile-time options. |
| `lib/LUFA/` | LUFA 210130, `LUFA/` directory unmodified. `library.json` picks the files to compile. |

## Environments

| Environment | Flow control |
|---|---|
| `teensy2` (default) | none |
| `teensy2_flowctl` | RTS/CTS |

Neither needs the board modified.

## Pins

| Signal | 32U4 pin | Teensy 2.0 pin | Notes |
|---|---|---|---|
| TXD | PD3 | 8 | |
| RXD | PD2 | 7 | |
| CTS (in, active low) | PD5 | 23 | `teensy2_flowctl` only. A pad inside the board outline, not on the edge rows. |
| RTS (out, active low) | PB7 | 4 | `teensy2_flowctl` only. |
| Activity LED | PD6 | 11 | The on-board LED. Pulses for data in either direction. |

Cross the pairs to the far end: TXD to its RXD, RTS to its CTS, CTS to its RTS. The Teensy 2.0's
I/O is at 5 V unless it has been converted to 3.3 V.

In `teensy2_flowctl`, CTS has a pull-up, so with nothing wired to it the adapter sends nothing.

## Build and flash

```
~/.platformio/penv/bin/pio run                            # build the default environment
~/.platformio/penv/bin/pio run -e teensy2_flowctl         # build the flow control one
~/.platformio/penv/bin/pio run -e teensy2_flowctl -t upload
```

In VS Code, the PlatformIO toolbar's environment switcher picks the environment and the arrow
uploads.

Flashing goes through the Teensy's own bootloader (HalfKay) with PJRC's `teensy_loader_cli`, so no
programmer is needed. The upload waits for the bootloader: **press the button on the Teensy** and
it programs and restarts the board. This is the same every time, whatever firmware is on it.

To go back to an ordinary Teensy, load any Teensyduino sketch (button again).

## On the host

The adapter appears as `/dev/ttyACM*`, USB ID `03eb:204b` (LUFA's ID for its USB-serial project,
for development use only). Turn on RTS/CTS in the terminal program as well, although the adapter
enforces it regardless.

16 MHz cannot produce 115200 baud exactly: the nearest rate is 117647, 2.1% fast.
