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


#include <Arduino.h>


#define CURRENT_SENSE_INPUT A6
#define FIRST_RELAY_GPIO 31
#define MOTION_DETECTOR_SENSE_INPUT 16
#define MOTION_DETECTOR_CONVENIENCE_GROUND 17



extern volatile bool believedLocked;
extern volatile bool believedLockedValid; // low for the first few seconds of startup
extern volatile bool believedJammed;



class serialconfig {
public:
  static setup();
  static loop();
};

class doorman {
public:
  static setup();
  static loop();
  static bool feature_enabled;
  static bool doorsClosed;
  static bool doorsOpen;
  static bool doorsPartlyOpen;
  // Used by the relay program that outputs the motion detector state,
  // Activates the motion detecting feature regardless of whether doorman is using or activating it itself.
  static void activateMotionSensing();
};


// The eepromconfig class retrieves values that were previously
// set and stored in EEPROM, and returns hardcoded default values
// whenever a particular option has not been set.
class eepromconfig {
  public:

  // IR codes: 87267xxx (example 87267014)
  // Translation option 14: Wiegand to Wiegand32 out GPIO14/15
  // Translation option 114: Wiegand to Paxton out GPIO14/15, LED in GPIO50 and ~out GPIO12
  // Translation option 160: Wiegand to Paxton out A0/A1, LED in A2 and ~out GPIO12
  // 0 or 255: disable
  static byte get_translationoption();
  static void set_translationoption(byte opt);

  // IR codes: 53386xxx (example 53386030) (53386 spells Lefto)
  // Option 30: Beep the Wiegand card reader every 30 seconds so long as door is not closed.
  static byte get_leftopenbeepoption();
  static void set_leftopenbeepoption(byte opt);


  // Door Manager Options
  // Door Manager determines whether the door is Open/Partial/Closed, Locked/NotLocked
  // for reporting purposes (I2C/relays), and for activating the motion sensor.
  // 10 = Single Door, Door is Closed When Sense_A (A14) Input Grounded, Report Locked via Current Detection
  // 11 = Single Door, Door is Open When Sense_A Input Grounded, Report Locked via Current Detection
  // 12 = Double Door, Sense A&B, Door is Closed when Input Grounded, Report Locked via Current Detection
  // 13 = Double Door, Sense A&B, Door is Open when Input Grounded, Report Locked via Current Detection
  // 14 = Single Door, Report Closed when Sense_A Grounded, Report Locked when Sense_B Input Grounded 
  // 15 = Single Door, Report Closed when Sense_A Not Grounded, Report Locked when Sense_B Input Not Grounded
  // 0 or 255 = no program
  static byte get_dooroption();
  static void set_dooroption(byte opt);

  // relayprogram sets the behavior of relays.
  // option 8 means relay will energize when 8 is low (on the Shield footprint)
  // option 20 means relay will energize when BelievedLocked==true (currentSensing signal)
  // option 35 means allow DoorMan to unlock the door (either by motion events, or if the door isn't fully closed)
  // option 36 means relay will energize when DoorMan thinks door is closed (as configured)
  // option 37 means relay will energize when DoorMan thinks door is locked (as configured)
  // option 38 means relay will energize whenever DoorMan receives a "motion detected" signal
  // option 112 means relay will energize when A12 is low.
  // 0 or 255: disable, relay does nothing.
  // IR Programming codes: 0010nppp where n is relay number, ppp is option.
  //  Example 00102008 makes relay 2 follow pin 8.
  static byte get_relayprogram(byte relaynumber1);
  static void set_relayprogram(byte relaynumber1, byte opt);

  // current_sensing_option enables the behavior of detecting locked and jammed
  // status based on current flow.
  // option 91 activates behavior for SDC 1091 bolt lock.
  // all other options, no current sensing.
  // IR Programming codes: 02877xxx where xxx=091 turns it on, anything else off.
  static byte eepromconfig::get_current_sensing_option();
  static void eepromconfig::set_current_sensing_option(byte opt);

  // 32355xxx - DOORBELL BEHAVIOR (32355 spells DBell)
  // 8 = Doorbell is rung by connecting pins A8+A9 (or A8+GND) and gets transmitted to the Paxton as a bell key keypad press.
  // 18 = Doorbell is rung by connecting pins A8+A9 (or A8+GND) and inhibits the "left open beep option"
  static byte eepromconfig::get_doorbell_option();
  static void eepromconfig::set_doorbell_option(byte opt);

  // current_sensor_zero_point is typically 512 (~midpoint of 0-1023), and saves
  // what value is expected from the current sensor when current is zero.
  static uint16_t eepromconfig::get_current_sensor_zero_point();
  static void eepromconfig::set_current_sensor_zero_point(uint16_t);

};

// This class stands for one of the screens that can be reached via the push button.
// Create the object and call addDisplayPage() to add it to the list.
// It can display up to one PROGMEM message, and then one message in RAM.
class displayPage {
  public:
    displayPage() {};
    displayPage(char *newmsg) {msg=malloc(strlen(newmsg)+1); strcpy(msg,newmsg);};
    displayPage(__FlashStringHelper *newmsg) : rommsg((const char PROGMEM*)newmsg) {};
    displayPage *onShortPress = NULL;
    displayPage *onLongPress = NULL;
    const char PROGMEM *rommsg = NULL;
    char *msg = NULL;
    displayPage* addDisplayPage(displayPage *msg);
    displayPage* addLongPressDisplayPage(displayPage *msg);


};

class translateWiegand {
  public:
    static void setup();
    static void loop();
};

class lcdMenus {
  public:
    static void setup();
    static void loop();
    // global function to prompt the display logic to update the screen ASAP
    static void updateScreen();

};

class relayPrograms {
  public:
    static void setup();
    static void loop();
    static void addRelayDetailPage(displayPage *dp);
};

class currentSensing {
  public:
    static void setup();
    static void loop();
    static bool feature_enabled;

};

class leftOpenBeep {
  public:
    static void timer0_compA_isr();
    static void setup();
    static void loop();
};

class doorbellButton {
  public:
    static void setup();
    static void loop();
};






displayPage* addDisplayPage(displayPage *msg);
extern displayPage* diagnosticsPage;


