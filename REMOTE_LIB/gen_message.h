/* This may look like C code, but it is really -*-c++-*- */
/*  gen_message.h -- Camera message superclass
 *
 *  Copyright (C) 2007 Mark J. Munkacsy

 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program (file: COPYING).  If not, see
 *   <http://www.gnu.org/licenses/>. 
 */
#ifndef _GEN_MESSAGE_H		/* only include this once */
#define _GEN_MESSAGE_H

class GenMessage {
public:
  GenMessage(int socket,
	     int size);		// create general mbessage
  virtual ~GenMessage(void);	// destructor

  virtual int send(void);	// returns -1 if error
  unsigned char MessageID(void) { return content[4]; }
  int MessageSize(void) { return GenMessSize; }
  void Resize(int newsize);

  static GenMessage *ReceiveMessage(int socket);

protected:
  unsigned char *content;
  int GenMessSize;		// number of bytes in message (doesn't
				// count Magic number) 
  GenMessage(GenMessage *message);
  int SocketID;			// file descriptor for the associated socket
private:
  // nothing
};

#define MagicValue 0x73		// must fit in one byte

#define ExposeMessageID        0x90
#define RequestStatusMessageID 0x91
#define StatusMessageID        0x92
#define FITSMessageID          0x93
#define CoolerMessageID        0x94
#define FilterQueryMessageID   0x95
#define FilterDataMessageID    0x96
#define CameraMessageID        0x97

// Message format:
// First byte: 0x73 (MagicValue) {not stored in content[] }
// 2nd byte:   size1 (low-order 8 bits)
// 3rd byte:   size2
// 4th byte:   size3
// 5th byte:   size4 (high-order 8 bits)
// 6th byte:   messageID

// All messages are a minimum of 6 bytes long. GenMessSize does not
// include the magic number byte in its count, so the minimum
// GenMessSize is 5.

int fetch_bytes(int socket,
		unsigned char *buffer,
		int count);

void
pack_4byte_int(unsigned char *p, int val);

unsigned long
get_4byte_int(unsigned char *p);


#endif
