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





static int currentReadingCount=0;
static byte currentReadings[1024];
static int histogram[256];
static int comsam = 0;
static uint16_t current_sensor_zero_point = 512;


constexpr int lockedinfo_size=16;
static int lockedinfo[lockedinfo_size];
static int lockedsamples=0;

bool currentSensing::feature_enabled=false;

displayPage lockDisplayPage;
char lockStatus[30]="";


static void currentSensing::setup() {
  if (eepromconfig::get_current_sensing_option() != 91) {
    // CURRENT SENSING IS NOT ENABLED so don't initialize.
    return;
  }

  currentSensing::feature_enabled=true;

  lockDisplayPage.rommsg = PSTR("SDC 1091 jam detect:\n");
  addDisplayPage(&lockDisplayPage);
  lockDisplayPage.msg = lockStatus;

  current_sensor_zero_point = eepromconfig::get_current_sensor_zero_point();

}


static void currentSensing::loop() {
  if (!currentSensing::feature_enabled) return;


  long m = millis();
  static long lastCurrentReading;

  // Read the Current (Amps) from the current sensor, and put it in the currentReadings array.
  if (m - lastCurrentReading >= 1) {
    lastCurrentReading = m;
    long currentReading = analogRead(CURRENT_SENSE_INPUT);
    // APPLY ANY ADJUSTMENT ALGORITHM HERE
    currentReading -= current_sensor_zero_point;
    /*
    float fcr = currentReading;
    if (fcr < 0) fcr=-fcr;
    int z = (currentReading < 0) ? -currentReading : currentReading;
    if (fcr > 22) fcr = pow(fcr, 1.3);
    currentReading = fcr;
    if (z < 20) {
      Serial.print('.');
    } else if (z >= 15 && z < (15+36)) {
      Serial.print("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"[z-15]);
    } else {
      Serial.print('#');
    }
    //long z = currentReading * currentReading;
    //int z = fcr;
    //for (int zz=0; zz<sizeof(syms); zz++) {
    //  if (z < thresholds[zz] || thresholds[zz]==0) {
    //    Serial.print(syms[zz]);
    //    break;
    //  }
    //}
    if ((currentReadingCount % 120) == 0) Serial.println();
*/
    if (currentReading < 0) currentReading = -currentReading;
    if (currentReading > 255) currentReading=255;

    int idx = currentReadingCount % sizeof(currentReadings);
    if (currentReadingCount >= sizeof(currentReadings)) histogram[currentReadings[idx]]--;
    histogram[currentReading]++;
    currentReadings[idx] = currentReading;
    currentReadingCount++;
    if (currentReadingCount >= sizeof(currentReadings)*2) currentReadingCount = sizeof(currentReadings);
  }

  static int responsesshown=0;
  static long lastHistoAnalysis;
  if ((m - lastHistoAnalysis) > 500) {
    lastHistoAnalysis = m;

    long tt=0;
    for (int i=30; i<256; i++) tt += histogram[i];
 
    int peakI=0;
    int peakIval=1;
    int sampleTotal=0;
    for (int i=10; i<30; i++) {
      int vv = histogram[i];
      sampleTotal += vv;
      vv += histogram[i+1];
      vv += histogram[i-1];

      if (vv > peakIval) peakIval=vv,peakI=i;
    }
    for (int i=30; i<255; i++) sampleTotal += histogram[i];

    // if there's significant current flowing at least (200/1024) or 20% of the time, consider the door locked.
    if (sampleTotal > 200) believedLocked=true; else believedLocked=false;
    if (believedLockedValid==false && m > 3000) believedLockedValid=true;

    // Compare the sample count at three quarters of the peak to the sample count at the top of the peak.
    // If the lock is jammed, we'll get to the peak quicker, and there will be fewer samples at the 3/4 mark.
    int fractionofI = peakI * 3 / 4;
    int comparativeSample = histogram[fractionofI];
    comparativeSample += histogram[fractionofI+1];
    comsam = comparativeSample * 100 / peakIval;

    if (believedLocked==false) {
      lockedsamples=0;
    } else {
      lockedinfo[lockedsamples % lockedinfo_size]=comsam;
      lockedsamples++;
      if (lockedsamples>=lockedinfo_size*2) lockedsamples=lockedinfo_size;
    }

    if (lockedsamples < lockedinfo_size) {
      believedJammed=false;
    } else {
      long avgI=0;
      for (int i=0; i<lockedinfo_size; i++) avgI += lockedinfo[i];
      avgI /= lockedinfo_size;
      believedJammed = (avgI < 25);
    }

    if (believedLocked) {
      Serial.print(F("Locked n="));
      Serial.print(comsam);
      Serial.println(F("%"));
    }
  }

  m = millis();
  static long millisSinceLastDisplay;
  if (m - millisSinceLastDisplay > 500) {
    millisSinceLastDisplay = m;
    if (believedLocked) {
      sprintf_P(lockStatus, PSTR("Locked n=%d%% %sjam"), comsam, believedJammed ? "" : "no");
    } else {
      strcpy_P(lockStatus, PSTR("Lock not engaged"));
    }
    lcdMenus::updateScreen();
  }
}