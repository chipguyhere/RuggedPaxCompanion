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



static const byte Wiegand0InputPin = 18; // Used for zeroes
static const byte Wiegand1InputPin = 19; // Used for ones
static byte Wiegand0OutputPin = 14;
#define PaxtonDataOutputPin Wiegand0OutputPin
static byte Wiegand1OutputPin = 15;
#define PaxtonClockOutputPin Wiegand1OutputPin
static int LEDInputPin = 50;
static int LEDOutputPin = -1;
static volatile bool bitArray[70]; // Array to store incoming Wiegand bits
static volatile int bitIndex = 0; // Index to keep track of the number of received bits
static unsigned long lastPulseTime = 0; // Time of the last pulse
static const long WiegandTimeout = 20; // 0.02 seconds timeout

static byte translationOption=0;
static bool usingPaxtonReaderProtocol=false;

static displayPage* wiegandDiagnosticsPage;

static void paxtonReaderOut(byte pindata, byte pinclock, uint32_t cardnumber);
static void paxtonKeypressOut(byte pindata, byte pinclock, char key);

// Allows other module (like leftOpenBeep) to appropriate the * or escape keypress
// for another purpose.  Returning true means the keypress was handled and can be discarded
bool (*star_key_handler)(void) = NULL;

// Interrupt handler for receiving a pulse on the zero line.
// Simply store the zero ("false") and increase the counter of bits received (bitIndex)
static void zeroPulse() {
  if (bitIndex < 70) {
    bitArray[bitIndex++] = false;
    lastPulseTime = millis();
  }
}

// Interrupt handler for receiving a pulse on the one line.
// Simply store the one ("true") and increase the counter of bits received (bitIndex)
static void onePulse() {
  if (bitIndex < 70) {
    bitArray[bitIndex++] = true;
    lastPulseTime = millis();
  }
}

static bool using_paxton_protocol_to_net2_board = false;



static void translateWiegand::setup() {

  translationOption = eepromconfig::get_translationoption();
  char smsg[200];

  // avoid allocating the display pages if the option isn't active.
  switch (translationOption) {
    case 14: case 114: case 160: break;
    default: return;
  }

  displayPage *dp1 = new displayPage(F("Card Reader / Keypad\ntranslation active.\n\nHold for details"));
  displayPage *dp2 = dp1->addLongPressDisplayPage(new displayPage(F("Converts Wiegand RFID\ninto Paxton Reader\nformat (card/keypad).\nMore info tap button.")));

  switch (translationOption) {
// Translation option 14: Wiegand to Wiegand32 out GPIO14/15
// Translation option 114: Wiegand to Paxton out GPIO14/15, LED in GPIO50 and ~out GPIO12
// Translation option 160: Wiegand to Paxton out A0/A1, LED in A2 and ~out GPIO12
  case 14:
    sprintf_P(smsg, PSTR("Wiegand program 14:\n to Wiegand32,\ninputs GPIO %d,%d\noutputs GPIO %d,%d"), 
      Wiegand0InputPin,Wiegand1InputPin,Wiegand0OutputPin,Wiegand1OutputPin);
    dp2->addDisplayPage(new displayPage(smsg));

    break;
  case 114:
    dp2->addDisplayPage(new displayPage(F("Wiegand program 114:\n Wiegand card reader:\nConnect D0/D1/LED\n to GPIO 18/19/12")));
    dp2->addDisplayPage(new displayPage(F("Wiegand program 114:\n to Paxton protocol:\n Net2 Data/Clk/RedLED\n connects to 14/15/50")));
    using_paxton_protocol_to_net2_board = true;
    LEDOutputPin = 12;
    pinMode(LEDInputPin, INPUT_PULLUP);
    usingPaxtonReaderProtocol=true;
    break;
  case 160:
    dp2->addDisplayPage(new displayPage(F("Wiegand program 160:\n Wiegand card reader:\nConnect D0/D1/LED\n to GPIO 18/19/12")));
    dp2->addDisplayPage(new displayPage(F("Wiegand program 160\n to Paxton Reader:\n Net2 Data/Clk/RedLED\n connects to A0/A1/A2")));
    using_paxton_protocol_to_net2_board = true;
    LEDInputPin = A2;
    LEDOutputPin = 12;
    pinMode(LEDInputPin, INPUT_PULLUP);
    PaxtonDataOutputPin = A0;
    PaxtonClockOutputPin = A1;
    usingPaxtonReaderProtocol=true;
    break;
  default:
    return;
  }
  addDisplayPage(dp1);

  wiegandDiagnosticsPage = new displayPage(F("Card Reader Test\n\nPress a key or\nswipe a card to test"));
  diagnosticsPage->addDisplayPage(wiegandDiagnosticsPage);

  
  pinMode(Wiegand0InputPin, INPUT_PULLUP);
  pinMode(Wiegand1InputPin, INPUT_PULLUP);
  // Leaving the outputs in "input pullup" mode while idle, to minimize potential
  // for damage in case of short circuits or miswiring.  Input_pullup looks
  // like "HIGH" to the Paxton, which it will treat as idle.
  pinMode(Wiegand0OutputPin, INPUT_PULLUP);
  pinMode(Wiegand1OutputPin, INPUT_PULLUP);
  // Reminder that PaxtonDataOutputPin and PaxtonClockOutputPin are the same as Wiegand0/1 pins.

  // Attach interrupt handlers to pins so we are notified when they receive a "falling" pulse
  attachInterrupt(digitalPinToInterrupt(Wiegand0InputPin), zeroPulse, FALLING);
  attachInterrupt(digitalPinToInterrupt(Wiegand1InputPin), onePulse, FALLING);

}



static void translateWiegand::loop() {


  //
  // Show diagnostic data on Card Reader diagnostic screen
  //
  static byte lastMessageSize;
  static long lastMessageWhen;
  static bool showingLastMessage;
  static __FlashStringHelper *lastMessageKind = F("Last event");
  static int lastSecondCount;
  if (showingLastMessage) {
    long m = millis();
    if (m - lastMessageWhen > 300000) showingLastMessage=false;
    else {
      static char* diagmsg;
      if (diagmsg==NULL) diagmsg=malloc(60);
      int secondCount = (m - lastMessageWhen) / 1000L;
      if (lastSecondCount != secondCount) {
        lastSecondCount = secondCount;
        strcpy_P(diagmsg, (const char*)lastMessageKind);
        sprintf_P(&diagmsg[strlen(diagmsg)], PSTR(" received:\n%d bits %d sec ago"),lastMessageSize, lastSecondCount);
        wiegandDiagnosticsPage->msg=diagmsg;
        wiegandDiagnosticsPage->rommsg=PSTR("Card Reader Test\n\n");
      }
    }
  } 

  if (LEDOutputPin != -1) {
    if (digitalRead(LEDInputPin)==LOW) {
      pinMode(LEDOutputPin, INPUT_PULLUP);
    } else {
      pinMode(LEDOutputPin, OUTPUT);
      digitalWrite(LEDOutputPin, LOW);
    }
//    digitalWrite(ledOutputPin, digitalRead(ledInputPin)==LOW ? HIGH : LOW);
  }

  // Look for new Wiegand messages
  if (bitIndex > 0 && (millis() - lastPulseTime) > WiegandTimeout) {
    uint64_t message = 0;
    if (bitIndex >= 4) {
      // Process an incoming Wiegand message
      Serial.print("Got a ");
      Serial.print(bitIndex);
      Serial.print(" bit Wiegand message: ");
      for (int i=0; i<bitIndex; i++) {
        Serial.print(bitArray[i] ? 1 : 0);
        message <<= 1;
        if (bitArray[i]) message++;      
      }
      Serial.println();
    }

    if (bitIndex >= 4) {
      lastMessageSize = bitIndex;
      lastMessageWhen = millis();
      showingLastMessage = true;
      lastSecondCount=-1;
      if (bitIndex==4) {
        switch (message) {
        case 10: lastMessageKind = F("*/ESC key"); break;
        case 11: lastMessageKind = F("#/Enter key"); break;
        case 12: lastMessageKind = F("Bell key"); break;
        case 13: case 14: case 15: lastMessageKind = F("Other key"); break;
        default: lastMessageKind = F("Digit key"); break;
        }
      } else {
        lastMessageKind = F("Card swipe");
      }
    }

    if (bitIndex==26) { // save the bottom 24 bits... highest number 16777216, always below 8 digits
      message &= 0x1FFFFFE;
      message >>= 1;
    } else if (bitIndex == 35) { // HID 35 bit, Cherry-pick the 20 bits we want, discard the rest.
      message &= 0x1FFFFE;
      message >>= 1;
    } else if (bitIndex==34) {
      // Wiegand34, we'll take the inner 32 thanks
      message &= 0x1FFFFFFFEUL;
      message >>= 1;
    } else if (bitIndex==36) {
      // Wiegand34 with ibutton detection, discard the first two bits (plus first and last parity bits)
      message &= 0x1FFFFFFFEUL;
      message >>= 1;  
    } else if (bitIndex != 4 && bitIndex < 26) {
      // unwanted message, probably noise.
      bitIndex=0;
      return;
    }



    message = message % 100000000; // Keep only the smallest 8 decimal digits.
    uint32_t message32 = message;
    Serial.print(F("The card number is "));
    Serial.println(message32);

    if (usingPaxtonReaderProtocol) {
      if (bitIndex==4 && message32 < 13) {
        bool handled=false;
        if (message32==10 && star_key_handler != NULL) handled = (*star_key_handler)();
        if (!handled) paxtonKeypressOut(PaxtonDataOutputPin, PaxtonClockOutputPin, message32);
      } else if (bitIndex >= 26 && message32 != 0) {
        paxtonReaderOut(PaxtonDataOutputPin, PaxtonClockOutputPin, message32);
      }
      bitIndex=0;
      message32=0;
      return;
    }

    if (message32 != 0) { // do not allow a message of cardnumber 0, Paxton doesn't like this

      // Send the modified Wiegand message out the Wiegand output pins.
      // The out message will always be exactly 32 bits.
      pinMode(Wiegand0OutputPin, OUTPUT);
      pinMode(Wiegand1OutputPin, OUTPUT);
      for (int i=0; i<32; i++) {
          if (message32 & 0x80000000) {
            digitalWrite(Wiegand1OutputPin, LOW);
            delayMicroseconds(40);
            digitalWrite(Wiegand1OutputPin, HIGH);
            delayMicroseconds(200);
          } else {
            digitalWrite(Wiegand0OutputPin, LOW);
            delayMicroseconds(40);
            digitalWrite(Wiegand0OutputPin, HIGH);
            delayMicroseconds(200);
          }
          message32 <<= 1;
      }
      pinMode(Wiegand0OutputPin, INPUT_PULLUP);
      pinMode(Wiegand1OutputPin, INPUT_PULLUP);
      bitIndex=0;
    }
    bitIndex=0;
  }
}


static void paxtonProtocolSend(byte pindata, byte pinclock, byte messageLength, byte *message) {
    pinMode(PaxtonDataOutputPin, OUTPUT);
    pinMode(PaxtonClockOutputPin, OUTPUT);


  // calculate parity word, which will be sent after the message
  byte messageparity = 0;
  for (byte i=0; i<messageLength; i++) messageparity ^= message[i];

  // Write preamble, which is ten clocks with data high
  digitalWrite(pindata, HIGH);
  for (byte i=0; i<10; i++) {
    delayMicroseconds(200);
    digitalWrite(pinclock, LOW);
    delayMicroseconds(200);
    digitalWrite(pinclock, HIGH);
    delayMicroseconds(200);
  }

  // Send the message (including a parity bit per word, and plus a parity word for the whole message)
  for (byte i=0; i<=messageLength; i++) {
    byte wordparity=1;
    byte mr = (i==messageLength) ? messageparity : message[i];
    for (byte j=0; j<5; j++) {
      wordparity ^= (mr & 1);
      if (j==4) mr=wordparity;
      digitalWrite(pindata, (mr & 1) ? LOW : HIGH);
      delayMicroseconds(200);
      digitalWrite(pinclock, LOW);
      delayMicroseconds(200);
      digitalWrite(pinclock, HIGH);
      delayMicroseconds(200);
      mr>>=1;
    }
  }

  // Write epilogue, which is another ten clocks with data high
  digitalWrite(pindata, HIGH);
  for (byte i=0; i<10; i++) {
    delayMicroseconds(200);
    digitalWrite(pinclock, LOW);
    delayMicroseconds(200);
    digitalWrite(pinclock, HIGH);
    delayMicroseconds(200);
  }

  pinMode(PaxtonDataOutputPin, INPUT_PULLUP);
  pinMode(PaxtonClockOutputPin, INPUT_PULLUP);


}

// Outputs a card swipe message using the proprietary Paxton clock/data
// card reader protocol (determined by scoping their reader).
// Pin 0 is Data, Pin 1 is Clock (consistent with labeling on ACU)
static void paxtonReaderOut(byte pindata, byte pinclock, uint32_t cardnumber) {

  // Do not allow a card number of 0, Paxton doesn't like this
  if (cardnumber==0) return;

  // Card number swipe is an 11-word message, where each word is 4 bits

  byte message[10];
  // positions 0 and 9 get constants to begin and end the message
  message[0] = 0x0b;
  message[9] = 0x0f;
  byte p=8; // put card number into message positions 12345678
  for (byte i=0; i<8; i++) {
    message[p--] = cardnumber % 10;
    cardnumber = cardnumber / 10;
  }
  paxtonProtocolSend(pindata,pinclock,10,message);
}

void paxtonSendBell() {
  if (!using_paxton_protocol_to_net2_board) return;
  paxtonKeypressOut(PaxtonDataOutputPin, PaxtonClockOutputPin, 'B');
}

// Send a keypress.  Valid keys are 0123456789*#B where B is the bell key.
static void paxtonKeypressOut(byte pindata, byte pinclock, char key) {
  byte message[6];
  message[0]=0x0b;
  message[1]=0x0c;
  message[2]=0x00;
  message[4]=0x0e;
  message[5]=0x0f;

  if (key=='0' || key==0) message[3]=0x09;
  else if (key >= '1' && key <= '9') message[3] = key-'1';
  else if (key >= 1 && key <= 9) message[3] = key-1;
  else if (key=='*' || key==10) message[2]=0x01,message[3]=0x00;
  else if (key=='#' || key==11) message[2]=0x01,message[3]=0x01;
  else if (key=='B' || key==12) message[2]=0x01,message[3]=0x05;
  else return;

  paxtonProtocolSend(pindata,pinclock,6,message);

}
