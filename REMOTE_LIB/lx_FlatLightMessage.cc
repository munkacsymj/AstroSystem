/*  FlatLightMessage.cc -- Mount message to control flat light panel
 *
 *  Copyright (C) 2020 Mark J. Munkacsy

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
#include <stdio.h>
#include "lx_FlatLightMessage.h"

//
// Message format:
//
// bytes 0-3	size
//       4	message ID
//       5      FlatLightFlags (direction bits)
//
// The response to this message is a FlatLightMessage
//

#define FLATFLAGS_BYTE        5

#define MESSAGE_SIZE          6

////////////////////////////////////////////////////////////////
//        Constructors
////////////////////////////////////////////////////////////////

lxFlatLightMessage::lxFlatLightMessage(int Socket) :
  lxGenMessage(Socket, MESSAGE_SIZE) {
  content[4] = lxFlatLightMessageID;
  content[FLATFLAGS_BYTE] = 0;
}

lxFlatLightMessage::lxFlatLightMessage(lxGenMessage *message) :
  lxGenMessage(message) {
  if(GenMessSize != MESSAGE_SIZE ||
     MessageID() != lxFlatLightMessageID) {
    fprintf(stderr,
	    "lxFlatLightMessage: constructor reasonableness check failed.\n");
  }
}

lxFlatLightMessage::~lxFlatLightMessage(void) { ; }

bool
lxFlatLightMessage::GetFlatLightDirUp(void) { // used in the server
  return (content[FLATFLAGS_BYTE] & FLAT_MOVE_UP);
}

unsigned char
lxFlatLightMessage::GetStatusByte(void) { // used in the client
  return (content[FLATFLAGS_BYTE]);
}

void
lxFlatLightMessage::SetStatusByte(unsigned char status) {
  content[FLATFLAGS_BYTE] = status;
}

void
lxFlatLightMessage::SetDirectionByte(unsigned char direction) {
  content[FLATFLAGS_BYTE] = direction;
}

bool
lxFlatLightMessage::MoveCommanded(void) {
  return (content[FLATFLAGS_BYTE] != 0);
}



