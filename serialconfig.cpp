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
#include "EEPROM.h"




static serialconfig::setup() {


}


char cmdbuffer[32];
byte cmdbufferlength=0;


static serialconfig::loop() {

  int c = Serial.read();
  if (c < 0) return;

  if (c != 13 && c != 10) {
    if (c >= 'a' && c <= 'z') c-=0x20;
    cmdbuffer[cmdbufferlength++] = c;
    if (cmdbufferlength >= 32) cmdbufferlength = 31;
    cmdbuffer[cmdbufferlength] = 0;
    return;
  }

  if (cmdbufferlength==0) return;
  cmdbufferlength=0;

  if (!strcmp_P(cmdbuffer, PSTR("?"))) {
    Serial.println(F("Command Help:"));
    Serial.println(F("ERASE = Erase all EEPROM"));
    Serial.println(F("SHOW = Show config"));
    Serial.println(F("D0 = Door Option = Single Door, Closed Contacts Closed Door"));
    Serial.println(F("D1 = Door Option = Single Door, Open Contacts Closed Door"));
    Serial.println(F("D2 = Door Option = Double Door, Closed Contacts Closed Door"));
    Serial.println(F("D3 = Door Option = Double Door, Open Contacts Closed Door"));
    Serial.println(F("D4 = Door Option = Single Door, Closed Contacts Lockconfirmed Door"));
    Serial.println(F("D5 = Door Option = Single Door, Open Contacts Lockconfirmed Door")); 
    return;
  }


  if (!strcmp_P(cmdbuffer,PSTR("ERASE"))) {
      Serial.print(F("Erasing "));
      Serial.print(EEPROM.length());
      Serial.println(F(" bytes..."));
      for (int i=0; i<EEPROM.length(); i++) EEPROM.write(i, 0xFF);
      Serial.println("Done!");
      return;
  }


  if (!strcmp_P(cmdbuffer,PSTR("SHOW"))) {
    Serial.print(eepromconfig::get_dooroption());
    Serial.println(F(" <-- Door option"));
    Serial.print(eepromconfig::get_current_sensor_zero_point());
    Serial.println(F(" <-- Current sensor zero point"));
    return;
  }

  if (strlen(cmdbuffer)==2 && cmdbuffer[0]=='D') {
    if (cmdbuffer[1] >= '0' && cmdbuffer[1] <= '7') {
      eepromconfig::set_dooroption(cmdbuffer[1]-'0');
      Serial.println(F("Updated."));
    }
    return;
  }

  if (strlen(cmdbuffer)==5 && cmdbuffer[0]=='Z' && cmdbuffer[1]=='P') {
    int nzp = cmdbuffer[2] - '0';
    nzp *= 10;
    nzp += cmdbuffer[3] - '0';
    nzp *= 10;
    nzp += cmdbuffer[4];
    eepromconfig::set_current_sensor_zero_point(nzp);
    Serial.print(F("Zero point is set to "));
    Serial.println(eepromconfig::get_current_sensor_zero_point());
  }

  if (!strcmp_P(cmdbuffer,PSTR("ZP"))) {
    Serial.print(F("Sampling... (confirm load is off)"));
    uint16_t samplecount[1024];
    uint32_t mean=0;
    for (int i=0; i<1024; i++) samplecount[i]=0;
    for (int i=0; i<1000; i++) {
      int scv = analogRead(CURRENT_SENSE_INPUT);
      mean += scv;
      samplecount[scv]++;
      delay(1);
    }
    Serial.println();
    for (int i=256; i<768; i+=16) {
      for (int j=0; j<16; j++) {
        Serial.print(i+j);
        Serial.print(' ');
      }
      Serial.println();
      for (int j=0; j<16; j++) {
        uint16_t sc = samplecount[i+j];
        if (sc < 1000) Serial.print(' ');
        if (sc < 100) Serial.print(' ');
        if (sc < 10) Serial.print(' ');
        Serial.print(sc);
      }
      Serial.println();
    }
  }




  


}
