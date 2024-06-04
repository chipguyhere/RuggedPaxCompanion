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
#include "irMega48.h"


// A specialized fork of chipguyhere/PulseCapture that has been
// reduced to the single purpose of taking over Timer5 and using it to capture NEC
// infrared remote signals on pin 48, its native input capture pin.


#define TIMERx 5
#define ICRx ICR5
#define TCNTx TCNT5
#define TIFRx TIFR5
#define TCCRxA TCCR5A
#define TCCRxB TCCR5B
#define TIMSKx TIMSK5
#define HWPINx 48
#define TIMERx_CAPT_vect TIMER5_CAPT_vect
#define TIMERx_OVF_vect TIMER5_OVF_vect
void ISRX(uint8_t PINx, char _portid, uint16_t cntx, uint16_t cnty);


void cgh_rugpaxpulsecap_add_tick_to_queue(uint32_t timestamp);
void cgh_rugpaxpulsecap_add_event_to_queue(uint8_t portRead, char portid, uint32_t timestamp);


volatile uint16_t ovfcount=0;
volatile byte mainTimer=TIMERx;


ISR(TIMER5_COMPB_vect) {
	uint16_t ovf = ovfcount;
	if (mainTimer==TIMERx) { 
		if (TIFRx & _BV(TOV5)) ovf++;		
		cgh_rugpaxpulsecap_add_tick_to_queue(ovf*65536 + TCNTx);  		
	}
}

class rugpaxpulsecap {
public:

  rugpaxpulsecap();
  rugpaxpulsecap(byte pin, byte _protocol);
  
  // return values of begin():
  //  3 : active using hardware timer capture 
  //  2 : active using pin change interrupts
  //  1 : active using classic Arduino attachInterrupt/millis/micros
  // -1 : pin not supported, nothing active
  int begin(void);
  void init(byte _pin, byte _protocol);

  // Reads the last message if any, optionally returning the count of bits in the message.
  uint32_t read();
  uint32_t read(int &bitcount);
  
  uint8_t capturedBitCount=0;
  uint32_t capturedMessage=0;

  void* _handle_irq(void *eev); 
  virtual void _handle_edge(char edgeKind, uint32_t timediff32, uint16_t timediff);    
  uint8_t pin=0;
  uint8_t inword=0;
  uint8_t protocol=0;      // 9=serial cardreader I=IR W=Wiegand1
  char portid=0;
  uint8_t mask=0;
  uint8_t lastRead=0;
  uint32_t lastTimestamp=0;
  uint8_t bitsreceived=0;
  uint32_t capbuf=0;
  uint8_t clockrate=0;  
  rugpaxpulsecap *next_rugpaxpulsecap_instance=NULL;

};


class rugpax_irReceiver : public rugpaxpulsecap {
public:
	rugpax_irReceiver(byte _pin);

	void _handle_edge(char edgeKind, uint32_t timediff32, uint16_t timediff);	
private:

	uint32_t lastMessageTimestamp=0;
	uint32_t sumdiffs=0;

};


// Represents the facts of an interrupt we received.
// Goes in a circular buffer for handling with interrupts re-enabled.
struct edgeevent {
  char portid;      
  uint8_t portRead;     // inputs of the port register when we read it (or 0/1 for ICP fall/rise)
  uint32_t timer;
}; 

bool rugpaxpulsecap_began=false;
bool main_timer_is_micros=false;
volatile rugpaxpulsecap *first_rugpaxpulsecap_instance=NULL;

// edgeEventCount must be a power of two.
// suggestions: 8 = normal usage
// 128 = debugging this code with lots of serial print
#define edgeEventCount 32

volatile struct edgeevent edgeEvents[edgeEventCount];
volatile uint8_t edgeEventHead=0;
volatile uint8_t edgeEventTail=0;

void handle_irq_queue();


void debugEdgeEvents() {
	return;

	static uint8_t debugTail = 0;
	
	while (debugTail != edgeEventHead) {
		edgeevent *ee = &edgeEvents[debugTail];
		debugTail++;
		if (debugTail==edgeEventCount) debugTail=0;	
	}


}


rugpaxpulsecap::rugpaxpulsecap() {}

rugpaxpulsecap::rugpaxpulsecap(byte pin, byte _protocol) {
  init(pin,_protocol);
}

void rugpaxpulsecap::init(byte _pin, byte _protocol) {

	// init gets called by the constructor
	// which could happen before setup().
	// interrupts should not be activated until calling begin

	pin=_pin;
	protocol=_protocol;
	
	noInterrupts();
	if (first_rugpaxpulsecap_instance==NULL) {
		first_rugpaxpulsecap_instance = this;
	} else {
		rugpaxpulsecap *pc = first_rugpaxpulsecap_instance;
		while (pc->next_rugpaxpulsecap_instance) pc=pc->next_rugpaxpulsecap_instance;
		pc->next_rugpaxpulsecap_instance=this;
	}
	interrupts();
}




uint32_t rugpaxpulsecap::read(int &bitcount) {
  bitcount = capturedBitCount;
  if (capturedBitCount==0) return 0;
  uint32_t rv = capturedMessage;
  capturedBitCount=0;
  capturedMessage=0;  
  return rv;
}
uint32_t rugpaxpulsecap::read() {
  if (capturedBitCount==0) return 0;
  uint32_t rv = capturedMessage;
  capturedBitCount=0;
  capturedMessage=0;  
  return rv;
}


void cgh_rugpaxpulsecap_add_tick_to_queue(uint32_t timestamp) {
  // add ticks only while the queue is empty
  if (edgeEventHead == edgeEventTail) cgh_rugpaxpulsecap_add_event_to_queue(0, 'T', timestamp);
  
}

void cgh_rugpaxpulsecap_add_event_to_queue(uint8_t portRead, char portid, uint32_t timestamp) {
  uint8_t newhead = (edgeEventHead+1) & (edgeEventCount-1);
  if (newhead == edgeEventTail) return;
  struct edgeevent* ee = &edgeEvents[newhead];

  ee->portid = portid;
  ee->portRead = portRead;
  ee->timer=timestamp;
  edgeEventHead = newhead;
  handle_irq_queue();
}


volatile uint8_t in_ISRX=0;
void handle_irq_queue() {
  if (in_ISRX) return;
  in_ISRX=1;  
  interrupts();
  byte processed=0;

  while (edgeEventHead != edgeEventTail) {
    byte newtail = (edgeEventTail + 1) & (edgeEventCount-1);
    struct edgeevent *ee = &edgeEvents[newtail];
    
    for (rugpaxpulsecap *ei = first_rugpaxpulsecap_instance; ei != NULL; ) {
      ei = (rugpaxpulsecap*)(ei->_handle_irq(ee));  
    }
    edgeEventTail = newtail; 
    if (++processed==32) break;
  }

  in_ISRX=0;
  
}


void rugpaxpulsecap::_handle_edge(char edgeKind, uint32_t timediff32, uint16_t timediff) {

	
}

void* rugpaxpulsecap::_handle_irq(void *eev) {
  struct edgeevent *ee = (struct edgeevent*)eev;
    
	char edgeKind = 'T';  // T=timer R=rise F=fall
  
  
  // Shortcut: if the interrupt is a timer tick, but we're not in a word, then we don't care.      
  if (ee->portid=='T' && inword==0) {
    // don't care.  Timer ticks can only end messages, not begin them.
  } else if (ee->portid != 'T' && ee->portid != portid) {
    // if it's for a different instance we also don't care.        
  } else if (ee->portid != 'T' && (ee->portRead & mask) == lastRead) {
    // if it's for our port, but there's no change to our bits, then also don't care.
    // (this case gets tripped if a pulse is so short that it ended before we could read it)        
  } else if (protocol=='0' && ee->portid=='T') {
    // no need to give timer to both Wiegand pins otherwise we're timering twice, so, don't care        
  } else {
    if (ee->portid != 'T') {
      lastRead = ee->portRead & mask;         
      if (lastRead) edgeKind = 'R';       
      else edgeKind = 'F';
    }
    
    uint32_t rcvtime = ee->timer;
		long timediff32 = rcvtime - lastTimestamp;
		if (edgeKind != 'T') lastTimestamp = rcvtime;		
		switch (clockrate) {
			case 0: if (main_timer_is_micros) break; // micros is already in us
					timediff32 = timediff32 * 4; break; // ticks are in units of 4us	
			case 20: timediff32 = timediff32 * 4000; break; // ticks are 4us, want ns
			case 21: timediff32 = timediff32 * 500; break; // ticks are 0.5us, want ns
			case 22: while (timediff32 < 0) timediff32 += 65536;
				timediff32 = timediff32 * 125; break; // ticks are 125ns and not a diff
		}
		uint16_t timediff = (uint32_t)timediff32;
		if (timediff32 > 65535) timediff=65535;
	
	  _handle_edge(edgeKind,timediff32, timediff);
	  
    /*
    Serial.print("inword now ");
    Serial.println(inword);
    */
  }  
  return next_rugpaxpulsecap_instance;
}


int rugpaxpulsecap::begin(void) {

	int rv=0;

  noInterrupts();
  pinMode(pin, INPUT);

  if (pin==HWPINx) { // Using hardware capture
    portid = '0' + TIMERx;
    mask=1;
    lastRead = digitalRead(pin)==HIGH ? 1 : 0;
    
		if (pin==HWPINx) TIMSKx |= _BV(ICIE1); 
		rv=3;
     
  }  


  rugpaxpulsecap_began=true;

	// stop all timers so we can sync them
	//GTCCR = (1<<TSM)|(1<<PSRASY)|(1<<PSRSYNC);

	// do to timers 4,5 what we're doing on Uno/Nano to timer 1.

	// see comments for TCCR1x to expand the meaning of this.

	// WGM13,12,11,10 = 0
	TCCRxA &= ~(_BV(WGM11)|_BV(WGM10));
	TCCRxB &= ~(_BV(WGM13)|_BV(WGM12));
	
	// Set input capture edge detection to falling (we will flip this as we get edges)
	
	TCCRxB &= ~(_BV(ICES1));

	// Set prescaler to /64, so we're counting at 250KHz in units of 4ms
	TCCRxB &= ~(_BV(CS12));
	TCCRxB |= _BV(CS11)|_BV(CS10);

	// Turn on timer overflow interrupt
	TIMSKx |= _BV(TOIE1);
		
	// Turn on timer 0 compare B for our tick
	//TIMSK0 |= _BV(OCIE0B);  
	TIMSK5 |= _BV(OCIE1B);
	
	// synchronize all timers
	//TCNTx=0;
		
	// restart all timers
	//GTCCR = 0;


  interrupts();

  return rv;
}




void ISRX(uint8_t PINx, char _portid, uint16_t cntx, uint16_t cnty) {
	if (mainTimer==0) {
		cgh_rugpaxpulsecap_add_event_to_queue(PINx, _portid, micros());
		return;
	}

  uint16_t ovfs = ovfcount;
	if (mainTimer==TIMERx && TIFRx & _BV(TOV1)) ovfs++;
  uint32_t timestamp = ovfs;
  timestamp = timestamp * 65536;
  if (mainTimer==TIMERx) timestamp = timestamp + cntx;
  
  cgh_rugpaxpulsecap_add_event_to_queue(PINx, _portid, timestamp);

    
}


void finish_capture_isr(char portid, uint8_t gotfall, uint16_t icr, uint16_t tcnt) {

	uint16_t ovf = ovfcount;
	if (icr >= 0xc000 && tcnt < 0x4000) ovf++;
	if (icr < 0x4000 && tcnt >= 0xc000) ovf--;
	
	uint32_t timestamp = ovf;
	timestamp *= 65536;
	timestamp += icr;
	cgh_rugpaxpulsecap_add_event_to_queue(gotfall ? 0 : 1, portid, timestamp);
  
}

ISR(TIMERx_CAPT_vect) {
 
 	uint16_t icr = ICRx;
 	uint16_t tcnt = TCNTx;

  TCCRxB ^= _BV(ICES1); // alternate direction of edge of next capture  

  // did we capture a rise or fall?
  uint8_t gotfall =   TCCRxB & _BV(ICES1);
  finish_capture_isr('0'+TIMERx, gotfall,icr,tcnt);
}

ISR(TIMERx_OVF_vect) {
  if (mainTimer==TIMERx) ovfcount++;
}

static rugpax_irReceiver irrcvr(48);

int irMega48::begin() {
	return irrcvr.begin();
}


unsigned long irMega48::read() {
	return irrcvr.read();
}

struct irNEC irMega48::decodeNEC(unsigned long readval) {
	struct irNEC rv;
	rv.addr_id = 0, rv.cmd_id=0, rv.status = irNEC::REPEAT;
	if (readval==1) return rv;
	//uint32_t flipval=0;
	// bit order: 24 25 26 27 28 29 30 31 16 17 18 19 20 21 22 23 
	//for (int i=0; i<32; i++) {
	//	flipval <<= 1;
	//	if (readval & 1) flipval++;
	//	readval >>= 1;	
	//}
	rv.addr_id = readval; // will only copy 16 bits.
	if (((rv.addr_id >> 8) ^ (rv.addr_id & 0xFF) == 0xFF)) rv.addr_id &= 0xFF;
	rv.cmd_id = readval >> 16; // will only copy 8 bits
	rv.status = (((readval >> 24) ^ 0xFF) == rv.cmd_id) ? irNEC::VALID : irNEC::INVALID;
  return rv;

}


rugpax_irReceiver::rugpax_irReceiver(byte _pin) {
	init(_pin, 'I');
}



void rugpax_irReceiver::_handle_edge(char edgeKind, uint32_t timediff32, uint16_t timediff) {

	sumdiffs += timediff32;
	char freason=' ';


	byte wasinword = inword;	

	if (edgeKind=='F') {   // Fall         
		if (inword==0 || timediff >= 30000U) {    // first edge of a message?
			inword=1;
			bitsreceived=0;
		} else if (inword==1) { // second edge of a message?  (got the start bit, but we're before the first data bit)
			if (timediff > 2000 && timediff < 2500) {

				if (lastMessageTimestamp) {				
					if (sumdiffs - lastMessageTimestamp < 262144) {
						// got the signal that says button is being held down.
						// While inword==0, capbuf is borrowed to be the timestamp of the last good message.
						// for a repeat to be valid, we needed to receive a good message in the last 125ms,
						// and the non-ISR code needs to have picked up the original message to know what to repeat.
						if (capturedBitCount==0) {
								capturedBitCount=1;
								capturedMessage=1;
						}
						if (edgeKind=='T') { // Timer Tick
				
							inword=0;
							freason='a';
						}
						lastMessageTimestamp=sumdiffs;
					} else {
						lastMessageTimestamp=0;
					}
				}
					
			} else
			// expecting about 4450
			if (timediff < 4000 || timediff > 5000) {
				inword=0;
				freason='b';
			
			} else {
				inword=2;
				capbuf=0;               
			}
			// while in a word,
			// the amount of time after a fall (i.e. the beginning of an IR pulse, since pulse is low)
			// tells us what bit it is.              
			// expecting around 562.5 for a low bit, or 1687.5 for a high bit.
			// look for anything deviating from that, cancelling our word if so.
		} else if (inword != 2) {
			inword=0;
			freason='c';
		
		} else if (timediff < 400 || timediff > 1880) { // timediff > 20500 || (timediff > 1830 && timediff < 19500)) {      

			inword=0;
			freason='d';
		
		} else if (timediff > 800 && timediff < 1400) {

			inword=0;
			freason='e';
		} else if (bitsreceived < 32) {
			if (timediff > 1000) capbuf |= (0x80000000 >> (bitsreceived^15));     
			bitsreceived++;
			if (bitsreceived==32) {
				capturedMessage = capbuf;
				lastMessageTimestamp = sumdiffs;    
				capbuf=0;          
				capturedBitCount=32;
				inword=0; 
				freason='f';          
					 
			}
		}
	} else if (edgeKind=='R') { // gotrise          
		// normal rises only come with a time diff of:
		// ~9000 for a sync bit
		// ~4500 for a sync bit on some other remote I saw
		// ~600 for a normal pulse
		// rises don't start messages because IR receiver is idle high.
		// rises can be shorter than expected in case of low light/contrast.            
		if (timediff > 11000) inword=0;
		if (inword==2 && (timediff < 300 || timediff > 950)) freason='h',inword=0;
	} else if (edgeKind=='T' && timediff > 10000) {
		inword=0;
		freason='g';
	
	}

}
