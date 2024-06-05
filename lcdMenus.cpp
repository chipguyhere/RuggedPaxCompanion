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
#include "Adafruit_SSD1306.h"
#include "stdlib.h"
#include <Watchdog.h>
#include "irMega48.h"
#include <Adafruit_NeoPixel.h>

#include "Fonts/FreeSerifBold9pt7b.h"

static Adafruit_SSD1306 display(128, 32, &Wire, -1); // 128x32 OLED display

static irMega48 ir;

static displayPage *firstdisplayPage=NULL;
displayPage* diagnosticsPage;
static displayPage programmingMode;
static bool lcdInitSuccess=false;
static const char keymap[] PROGMEM = "0123456789*#";

static Adafruit_NeoPixel pixels(1, 46, NEO_GRB + NEO_KHZ800);


static void setRgbLedColor(byte r, byte g, byte b) {
  static bool inited;
  if (!inited) pixels.begin();
  inited=true;
  pixels.setPixelColor(0, pixels.Color(r,g,b));
  pixels.show();
}

static void lcdMenus::setup() {
  setRgbLedColor(0, 0, 128);
  ir.begin();

  pinMode(47, INPUT_PULLUP);

  // Initialize the OLED display
  lcdInitSuccess = display.begin(SSD1306_SWITCHCAPVCC, 0x3c); // 0x3c = 128x32 i2c screen address
  
  if (lcdInitSuccess) {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 10);
    display.setFont(&FreeSerifBold9pt7b);
    display.println(F("@chipguyhere"));
    display.display();      // Show initial text
    display.setFont();
  } else {
    Serial.println(F("SSD1306 i2c display initialization failed"));
  }

  // Add the screen for the programming mode
  programmingMode.rommsg = PSTR("Programming Mode\n\n\nUse infrared remote");
  addDisplayPage(&programmingMode);
  

}

displayPage* addDisplayPage(displayPage *msg) {
  if (firstdisplayPage==NULL) {
    firstdisplayPage=msg;
    return msg;    
  }
  displayPage *sm = firstdisplayPage;
  while (sm->onShortPress != NULL) sm = sm->onShortPress;
  sm->onShortPress = msg;    
  return msg;
}

displayPage* displayPage::addDisplayPage(displayPage *msg) {
  displayPage *sm = this;
  while (sm->onShortPress != NULL) sm = sm->onShortPress;
  sm->onShortPress = msg;    
  return msg;
}

displayPage* displayPage::addLongPressDisplayPage(displayPage *msg) {
  displayPage *sm = this;
  while (sm->onLongPress != NULL) sm = sm->onLongPress;
  sm->onLongPress = msg;    
  return msg;
}

extern Watchdog watchdog;

static char alreadyDisplayed[100];
static long lastPaintdisplayPage;

// global function to prompt the display logic to update the screen ASAP
static void lcdMenus::updateScreen() {
  // set alreadyDisplayed to some sort of invalid value
  // that will quickly miscompare and force a screen redraw
  alreadyDisplayed[0]=0xFF;
  alreadyDisplayed[1]=0;
  lastPaintdisplayPage = millis() - 2000;
}

static void lcdMenus::loop() {

  // LED splash, show fade from green to blue for first second after poweron
  static byte splashCompleted;
  if (splashCompleted==0) {
    uint32_t um = millis();
    if (um > 1000) {
      splashCompleted=1; 
      display.clearDisplay();
      display.setTextColor(SSD1306_WHITE);
      display.setCursor(20, 15);
      display.setFont(&FreeSerifBold9pt7b);
      display.println(F("ARDUINO"));
      display.display();      // Show initial text
      display.setFont();
    }
    else setRgbLedColor(0, (byte)(um>>4), 255-(byte)(um>>3));
  } else if (splashCompleted==1) {
    uint32_t um = millis();
    if (um > 2000) { splashCompleted=2; updateScreen(); }
    else setRgbLedColor(0, (byte)(um>>4), 255-(byte)(um>>3));
  }


  long m = millis();

  // Perform the blinking green light heartbeat once per second
  static long lastLightOn, lastLightOff;
  if (splashCompleted==2) {
    if (m - lastLightOn >= 1000) {
      setRgbLedColor(0, 24, 0);
      lastLightOn=lastLightOff=m;
    } else if (m - lastLightOff > 50) {
      setRgbLedColor(0,0,0);
      lastLightOff=m;
    }
  }

  // refresh displayed message
  static displayPage *sm;
  static bool displayTimeoutEnabled;
  static long lastKeyPress;
  if ((m - lastPaintdisplayPage > 1000) && lcdInitSuccess && splashCompleted==2) {
    char whatToDisplay[100];
    char *w = whatToDisplay;
    byte n = 0;

    if (sm != NULL && sm->rommsg != NULL) {
      const char PROGMEM *rommsg = sm->rommsg;
      while (pgm_read_byte(rommsg) && n<(sizeof(whatToDisplay)-1)) {
        *w++ = (char)pgm_read_byte(rommsg++);
        n++;
      }
    }
    if (sm != NULL && sm->msg != NULL) {
      char *s = sm->msg;
      while (*s && n<(sizeof(whatToDisplay)-1)) {
        *w++ = *s++;
        n++;
      }
    }
    *w=0;

    // Display has changed, so refresh it
    if (strcmp(alreadyDisplayed, whatToDisplay) != 0) {
      strcpy(alreadyDisplayed, whatToDisplay);
      display.clearDisplay();
      display.setTextColor(SSD1306_WHITE);
      display.setCursor(0,0);
      display.print(whatToDisplay);
      display.display();
    }
  }
  
  m=millis();

  // blank out the screen if no keypress in 5 minutes.
  if (displayTimeoutEnabled && (m-lastKeyPress > 300000)) sm=NULL,displayTimeoutEnabled=false;


  // poll the button, and go to the next displayPage if it's been pressed.
  static long lastButtonPoll;
  static bool lastButtonPressed;
  static bool letsreboot;
  static byte buttonHeldTicks;

  enum {none, shortPressed, longPressed} buttonEvent = none;

#define LONG_BUTTONPRESS_LENGTH_TENTHS 8

  bool buttonPressed = lastButtonPressed;
  if (m - lastButtonPoll > 100) {
    buttonPressed = (digitalRead(47)==LOW), lastButtonPoll=m;
    // some tenths of seconds of holding is a "long press"
    if (buttonPressed) if (++buttonHeldTicks==LONG_BUTTONPRESS_LENGTH_TENTHS) buttonEvent = longPressed;
  }
  // Elsewhere in the sketch, if the button is held 8+ sec, the watchdog timer feed is skipped,
  // causing a reboot.  // if button released after not having been held, that's a "short press"
  if (lastButtonPressed==true && buttonPressed==false && buttonHeldTicks < LONG_BUTTONPRESS_LENGTH_TENTHS) buttonEvent = shortPressed;
  if (buttonPressed==false) buttonHeldTicks=0;
  lastButtonPressed = buttonPressed;

  if (buttonPressed && letsreboot) {
    if (lcdInitSuccess) {
      display.clearDisplay();
      display.setCursor(0,0);
      display.print("Rebooting...");
      display.display();
    }
    watchdog.enable(Watchdog::TIMEOUT_1S);
    while (true) {}
  }    

  if (buttonEvent != none) {
    ir.read(); // flush buffer
    displayTimeoutEnabled=true;
    lastKeyPress=m;
    if (sm==NULL) sm=firstdisplayPage;
    else sm = (buttonEvent==shortPressed) ? sm->onShortPress : sm->onLongPress;
  }


  static char irrxtxt[40];
  static uint16_t addresscode;
  static byte totalrx0;
  static byte totalrxn;
  static byte commandcodes[12];
  if (sm == &programmingMode) {
    uint32_t irrx = ir.read();
    // We can either use the cheap Amazon $1 Arduino remote (ten key with blue arrows and red */#/OK)
    // SET UP THE IR RECEIVER TO LEARN A NEW REMOTE ON DEMAND.
    // Simply press 0000000000123456789*# on any NEC-compatible remote, and the keys are displayed as learned.
    // # will be enter and * will be erase/startover.
    // If you mistake, just start over with 0's again.
    if (irrx == 0); // Nothing was pressed.
    if (irrx == 1); // A previous key is being held down, we'll ignore.
    if (irrx > 1) { // A new key was pressed.
      programmingMode.rommsg = PSTR("Programming Mode\n"); // Removes the "Use Infrared Remote" message
      byte c = 0xFF;
      uint16_t irrxaddr = irrx>>16;
      byte irrxcmd = irrx;

      // if using the cheap $1 arduino remote and no other remote has begun learning, decode the key from this table
      if (totalrxn==0 && irrxaddr == 0xFF00) {
        static const byte PROGMEM xlate[] = {0x19,0x45,0x46,0x47,0x44,0x40,0x43,0x07,0x15,0x09,0x16,0x0D,0x08,0x1C,0x18,0x5A,0x52};
        for (byte i=0; i<17; i++) if (irrxcmd == pgm_read_byte(&xlate[i])) c=i;
        if (c==12 || c==13) c-=2; // in case user pressed left arrow or OK button - good as secondary clear/enter keys.
      }
      // if we have a full set of keys learned, decode the key
      if (totalrxn==12 && addresscode==irrxaddr) {
        for (byte i=0; i<12; i++) if (irrxcmd==commandcodes[i]) c=i;
      }

      // was a key pressed?
      if (c>=0 && c<=9) {
        // user pressed a number.
        int sl = strlen(irrxtxt);
        if (sl<8) {
          irrxtxt[sl++]='0'+c;
          irrxtxt[sl]=0;
        }
      } else if (c==10) {
        // user pressed * or backspace, erase their input
        irrxtxt[0]=0;
      } else if (c==11) {
        // user pressed # or OK or ENTER
        if (strlen(irrxtxt)==8) {
          // evaluate their input.
          uint32_t theircommand = strtoul(irrxtxt, NULL, 10);
          uint32_t ls = theircommand / 1000;
          uint16_t rs = theircommand % 1000;

          // IR PROGRAMMING OPTIONS
          // 00101xxx thru 00104xxx - RELAY BEHAVIOR
          if (ls >= 101 && ls <= 104) {
            eepromconfig::set_relayprogram(ls-100, rs);
            strcpy_P(irrxtxt,PSTR("relay program set."));
            letsreboot=true;
          }

          // 36677 (DOORS) - DOOR OPTION
          if (ls == 36677) {
            eepromconfig::set_dooroption(rs);
            strcpy_P(irrxtxt,PSTR("Door option set."));
            letsreboot=true;
          }

          // 53386 (LEFTO) - LEFTOPEN OPTION
          if (ls == 53386) {
            eepromconfig::set_leftopenbeepoption(rs);
            strcpy_P(irrxtxt,PSTR("LeftOpen option set."));
            letsreboot=true;
          }

          // 02877xxx - CURRENT SENSING BEHAVIOR, 091 on
          if (ls == 2877) {
            eepromconfig::set_current_sensing_option(rs);
            strcpy_P(irrxtxt,PSTR("current sensing set."));
            letsreboot=true;              
          }

          // 32355xxx - DOORBELL BEHAVIOR
          if (ls==32355) {
            eepromconfig::set_doorbell_option(rs);
            strcpy_P(irrxtxt,PSTR("doorbell option set."));
            letsreboot=true; 
          }

          // 87267xxx - TRANSLATION BEHAVIOR
          if (ls==87267) {
            eepromconfig::set_translationoption(rs);
            strcpy_P(irrxtxt, PSTR("translation option set."));
            letsreboot=true;
          }
        }
      } else if (totalrx0==0 && totalrxn==0) {
        // got a first keypress, assume it's a 0.
        addresscode = irrx>>16;
        commandcodes[0]=irrx;
        totalrx0++;
      } else if (totalrxn>0 && totalrxn<12) {
        if (addresscode!=irrxaddr) totalrx0=totalrxn=0;
        else {
          // learning keys.
          for (int i=0; i<totalrxn; i++) {
            // Got a subsequent keypress, make sure it's a key
            // we don't already have, if we do, start over.
            if (irrxcmd == commandcodes[i]) {
              totalrx0=totalrxn=0;
              break;
            }
          }
          if (totalrxn) commandcodes[totalrxn++]=irrx;
          // if we just got the 12th code, learning is complete.
          // clear the buffer so user can type.
          if (totalrxn==12) irrxtxt[0]=0;
        } 
      } else if (totalrx0>0 && totalrxn==0) {
        // got second thru tenth 0's, confirm they match.
        if (addresscode==irrxaddr && commandcodes[0]==irrxcmd) {
          // yep, and if it's the 10th, now expect a 1.
          totalrx0++;
          if (totalrx0==10) totalrxn++;
        } else {
          // nope, start over.
          totalrx0=totalrxn=0;
        }
      }

      // if we're learning the IR remote, print the characters we have learned so far.
      if (totalrxn>0 && totalrxn<12) {
        for (int i=0; i<totalrxn; i++) irrxtxt[i]=pgm_read_byte(keymap+i);
        irrxtxt[totalrxn]=0;
        strcat_P(irrxtxt, PSTR(" Press "));
        char z[2] = {pgm_read_byte(keymap+totalrxn),0};
        strcat(irrxtxt, z);
      }
      if (totalrxn==0 && totalrx0>7) {
        strcpy_P(irrxtxt, PSTR("Press 0"));
        for (int i=0; i<totalrx0; i++) irrxtxt[i+7]='.';
        irrxtxt[totalrx0+7]=0;
      }
      programmingMode.msg=irrxtxt;
    }
  }







}
