/*  lx_TrackMessage.cc -- Mount message with small guiding command
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
#include "lx_TrackMessage.h"

//
// Message format:
//
// bytes 0-3	size
//       4	message ID
//       5-8    North Track amount (byte 5 = lsb; the value is the motor
//                  running time in msec, with 1,000,000 added so that
//                  all values are positive)
//       9-12   East Track amount (byte 9 = lsb; the value is the motor
//                  running time in msec, with 1,000,000 added so that
//                  all values are positive)
// 
// The only response to this message is a StatusMessage.
//

////////////////////////////////////////////////////////////////
//        Constructors
////////////////////////////////////////////////////////////////

#define TRACK_OFFSET 1000000

lxTrackMessage::lxTrackMessage(int Socket,
			       int NorthMSec,
			       int EastMSec) :
  lxGenMessage(Socket, 13) {
	     
    content[4] = lxTrackMessageID;
    lx_pack_4byte_int(content+5, TRACK_OFFSET+NorthMSec);
    lx_pack_4byte_int(content+9, TRACK_OFFSET+EastMSec);
}


lxTrackMessage::lxTrackMessage(lxGenMessage *message) :
  lxGenMessage(message) {

    if(GenMessSize != 13 ||
       MessageID() != lxTrackMessageID) {
      fprintf(stderr,
	      "lxTrackMessage: constructor reasonableness check failed.\n");
    }
  }

int
lxTrackMessage::GetTrackNorthTimeInMsec(void) {
  return lx_get_4byte_int(content+5) - TRACK_OFFSET;
}

int
lxTrackMessage::GetTrackEastTimeInMsec(void) {
  return lx_get_4byte_int(content+9) - TRACK_OFFSET;
}

