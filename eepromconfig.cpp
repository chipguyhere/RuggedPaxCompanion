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

/*
 * EEPROM Memory Map
 * 
 * 
 * 8 = Translation Option (Wiegand/Paxton)
 * 9 = Door left open beep option
 * 10 = Door option
 * 11-12 = Current sensor zero point
 * 13 = Relay 1 program
 * 14 = Relay 2 program
 * 15 = Relay 3 program
 * 16 = Relay 4 program
 * 20 = Current sensing option
 * 21 = Doorbell option

 */



static byte eepromconfig::get_translationoption() { return EEPROM.read(8); }

static void eepromconfig::set_translationoption(byte opt) { EEPROM.update(8, opt); }


static byte eepromconfig::get_leftopenbeepoption() {
  byte rv = EEPROM.read(9); // Door Left Open Beep Option
  if (rv==30) return rv;
  return 0;
}

static void eepromconfig::set_leftopenbeepoption(byte opt) { EEPROM.update(9, opt); }



static byte eepromconfig::get_dooroption() {
  byte rv = EEPROM.read(10) ^ 0x55;
  if (rv >= 10 && rv <= 15) return rv;
  return 0xFF;
}

static void eepromconfig::set_dooroption(byte opt) { EEPROM.update(10, opt ^ 0x55); }


static byte eepromconfig::get_relayprogram(byte relaynumber1) { return EEPROM.read(relaynumber1+13-1); }

static void eepromconfig::set_relayprogram(byte relaynumber1, byte opt) {
  if (relaynumber1 >= 1 && relaynumber1 <= 4) {
    EEPROM.update(relaynumber1+13-1, opt);
  }
}


static byte eepromconfig::get_current_sensing_option() { return EEPROM.read(20); }
static void eepromconfig::set_current_sensing_option(byte opt) { EEPROM.update(20, opt); }
static byte eepromconfig::get_doorbell_option() { return (byte)(EEPROM.read(21)); }
static void eepromconfig::set_doorbell_option(byte opt) { EEPROM.update(21, opt); }



static uint16_t eepromconfig::get_current_sensor_zero_point() {
  uint16_t rv = EEPROM.read(11) * 256 + EEPROM.read(12);
  if (rv < 480 || rv > 560) rv = 512;
  return rv;
}

static void eepromconfig::set_current_sensor_zero_point(uint16_t zp) {
  if (zp >= 480 && zp <= 560) {
    EEPROM.update(11, zp >> 8);
    EEPROM.update(12, zp);
 }
}