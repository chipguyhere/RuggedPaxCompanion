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

static bool feature_enabled;
static byte feature_cfg;

static displayPage *featuredisplayPage;

static long lastRing;
static long lastRelease;
static bool everRung=false;
static bool everReleased=false;


static void doorbellButton::setup() {
  feature_cfg = eepromconfig::get_doorbell_option();

  switch (feature_cfg) {
  case 8:
    addDisplayPage(new displayPage(F( "Doorbell program 8:\n"
                                          " A8+A9 bell switch,\n"
                                          " Bell sent as bell\n"
                                          " keypress to Paxton")));
    pinMode(A8, OUTPUT);
    digitalWrite(A8, LOW);                                          
    break;

  case 9:
    addDisplayPage(new displayPage(F( "Doorbell program 9:\n"
                                          " A9+GND bell switch,\n"
                                          " Bell sent as bell\n"
                                          " keypress to Paxton"))); break;

  case 18:
    addDisplayPage(new displayPage(F( "Doorbell program 18:\n"
                                          " A8+A9 bell switch,\n"
                                          " pressing doorbell\n"
                                          " stops LeftOpen beep")));
    pinMode(A8, OUTPUT);
    digitalWrite(A8, LOW);                                          
    break;

  case 19:
    addDisplayPage(new displayPage(F( "Doorbell program 9:\n"
                                          " A9+GND bell switch,\n"
                                          " pressing doorbell\n"
                                          " stops LeftOpen beep"))); break;

  default:
    feature_cfg=0;
    return;
  }

  featuredisplayPage = new displayPage(F("Doorbell status:\n"));
  addDisplayPage(featuredisplayPage);
  featuredisplayPage->msg = malloc(50);
  strcpy_P(featuredisplayPage->msg, PSTR(""));

  pinMode(A9, INPUT_PULLUP);
  feature_enabled=true;
}

bool inhibitLeftOpenBeep(void);
void paxtonSendBell();

static void doorbellButton::loop() {
  if (!feature_enabled) return;

  static long lastCheck;
  static bool lastPressed;
  long m = millis();
  if (m - lastCheck > 100) {
    lastCheck = m;
    bool pressed = digitalRead(A9)==LOW;
    if (lastPressed==false && pressed) {
      lastRing=m;
      everRung=true;
      if (feature_cfg == 18 || feature_cfg == 19) {
        inhibitLeftOpenBeep();
      } else {
        paxtonSendBell();
      }
    } else if (lastPressed==true && !pressed) {
      lastRelease=m;
      everReleased=true;
    }
    lastPressed = pressed;
  }
  if (everRung && m-lastRing>600000) everRung=false,lastRing=1;
  if (everReleased && m-lastRelease>600000) everReleased=false,lastRelease=1;
  if (everRung==false)
    if (lastRing==1) strcpy_P(featuredisplayPage->msg, PSTR("Last ring 10m+ ago\n"));
    else strcpy_P(featuredisplayPage->msg, PSTR("No press since boot\n"));
  else sprintf_P(featuredisplayPage->msg, PSTR("Last pressed:\n %d sec ago\n"), (m-lastRelease)/1000L);
  if (lastPressed) strcat_P(featuredisplayPage->msg, PSTR("PRESSED"));

}
