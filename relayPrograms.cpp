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



static byte programSelection[4];
static displayPage *initialPage=NULL;


// Note that the Doorman module also has some of the relay programs in it,
// that switch the relays based on its own signals.

static void relayPrograms::setup() {

  for (int i=0; i<4; i++) {
    byte p = eepromconfig::get_relayprogram(i+1);
    programSelection[i] = p;
    char buf[100];
    switch (p) {
    case 8:
      sprintf_P(buf, PSTR("Relay%d program 8:\n"
                          "energized when shield\n"
                          " pin 8 to ground or\n"
                          " to pin 9"), i+1);
      addRelayDetailPage(new displayPage(buf));
      pinMode(8, INPUT_PULLUP);
      pinMode(9, OUTPUT);
      digitalWrite(9, LOW);
      pinMode(FIRST_RELAY_GPIO+i, OUTPUT);
      break;
    case 20:
      sprintf_P(buf, PSTR("Relay%d program 20:\n"
                          " energized when door\n"
                          " believed locked via\n"
                          " current sensing"), i+1);
      addRelayDetailPage(new displayPage(buf));
      pinMode(FIRST_RELAY_GPIO+i, OUTPUT);
      break;
    /*
      // implemented in Doorman
        "Relay%d program 35:\n"
        " energize when door\n"
        " not closed or motion\n"
        " detected"), i+1);
        "Relay%d program 36:\n"
        " energize when door\n"
        " detected as closed"), i+1);
        "Relay%d program 37:\n"
          " energize when door\n"
          " detected as locked"), i+1);
    */
    case 38:
      sprintf_P(buf, PSTR("Relay%d program 38:\n"
                          " energize when motion\n"
                          " sensor reports\n"
                          " motion"), i+1);
      addRelayDetailPage(new displayPage(buf));
      pinMode(FIRST_RELAY_GPIO+i, OUTPUT);
      doorman::activateMotionSensing();
      break;
    case 112:
      sprintf_P(buf, PSTR("Relay%d program 112:\n"
                          " energized when input\n"
                          " A12 is grounded\n"), i+1);      
      addRelayDetailPage(new displayPage(buf));
      pinMode(A12, INPUT_PULLUP);
      pinMode(FIRST_RELAY_GPIO+i, OUTPUT);
      break;
    default:
      programSelection[i] = 0;
      break;
    }
  }
}


static void relayPrograms::addRelayDetailPage(displayPage *dp) {
  if (initialPage==NULL) {
    initialPage = addDisplayPage(new displayPage(F("Relay program is\nactive.\n\nHold for details")));
    initialPage->addLongPressDisplayPage(dp);
  } else {
    // add subsequent page to the chain
    initialPage->onLongPress->addDisplayPage(dp);
  }
}


static void relayPrograms::loop() {
  for (byte i=0; i<4; i++) {
    byte p = programSelection[i];
    switch (p) {
    case 8:
      bool a = digitalRead(8)==LOW;
      digitalWrite(FIRST_RELAY_GPIO+i, a ? HIGH : LOW);
      break;
    case 20:
      digitalWrite(FIRST_RELAY_GPIO+i, (believedLocked && believedLockedValid) ? HIGH : LOW);    
      break;
    
    /* programs 35,36,37 depend on Doorman and are implemented in Doorman loop */
    case 38:
      bool motionDetectorSenseInputActive = digitalRead(MOTION_DETECTOR_SENSE_INPUT)==LOW;
      digitalWrite(FIRST_RELAY_GPIO+i, motionDetectorSenseInputActive ? HIGH : LOW);
      break;
    case 112:
      a = digitalRead(A12)==LOW;
      digitalWrite(FIRST_RELAY_GPIO+i, a ? HIGH : LOW);
      break;
    }
  }
}