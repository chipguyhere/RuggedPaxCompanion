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

#include "Arduino.h"
#include "RuggedPax.h"


#define READER_BEEP_PIN 11

volatile byte beep100msperiodsleft=0;
volatile byte beepticksleft=0;
volatile byte beepsleft=0;


static bool feature_enabled=false;
bool inhibited_with_star_key=false;


// gets called 16000000/16384 times per second,
// or 976+9/16 times per second.
static void leftOpenBeep::timer0_compA_isr() {
  if (beepticksleft>0) {
    beepticksleft--;
    return;
  }

  // Beep will only happen while we are on odd-numbered counts of "100 ms periods left".
  // On the even count (including 0), we de-assert the beep pin.
  if ((beep100msperiodsleft & 1)==0) pinMode(READER_BEEP_PIN, INPUT);

  // On the count becoming odd, set the beep pin low, triggering the reader's beep.
  // On running out of ticks, consider another 100ms passed, time another 100ms.
  if (beep100msperiodsleft) {
    beep100msperiodsleft--;
    beepticksleft=98; // 976/10
    if ((beep100msperiodsleft & 1)) {
      pinMode(READER_BEEP_PIN, OUTPUT);
      digitalWrite(READER_BEEP_PIN, LOW);
    }
  }
}

extern bool (*star_key_handler)(void);
static bool (*old_star_key_handler(void));
bool inhibitLeftOpenBeep(void) {
  inhibited_with_star_key=true;
  return true;
}

static void leftOpenBeep::setup() {
  byte cfgdo = eepromconfig::get_leftopenbeepoption();
  if (cfgdo != 30) return;

  feature_enabled=true;
  star_key_handler = inhibitLeftOpenBeep;


  displayPage *dp = addDisplayPage(new displayPage(F( "LeftOpen Warning Beep\nprogram is active.\n\nHold for details")));

  auto dp1 = dp->addLongPressDisplayPage(new displayPage(F( "LeftOpen program 30:\n"
                                                            "Beep every 30 seconds\n"
                                                            " while door is left\n"
                                                            " open.")));

  dp1->addDisplayPage(new displayPage(F( "LeftOpen program 30:\n"
                                        " The reader/keypad\n"
                                        " shall beep when\n"
                                        " GPIO11 is set LOW.")));

  dp1->addDisplayPage(new displayPage(F( "LeftOpen program 30:\n"
                                        " Pressing * or ESC\n"
                                        " on PIN keypad stops\n"
                                        " LeftOpen beeping.")));

}

// Trigger the beep by setting the period counters
// so that on the next interrupt, the beep pin will be turned on.
static void doBeep() {
  if (!feature_enabled) return;
  uint8_t oldSREG = SREG;
  cli();
  beep100msperiodsleft=10;
  beepticksleft=0;
  SREG = oldSREG;  
}

static void leftOpenBeep::loop() {
  if (!feature_enabled) return;

  static bool doorsWereOpen;
  static long doorsWereOpenSinceWhen;
  long m=millis();
  if (doorman::doorsClosed) {
    inhibited_with_star_key=false;
    beepsleft=5;
    doorsWereOpen=false;
  } else {
    // door is / doors are open.
    if (doorsWereOpen==false) {
      doorsWereOpen=true;
      doorsWereOpenSinceWhen=m;
    }
    if (m - doorsWereOpenSinceWhen > 30000) {
      if (!inhibited_with_star_key) {
        if (beepsleft) doBeep();
        if (beepsleft > 0 && beepsleft < 255) beepsleft--;
      }
      doorsWereOpenSinceWhen=m;
    }
  }
}

