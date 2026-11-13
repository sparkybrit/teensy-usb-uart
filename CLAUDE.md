This is a project to house the firmware to support a Teensy 2.0 (ATmega32U4) used as a USB-UART adapter.

**The target is the Teensy 2.0, not the Teensy++ 2.0 and not the Arduino Micro.** The project began
for the Arduino Micro; Graeme switched it to the Teensy 2.0 on 2026-11-13 and the folder was renamed
from `arduino-micro-usb-uart` to `teensy-usb-uart` the same day.

## Goal

Use a Teensy 2.0 (ATmega32U4) as a USB-UART adapter with working hardware flow control, in
front of an Exar XR68C681 DUART (called "XR68681" below).

## The problem it solves

Streaming data to the XR68681 at 115200 baud through an FTDI module drops characters. Adding
hardware flow control did not fix it: after the XR68681 negates its RTS (the FTDI's CTS input), the
FTDI chip sends up to 3 more characters, and those overflow the XR68681's receive FIFO.

The hypothesis is that a 32U4 can stop transmitting sooner than the FTDI does, soon enough that the
FIFO does not overflow. This is not yet tested.

## Plan

1. **Firmware:** start from the USB-serial firmware that runs on the ATmega16U2 of the Arduino Uno.
   That firmware is `Arduino-usbserial`, built on LUFA (source in ArduinoCore-avr under
   `firmwares/atmegaxxu2/`). It needs porting, not just reflashing: it targets the 16U2, its LED
   pins differ, and as shipped it has no RTS/CTS handling, so flow control has to be added.
2. **Hardware:** wire the 32U4's USART1 hardware flow control pins. CTS is PD5, Teensy pin 23, a
   pad inside the board outline rather than on the edge rows. RTS is PB7, Teensy pin 4. Nothing on
   the Teensy 2.0 uses either, so no LED has to move (on the Arduino Micro the TX LED sat on PD5).
   The pin numbers are from memory of PJRC's pinout card: check them against it before soldering.

## What was built (2026-11-13)

The project is a PlatformIO project and both environments build without warnings. **Nothing has
been flashed or run on a board yet**: no Teensy was connected, so the upload and the firmware
itself are untested on hardware. `README.md` has the layout and the pin table.

- **Firmware:** `src/usb-uart.c` is the Uno's `Arduino-usbserial` ported to the 32U4. It was ported
  to the current LUFA (210130), not the 2010 release (100807) the Uno firmware was written against,
  using LUFA's current `USBtoSerial` project for the API changes. LUFA is copied unmodified into
  `lib/LUFA/LUFA`; `lib/LUFA/library.json` picks the files to compile.
- **Two environments** in `platformio.ini` (bare avr-gcc, no Arduino or Teensyduino framework):
  - `teensy2` (the default) is a plain bridge with no flow control, for proving the port with only
    TX and RX wired.
  - `teensy2_flowctl` (`-DHW_FLOW_CONTROL`) adds RTS/CTS using the 32U4's own USART1 flow-control
    hardware (`UCSR1D` = `CTSEN` | `RTSEN`), so no firmware is in the path that stops the
    transmitter. CTS is PD5 and RTS is PB7.
- **Platform:** `atmelavr` with a project-local board, `boards/teensy2_bare.json`, not PlatformIO's
  `teensy` platform. `platform = teensy` with `board = teensy2` and no framework crashes
  (platform-teensy 5.0.0, `KeyError: 'toolchain-gccarmnoneeabi'` in its `platform.py`).
- **VS Code files** (`.vscode/`) were generated with `pio project init --ide vscode`, for the
  PlatformIO toolbar's build and upload buttons and its environment switcher.

## Flashing

Flashing goes through the Teensy's own bootloader (HalfKay) with PJRC's `teensy_loader_cli`, from
PlatformIO's `tool-teensy` package; no programmer is needed.

- `~/.platformio/penv/bin/pio run -t upload`, or the upload arrow in VS Code. The loader then waits:
  **press the button on the Teensy** and it programs and restarts the board. It is the same for
  every flash.
- Add `-e teensy2_flowctl` for the flow-control build.
- Checked without a board: the upload target starts the loader with the right hex file and it
  waits. An actual program cycle is untested.

## Choices made in the port

Claude made these while porting; Graeme has not yet confirmed them, and any may change.

- **LED:** the Teensy 2.0 has one LED (PD6, pin 11). It pulses for data in either direction, where
  the Uno firmware has separate TX and RX LEDs.
- **Dropped from the Uno firmware:** the DTR-driven reset line and the 57600-baud special case,
  which only serve the ATmega328 on the Uno.
- **Non-blocking transmit:** the Uno firmware blocks while waiting to send, which would stall USB
  whenever CTS is held off. This version does not block.
- **CTS pull-up:** in `teensy2_flowctl` PD5 has its pull-up on, so an unconnected CTS reads as
  "stop" and the adapter sends nothing.
- **USB ID:** `03eb:204b`, LUFA's development ID for its USB-serial project.
- **ModemManager:** it runs on beelink0, so the USB descriptor does not advertise AT-command
  support. Otherwise ModemManager would likely probe the port and send `AT` strings to the UART.
- **Baud rate:** with a 16 MHz crystal the nearest rate to 115200 is 117647 baud, 2.1% fast. That
  is normally within tolerance, but it is untested against the XR68C681.
- **No reboot-to-bootloader over USB:** flashing always needs the button.

## Open questions

- How many characters the 32U4 sends after CTS is negated. With USART1's hardware CTS enable
  (`CTSEN` in `UCSR1D`) it should be only the character already in the shift register, but
  measure it.
- Whether one character of headroom (see the datasheet notes below) is enough in practice. If the
  XR68C681 negates RTS just as the adapter starts its next character, that character takes the
  only spare slot, so the adapter must never start a second one.

## Datasheets

Datasheets live in the sibling `datasheets` project (`../datasheets`), each as the original PDF plus
a Markdown transcription. Read the `.md`, not the PDF.

- **XR68C681 (the "XR68681" above):**
  `../datasheets/UART/Exar XR68C681v210/Exar XR68C681v210.md` (Exar Rev. 2.10, September 1999).
  The transcription is OCR'd, so check anything critical against the PDF beside it. What it says
  about flow control:
  - The receive holding register is a 3-byte FIFO behind the receive shift register (section G.2).
  - With `MR1n[7]` = 1 the receiver negates RTSn when the FIFO is full (section G.3, and H.1.1 with
    Figure 23). RTSA is OP0, RTSB is OP1.
  - After that, one more character can wait in the shift register. A further character overwrites
    it and sets the overrun bit in the status register.
  - So the sender may complete at most one character after RTS is negated. The FTDI's up-to-3
    exceeds that.
- The FTDI parts and modules (FT231X, UM232R, UB232R, UMFT231XA and others) are at the top level of
  `../datasheets`.
