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


#include "RuggedPax.h"


// Door Manager
// Manages tracking whether the door is open or closed,
// and whether to enable the motion detector as a result.

// Door Options:
// 10 = Single Door, Door is Closed When Sense_A (A15) Input Grounded, Report Locked via Current Detection
// 11 = Single Door, Door is Open When Sense_A Input Grounded, Report Locked via Current Detection
// 12 = Double Door, Sense A&B, Door is Closed when Input Grounded, Report Locked via Current Detection
// 13 = Double Door, Sense A&B, Door is Open when Input Grounded, Report Locked via Current Detection
// 14 = Single Door, Report Closed when Sense_A Grounded, Report Locked when Sense_B Input Grounded 
// 15 = Single Door, Report Closed when Sense_A Not Grounded, Report Locked when Sense_B Input Not Grounded
// 0 or 255 = no program



bool doorman::doorsClosed = false;
bool doorman::doorsOpen = false; 
bool doorman::doorsPartlyOpen = false;


char lastDoorState=0;
long lastDoorStateStamp=0;

//extern char display_version[16];
extern volatile bool believedLocked;

#define SECONDS_TO_IGNORE_MOTION_AFTER_DOOR_CLOSE 22

//#define EXIT_BUTTON_SENSE_INPUT A13
#define DOOR_CLOSE_SENSE_A A15
#define DOOR_CLOSE_SENSE_B A14
//#define CONTACT_OUTPUT A7
//#define PAXTON_ALARM_INPUT A9


static bool doorman::feature_enabled=false;

displayPage *doordisplayPage;

static void doorman::activateMotionSensing() {
  static bool isActive;
  if (isActive) return;
  isActive=true;
  pinMode(MOTION_DETECTOR_SENSE_INPUT, INPUT_PULLUP);
  pinMode(MOTION_DETECTOR_CONVENIENCE_GROUND, OUTPUT);
  digitalWrite(MOTION_DETECTOR_CONVENIENCE_GROUND, LOW);

  auto dp = addDisplayPage(new displayPage(F("Motion detector input\n"
                                       " enabled\n\nHold for details")));


  dp->addLongPressDisplayPage(new displayPage(F("Motion detector input\n"
                                                " enabled: GPIO16 to\n"
                                                " ground or to GPIO17\n"
                                                " indicates motion.")));
}

static doorman::setup() {
  byte cfgdo = eepromconfig::get_dooroption();
  if (cfgdo < 10 || cfgdo > 15) return; // doorman isn't configured.

  doorman::feature_enabled=true;


  doorman::activateMotionSensing();

  displayPage *dp = NULL;

  switch (cfgdo) {
  case 10:
      dp = new displayPage(F( "Door program 10:\n"
                                            " Single Door,\n"
                                            " Closed if A15 to GND\n"
                                            " Locked if cur sensed"));   break;
  case 11:
      dp = new displayPage(F( "Door program 11:\n"
                                            " Single Door,\n"
                                            " Closed if A15 notGND\n"
                                            " Locked if cur sensed"));   break;
  case 12:
      dp = new displayPage(F( "Door program 12:\n"
                                            " Double Door, A14+A15\n"
                                            " Closed if input GND\n"
                                            " Locked if cur sensed"));   break;
  case 13:
      dp = new displayPage(F( "Door program 13:\n"
                                            " Double Door, A14+A15\n"
                                            " Closed if notGND\n"
                                            " Locked if cur sensed"));   break;
  case 14:
      dp = new displayPage(F( "Door program 14:\n"
                                            " Single Door,\n"
                                            " Closed if A15 GND\n"
                                            " Locked if A14 GND"));   break;

  case 15:
      dp = new displayPage(F( "Door program 15:\n"
                                            " Single Door,\n"
                                            " Closed A15 notGND\n"
                                            " Locked A14 notGND"));   break;

  }

  if (dp != NULL) {
    auto dp1 = addDisplayPage(new displayPage(F("Door program is\nactive.\n\nHold for details")));
    dp1->addLongPressDisplayPage(dp);
  }


  doordisplayPage = new displayPage(F("Door status\n "));
  doordisplayPage->msg = malloc(30);
  doordisplayPage->msg[0]=0;
  diagnosticsPage->addDisplayPage(doordisplayPage);

  for (byte i=0; i<4; i++) {
    byte cfg = eepromconfig::get_relayprogram(i+1);
    char buf[150];
    if (cfg==35) {
      sprintf_P(buf, PSTR("Relay%d program 35:\n"
                          " energize when door\n"
                          " not closed or motion\n"
                          " detected"), i+1);
      relayPrograms::addRelayDetailPage(new displayPage(buf));
      pinMode(FIRST_RELAY_GPIO+i, OUTPUT);
    } else if (cfg==36) {
      sprintf_P(buf, PSTR("Relay%d program 36:\n"
                          " energize when door\n"
                          " detected as closed"), i+1);
      relayPrograms::addRelayDetailPage(new displayPage(buf));
      pinMode(FIRST_RELAY_GPIO+i, OUTPUT);
    } else if (cfg==37) {
      sprintf_P(buf, PSTR("Relay%d program 37:\n"
                          " energize when door\n"
                          " detected as locked"), i+1);
      relayPrograms::addRelayDetailPage(new displayPage(buf));
      pinMode(FIRST_RELAY_GPIO+i, OUTPUT);
    }
  }

  pinMode(DOOR_CLOSE_SENSE_A, INPUT_PULLUP);
  pinMode(DOOR_CLOSE_SENSE_B, INPUT_PULLUP);

}

static doorman::loop() {
  if (!doorman::feature_enabled) return;

  static long last_doors_checked_stamp = 0;
  if ((millis() - last_doors_checked_stamp) > 100) {
    last_doors_checked_stamp += 100;

    bool doorAclosed = true;
    bool doorBclosed = true;
    bool doorLocked=false;

    byte cfgdo = eepromconfig::get_dooroption();
    
    switch (cfgdo) {
      case 10:
      case 14:
        doorAclosed = (analogRead(DOOR_CLOSE_SENSE_A) < 128);
        doorBclosed = doorAclosed;
        if (cfgdo==14) doorLocked = (analogRead(DOOR_CLOSE_SENSE_B) < 128);
        else doorLocked=believedLocked;
        break;
      
      case 11:
      case 15:
        doorAclosed = (analogRead(DOOR_CLOSE_SENSE_A) >= 128);
        doorBclosed = doorAclosed;
        if (cfgdo==15) doorLocked = (analogRead(DOOR_CLOSE_SENSE_B) >= 128);
        else doorLocked=believedLocked;

        break;
      
      case 12:
      case 16:
        doorAclosed = (analogRead(DOOR_CLOSE_SENSE_A) < 128);
        doorBclosed = (analogRead(DOOR_CLOSE_SENSE_B) < 128);
        doorLocked=believedLocked;
        break;
      
      case 13:
      case 17:
        doorAclosed = (analogRead(DOOR_CLOSE_SENSE_A) >= 128);
        doorBclosed = (analogRead(DOOR_CLOSE_SENSE_B) >= 128);
        doorLocked=believedLocked;
        break;
    }
  
    doorman::doorsClosed = doorAclosed && doorBclosed;
    doorman::doorsOpen = (doorAclosed==false && doorBclosed==false); 
    doorman::doorsPartlyOpen = (doorAclosed != doorBclosed);

    char *doorstatustext = doordisplayPage->msg;
    doorstatustext[0]=0;
    if (doorman::doorsClosed) strcpy_P(doorstatustext, PSTR("Closed   "));
    else if (doorman::doorsOpen) strcpy_P(doorstatustext, PSTR("Open     "));
    else if (doorman::doorsPartlyOpen) strcpy_P(doorstatustext, PSTR("PartOpen "));
    if (doorLocked) strcat_P(doorstatustext, PSTR("Locked\n "));
    else strcat_P(doorstatustext, PSTR("\n "));



    bool enableMotionDetector=false;
    bool allowLocking=true;

    // POSSIBLE DOOR STATES:
    // 0 (zero) = status at boot
    // O = open
    // o = partly open after having been open
    // C = closed
    // c = partly open after having been closed
    // L = closed for 22+ seconds (and unlockable via motion)
    // l = partly open after having been status L (switch to c)

    // LOOK FOR CHANGES IN THE DOOR STATE, AND (if applicable) WHETHER
    // THE STATE HAS STAYED THE SAME for a certain number of seconds

    char doorState=lastDoorState;
    switch (lastDoorState) {
    case 0:
      if (doorman::doorsClosed) doorState='C';
      if (doorman::doorsOpen) doorState='O';
      if (doorman::doorsPartlyOpen) doorState='o';
      break;      
    case 'O':
      if (doorman::doorsPartlyOpen) doorState='o';
      else if (doorman::doorsClosed) doorState='C';
      break;
    case 'o':
      // continue
    case 'c':
      if (doorsOpen) doorState='O';
      else if (doorman::doorsClosed) doorState='C';
      break;
    case 'C':
      if (doorman::doorsPartlyOpen) doorState='c';
      else if (doorman::doorsOpen) doorState='O';
      else if ((millis() - lastDoorStateStamp) >= 1000*SECONDS_TO_IGNORE_MOTION_AFTER_DOOR_CLOSE) {
        doorState='L'; 
      }
      break;
    case 'L':
      if (doorman::doorsPartlyOpen) doorState='l';
      else if (doorman::doorsOpen) doorState='O';
      break;
    case 'l':
      if (doorman::doorsClosed) doorState='L';
      else if (doorman::doorsOpen) doorState='O';
      else if ((millis() - lastDoorStateStamp) >= 1000*SECONDS_TO_IGNORE_MOTION_AFTER_DOOR_CLOSE) {
        doorState='c'; 
      }
      break;    
    }

    if (lastDoorState != doorState) {
      lastDoorState = doorState;
      Serial.println(doorState);
      lastDoorStateStamp = millis();
    } else {
      // set a maximum age on the stamp to avoid issues at age 2^31ms
      if ((millis() - lastDoorStateStamp) > 10000000) lastDoorStateStamp += 10000;
    }

    // Enable motion detector unlock, if we believe the door has been locked for 22sec period.
    if (doorState=='l' || doorState=='L') enableMotionDetector=true;

    // Inhibit locking the door if we think the door isn't closed.
    if (doorState=='O' || doorState=='o' || doorState=='c') allowLocking=false;
    
    // Report if we think the door is closed, to the Paxton (via its Contact pin)
    /* temporarily disabling this to see if I will actually ever hook this up, and decide where and how.
    if (doorsClosed && (cfgdo < 14 || doorLocked)) {
      pinMode(CONTACT_OUTPUT, OUTPUT);
      digitalWrite(CONTACT_OUTPUT, LOW);    
    } else {
      pinMode(CONTACT_OUTPUT, INPUT);
    }
    */

    // add the status letter to the door status text.
    char statestr[3] = {doorState, ' ', 0};
    strcat(doorstatustext, statestr);


    
    bool activateMotionCutoff = enableMotionDetector;
    bool motionDetectorSenseInputActive = digitalRead(MOTION_DETECTOR_SENSE_INPUT)==LOW;
//    display_version[10] = motionDetectorSenseInputActive ? '1' : '0';
    if (motionDetectorSenseInputActive==false) activateMotionCutoff=false;

    if (motionDetectorSenseInputActive) strcat_P(doorstatustext, PSTR("Motion"));
    if (activateMotionCutoff) strcat_P(doorstatustext, PSTR("+Cutoff"));

    // Set relays to indicate door closed and locked status.
    for (byte i=0; i<4; i++) {
      byte cfg = eepromconfig::get_relayprogram(i+1);
      if (cfg==35) {
        // MOTION_LOCK_CUTOFF_OUTPUT
        pinMode(FIRST_RELAY_GPIO+i, OUTPUT);
        digitalWrite(FIRST_RELAY_GPIO+i, (activateMotionCutoff || (allowLocking==false)) ? HIGH : LOW);
      } else if (cfg==36) {
        // DOOR_CLOSED_OUTPUT
        pinMode(FIRST_RELAY_GPIO+i, OUTPUT);
        digitalWrite(FIRST_RELAY_GPIO+i, doorsClosed ? HIGH : LOW);
      } else if (cfg==37) {
        // DOOR_LOCKED_OUTPUT
        pinMode(FIRST_RELAY_GPIO+i, OUTPUT);
        digitalWrite(FIRST_RELAY_GPIO+i, (doorLocked ? HIGH : LOW));
      }
    }
  }  
}
