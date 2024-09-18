/* This may look like C, but it's really -*-c++-*- */
/*  lx_gen_message.h -- Mount message superclass
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
#ifndef _LX_GEN_MESSAGE_H
#define _LX_GEN_MESSAGE_H

class lxGenMessage {
public:
  lxGenMessage(int socket,
	     int size);		// create general mbessage
  ~lxGenMessage(void);		// destructor

  int send(void);		// returns -1 if error
  unsigned char MessageID(void) { return content[4]; }

  static lxGenMessage *ReceiveMessage(int socket);

protected:
  unsigned char *content;
  int GenMessSize;		// number of bytes in message (doesn't
				// count Magic number) 
  lxGenMessage(lxGenMessage *message);
  int SocketID;			// file descriptor for the associated socket
private:
  // nothing
};

#define lxMagicValue 0x74		// must fit in one byte

#define lxRequestStatusMessageID 0x71
#define lxStatusMessageID        0x72
#define lxFocusMessageID         0x73
#define lxScopeMessageID         0x74
#define lxScopeResponseMessageID 0x75
#define lxTrackMessageID	 0x76
#define lxResyncMessageID	 0x77
#define lxFlatLightMessageID     0x78

// Message format:
// First byte: 0x74 (MagicValue) {not stored in content[] }
// 2nd byte:   size1 (low-order 8 bits)
// 3rd byte:   size2
// 4th byte:   size3
// 5th byte:   size4 (high-order 8 bits)
// 6th byte:   messageID

// All messages are a minimum of 6 bytes long. GenMessSize does not
// include the magic number byte in its count, so the minimum
// GenMessSize is 5. When customizing messages or creating new ones,
// dont't touch anything in the first 6 bytes.

int lx_fetch_bytes(int socket,
		unsigned char *buffer,
		int count);

void
lx_pack_4byte_int(unsigned char *p, int val);

unsigned long
lx_get_4byte_int(unsigned char *p);


#endif
