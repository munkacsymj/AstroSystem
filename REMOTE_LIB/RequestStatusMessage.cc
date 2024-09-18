/*  RequestStatusMessage.cc -- Camera message asking for status of camera
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
#include "RequestStatusMessage.h"

//
// Message format:
//
// bytes 0-3	size
//       4	message ID
//       5      Unique ID
// 
// The only response to this message is a StatusMessage.
//

////////////////////////////////////////////////////////////////
//        Constructors
////////////////////////////////////////////////////////////////

static char next_unique_id = 1;

RequestStatusMessage::RequestStatusMessage(int Socket) :
  GenMessage(Socket, 6) {
	     
    content[4] = RequestStatusMessageID;
    content[5] = next_unique_id++;
  }


RequestStatusMessage::RequestStatusMessage(GenMessage *message) :
  GenMessage(message) {

    if(GenMessSize != 6 ||
       MessageID() != RequestStatusMessageID) {
      fprintf(stderr,
	      "RequestStatusMessage: constructor reasonableness check failed.\n");
    }
  }

char
RequestStatusMessage::GetUniqueID(void) {
  return content[5];
}
