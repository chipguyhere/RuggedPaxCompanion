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



// A specialized fork of chipguyhere/PulseCapture that has been
// reduced to the single purpose of taking over Timer5 and using it to capture NEC
// infrared remote signals on pin 48

struct irNEC {
  uint16_t addr_id;
  uint8_t cmd_id;
  enum Status { INVALID, VALID, REPEAT };
  Status status;
  
};

class irMega48 {
public:

	// Initialize the IR receiver, and configure on-chip Timer5 to read IR messages 
	// in the background.
	// example: ir.begin();
	int begin();
	
	// Reads the most recent IR message, which can be up to 32 bits.
	// The 32 bits might typically be 4 8-bit values: MFGID ~MFGID CMDID ~CMDID
	// but transmitted LSB first (so may require reversal)
	// The value 0 indicates there was no message.
	// The value 1 indicates there was a 1-bit "repeat" message received, that signifies
	// that a button is being held down.  This is a distinct 1-bit message sent by IR
	// remotes that differs from receiving the full message again, which means new keypress.
	// example ir.read();
	unsigned long read();
	struct irNEC decodeNEC(unsigned long readval);
};
