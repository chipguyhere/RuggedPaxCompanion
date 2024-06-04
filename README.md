# RuggedPaxCompanion
This is the stock firmware for the Ruggeduino Companion board from Rugged Circuits, which is compatible with the
footprint and protocol of the Paxton Net2 access control system.

This is an Arduino-compatible board whose source code can be edited to suit specific customizations not
available in a default Net2 installation.  Several features are provided in this firmware and can be
enabled by an installer using an infrared remote control.

# Hardware Features of the Ruggeduino Companion board
* Fits perfectly behind a Paxton Net2 ACU in its native enclosure, using standard 1-inch nylon standoffs
* Atmega2560 Arduino-compatible processor
* Arduino Shield-compatible connector
* 4 programmable relays
* Current sensor on flow through Relay1
* 128x32 OLED display
* Eye sensor for IR remotes (configurator)
* Supply voltage 6 to 30 volts
* Onboard current-limited 12-volt power supply (1 amp)
* Onboard current-limited 5-volt power supply (1 amp)
* Beeper, RGB status LED, and setup button

# Some Firmware Features

1. Use generic Wiegand-compatible RFID card readers and PIN keypads, with support for variable bit
   lengths (e.g. for HID MultiClass and similar readers) and proper conversion of Paxton-branded
   tokens (so they read the same code as on Paxton readers).
   This firmware emulates the clock-and-data protocol used by Paxton card readers and keypads, so,
   simply program them as "Paxton Reader" with "Paxton Tokens".

2. Multiple relay outs.  Switch multiple loads (up to 4 relays) when access is granted.

3. Current sensor, as a way of detecting lock status.  Can be used to drive a relay out and
   inform a security system.

4. Motion detector timing program.  Avoids allowing the motion detector to unlock the door for the
   first 20 seconds of being closed, so it locks immediately upon closure.

5. Door-left-open warning beep (for RFID keypads with a beep function)

6. Two door contact inputs, and logic for handling double-doors.

# Setup

This firmware is designed to work on a board that is pre-flashed for an out-of-box experience.
The board has a single button that allows navigation through a basic menu screen.

Press the button to flip through the pages.  Some of the pages may suggest doing a long press
(~1 sec) to drill down into more detail about that feature (which is view-only).

One of the first pages is "programming mode".

In "programming mode", an infrared remote can be used to enable and configure the built-in
features.  The low-cost generic hobby remote provided in Arduino kits on Amazon is natively
supported, and a quick-learn feature allows the board to learn a new remote in case you do not
have one of these handy.

As implemented, each feature is enabled or disabled by entering an 8-digit number, consisting
of a 5-digit feature code, and then 3-digits to enable, disable, or configure the feature.
Type the number and press the enter key on the remote.  




