// BOARD: Ruggeduino Paxton Custom Arduino Mega2560 Board
// COMPILE AS: Arduino Mega or Mega 2560

/*
RuggedPaxCompanion Copyright 2024 Michael Caldwell-Waller (@chipguyhere), License: GPLv3

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


// This application bundles multiple features for the Ruggeduino Companion for Net2
// board to enhance a Paxton Net2 board, each of which can be enabled individually.
//
// In order to configure this, you need a generic hobby IR remote off Amazon (~$1).
//   Or most any remote with a num pad that uses NEC protocol.
//   The hobby remote support is hardcoded, but for any other remote, press
//   its 0 key, ten times from the programming mode screen, to start IR learning mode.
//
// Feature: Convert Wiegand card readers to native Paxton Reader protocol.
//   This code does the conversions so that Paxton-branded tokens read the
//   same card number as on Paxton's own readers (Wiegand minimum 34 bits), and,
//   PIN entries made on third-party Wiegand-compatible keypads work the same as
//   on Paxton keypads.  Also converts Paxton LED color outputs to the proper
//   signal for Wiegand-compatible readers.
//
// Feature: Current sensor to detect door locked status.  Status can be sent out
//   via relays, or via I2C protocol.
//
// Feature (possible depending on lock): Current sensor analysis to detect
//   situations where a lock is jammed (not effectively holding the door)
//   despite being energized.
//
// Feature: Close multiple relays in response to single Paxton relay closure.
//   Allows signaling to a security system, or switching of multiple loads.
//   Can report "door closed", "motion detected" and "door locked"
//   (among other signals) to security systems.
//
// Possible future feature: Connect up to 4 Wiegand card readers, to open 4
//   different doors using all 4 relays, using single Paxton ACU for
//   authentication and logging.
//
// Possible future feature: extend the Net2's "Turnstile Mode" so that its relay
//   clicks (on Relay 1 or 2, signaling whether Reader 1 or 2 was used) can be lengthened to
//   longer than 1 second, so the mode can be used to operate two different doors,
//   each with independent logging and identity in the Net2 system.
//
// Possible future feature: send the Alarm output of the Paxton ACU to the
//   beeper pin on Wiegand card readers.
//
// Feature: Motion detector time-in.  Prevents motion detectors from unlocking
//   doors until the door has been closed for a certain number of seconds.
//   Prevents annoying experience of having to wait for a motion detector
//   to time itself out, while trying to confirm you properly locked a door.
//   Supports two independent door switches for an option of double doors.
//
// Possible future feature: Inhibit the motion detector with a button press
//   inside the room, or a mode selectable on a PIN keypad.
//
// Possible future feature: allow a "privacy" mode that decides whether to
//   send card reads to Reader1 or Reader2 on the ACU (which can have different
//   access permissions).  Mode selected via switch or via PIN keypad.
//
// Feature: I2C slave that can be used for an outside network-enabled microcontroller
//   (such as an ESP32 running ESPHome) to retrieve status information from the board.
//   I2C address is 0x27, connect to J5 on the board (or SCL/SDA on Arduino shield port) 
//   TODO: implement workaround for where AVR doesn't play well with second master on the bus




#include <Wire.h>
#include <Watchdog.h>
#include "RuggedPax.h"




const char PROGMEM helloString[] = "Ruggeduino Companion\n for Paxton Net2\n@chipguyhere firmware\n compiled " __DATE__;
  

Watchdog watchdog;



ISR(TIMER0_COMPA_vect) { leftOpenBeep::timer0_compA_isr(); }

// Array to hold next I2C response we will give when requested
byte nextResponse[4];

// Flags we are tracking globally, and available via I2C
volatile bool believedLocked=false;
volatile bool believedLockedValid=false; // if false, the believedLocked value is invalid, e.g. fresh boot
volatile bool believedJammed=false;

// Interrupt handler for receiving an I2C Write command from the ESP32.
// In this context of I2C, Write means "Do Command".
// Command code 0x21: sample and report (on subsequent read).
// Only the single command code 0x21 is implemented.
void onI2CReceive(int bytes) {
  while (Wire.available()) {
    int c = Wire.read();
    if (c == 0x21) {
      // First byte: status of inputs A12/A13/A14/A15 (LOW active)
      // We are intentionally using two bits to send each status
      // so that the lack of a status is easy to distinguish separately.
      // 01=active 10=inactive 00=nostatus 11=nostatus
      byte bs=0;
      if (digitalRead(A15)==LOW) bs += 1; else bs += 2;
      if (digitalRead(A14)==LOW) bs += 4; else bs += 8;
      if (digitalRead(A13)==LOW) bs += 16; else bs += 32;
      if (digitalRead(A12)==LOW) bs += 64; else bs += 128;
      nextResponse[0] = bs;

      // Second byte: status of whether we believe lock is locked
      // Same 2-bit idea: if we don't think our signal is "valid" yet, we send "no status".
      bs=0;
      if (believedLockedValid) bs += (believedLocked ? 1 : 2);
      // 6 unused bits available for future use
      nextResponse[1] = bs;

      // Third byte: status of whether we think lock is jammed (i.e. ineffectively locked)
      bs=0;
      if (believedLockedValid) bs += (believedLocked && believedJammed) ? 1 : 2;
      // 6 unused bits available for future use
      nextResponse[2] = bs;

      // Fourth byte: available for future use
      nextResponse[3] = bs;
    } 
  }
}

volatile long lastI2CRequest=0;

// Interrupt handler for receiving an I2C read.
// We simply send the prepared response from the earlier Write command.
void onI2CRequest(void) {
  Wire.write(nextResponse, 4);
  lastI2CRequest=millis();
}



void setup() {
  watchdog.enable(Watchdog::TIMEOUT_8S);
  Serial.begin(115200);

  // Initialize ourselves as an I2C slave on address 0x27 so we can respond to an ESP32
  Wire.begin(); // (0x27);
  Wire.setWireTimeout();
  Wire.onReceive(onI2CReceive);
  Wire.onRequest(onI2CRequest);

  // A12-A15 are being used as door reporting (or other dry-contact-to-ground) inputs
  pinMode(A12, INPUT_PULLUP);
  pinMode(A13, INPUT_PULLUP);
  pinMode(A14, INPUT_PULLUP);
  pinMode(A15, INPUT_PULLUP);

  // Say hello, identify the application and version.
  Serial.println((__FlashStringHelper*)helloString);
  addDisplayPage(new displayPage((__FlashStringHelper*)helloString));

  auto dp = addDisplayPage(new displayPage(F("Diagnostics Mode\n\n\nHold button to enter")));
  diagnosticsPage = dp->addLongPressDisplayPage(new displayPage(F("Diagnostics Mode\n\nTap button to\nselect diagnostic")));


  // Initialize all of the separate modules.
  lcdMenus::setup();
  translateWiegand::setup();
  relayPrograms::setup();
  currentSensing::setup();
  serialconfig::setup();
  doorman::setup();
  leftOpenBeep::setup();
  doorbellButton::setup();

  // ensure the hardware button is readable
  // (so we can use it as a reset button / watchdog timer feed inhibit)
  pinMode(47, INPUT_PULLUP);

  // Activate the timer 0 overflow interrupt so we get an interrupt every 1.024ms
  // (1.024ms interval is 16000000/16384 or 976.5625 times per second)
  TIMSK0 |= _BV(OCIE0A);


}


void loop() {
  // Reset the watchdog timer, except if the top button is being held down.
  // Hold down button long enough, get a reboot via watchdog timeout.
  // button pressed is LOW, so HIGH means not pressed.
  if (digitalRead(47)==HIGH) watchdog.reset();

  // Run the loop of all the various classes.
  lcdMenus::loop();
  translateWiegand::loop();
  relayPrograms::loop();
  currentSensing::loop();
  serialconfig::loop();
  doorman::loop();
  leftOpenBeep::loop();
  doorbellButton::loop();

}
