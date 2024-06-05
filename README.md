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

7. Use a regular dry-contact doorbell switch to transmit the "doorbell pressed" message to the Paxton Net2 ACU.

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
Type the number and press the enter key on the remote.  The 5-digit feature code is influenced
by what digits would spell the name of the feature if it were a touch-tone keypad.

# Compiling
Compile this sketch for Arduino Mega 2560.  It has the following library dependencies:
* Adafruit NeoPixel
* Adafruit SSD1306

# Connecting things

## Wiegand RFID card reader
A Wiegand card reader connects to the top-left 8-terminal port (J3), as follows:
* +12V
* Wiegand Data D0 (GPIO18)
* Wiegand Data D1 (GPIO19)
* LED (GPIO12)
* Beep (GPIO11)
* N/C (A8)
* Bell button (A9)
* GND

If your reader doesn't have a feature, or a wire for it, then leave the terminal unconnected.  If your reader has
a wire that selects 26 or 34 bit Wiegand, this wire should be tied with the ground wire, and both connected
to GND (we always want to receive the largest possible bit count).

Use the bottom-left 8-terminal port (J4) to connect to the Paxton Net2 module, as follows:
* N/C (Do not connect +12V to Paxton)
* Paxton D0/Data
* Paxton D1/Clock
* Paxton Red LED Connection
* N/C
* N/C
* N/C
* Paxton Ground

The programming code to enable this feature is 87267114.  To disable it, 87267000.

## Doorbell button
The Paxton Net2 system supports a doorbell button (which is present on their PIN keypads).  The doorbell button
is transmitted over the same wire as the card swipes and key presses.

The programming code 32355008 enables the pins A8+A9 (or A8+GND) to be a contact closure doorbell connection.
When enabled, the board will transmit a doorbell keypress message to the Paxton ACU whenever the
contacts are closed.

Sometimes Wiegand RFID keypads provide a doorbell button -- and if there are two doorbell wires in the
wiring harness, then it's a good indication that it is just a dry contact switch.

## Door contact
Connect to A15 and GND on the lower right side of the board (J12).

If it's a double door, use A15 for one door, and A14 for the other door.

The selection of a "door program" enables the behavior of detecting whether the door(s) are
closed, and whether to allow the motion detector input to cut the lock current (via Relay 1).

The programming code to enable door contact sensing is:
36667010 = Single Door, Sense Closed When Sense_A (A15) Input Grounded, Sense Locked via Current Detection
36667011 = Single Door, Sense (Not Closed) Open When Sense_A Input Grounded, Sense Locked via Current Detection
36667012 = Double Door, Sense A&B (A15&A14), Door is Closed when Input Grounded, Report Locked via Current Detection
36667013 = Double Door, Sense A&B, Door is Open when Input Grounded, Report Locked via Current Detection
36667014 = Single Door, Sense Closed when Sense_A Grounded, Sense Locked when Sense_B Input Grounded 
36667015 = Single Door, Sense Closed when Sense_A Not Grounded, Sense Locked when Sense_B Input Not Grounded

When the board "senses" the door closed or locked, this status information can be routed
elsewhere.  For example, you can configure any of the relays to energize when "the door is closed".

## Motion detector
Connect to middle right side of the board (J10).  +12V and GND are provided for powering
the motion detector.  The motion detector should connect (or ground) GPIO16 and GPIO17
when motion is detected.

The motion detector program doesn't require a feature code -- it's active any time a door program
is active, or a relay program that references the motion detector.

## Door lock
Connect this to Relay 1 (J8).  It's best to use the "normally closed" connection.
The motion detector and door program will energize this relay when unlocking the door is
desired due to motion detection.

## Relay program
This feature allows you to configure any of the 4 onboard relays to mirror some sensed
status on board.


