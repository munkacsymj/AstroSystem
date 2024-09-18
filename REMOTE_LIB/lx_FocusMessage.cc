/*  lx_FocusMessage.cc -- Mount message to control focus motor
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
#include <stdio.h>
#include "lx_FocusMessage.h"

//
// Message format:
//
// bytes 0-3	size
//       4	message ID
//       5      Focus Flags
//       6-9    Focus amount (byte 6 = lsb; the value is the motor
//                  running time in msec, with 1,000,000 added so that
//                  all values are positive)
// 
// The only response to this message is a StatusMessage.
//

////////////////////////////////////////////////////////////////
//        Constructors
////////////////////////////////////////////////////////////////

#define FOCUS_OFFSET 1000000

lxFocusMessage::lxFocusMessage(int Socket,
			       int FocusFlags,
			       int FocusTravelInMsec) :
  lxGenMessage(Socket, 10) {
	     
    content[4] = lxFocusMessageID;
    content[5] = FocusFlags;
    lx_pack_4byte_int(content+6, FOCUS_OFFSET+FocusTravelInMsec);
}


lxFocusMessage::lxFocusMessage(lxGenMessage *message) :
  lxGenMessage(message) {

    if(GenMessSize != 10 ||
       MessageID() != lxFocusMessageID) {
      fprintf(stderr,
	      "lxFocusMessage: constructor reasonableness check failed.\n");
    }
  }

int
lxFocusMessage::GetFocusTravelInMsec(void) {
  return lx_get_4byte_int(content+6) - FOCUS_OFFSET;
}

bool
lxFocusMessage::FocusTravelIsAbsolute(void) {
  return (content[5] & FOCUS_FLAG_ABSOLUTE);
}

bool
lxFocusMessage::FocuserIsC14(void) {
  return (content[5] & FOCUS_FLAG_C14);
}



