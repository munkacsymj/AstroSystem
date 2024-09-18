/*  lx_RequestStatusMessage.cc -- Mount message to request mount status
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
#include "lx_RequestStatusMessage.h"

//
// Message format:
//
// bytes 0-3	size
//       4	message ID
// 
// The only response to this message is a StatusMessage.
//

////////////////////////////////////////////////////////////////
//        Constructors
////////////////////////////////////////////////////////////////

lxRequestStatusMessage::lxRequestStatusMessage(int Socket) :
  lxGenMessage(Socket, 5) {
	     
    content[4] = lxRequestStatusMessageID;
}


lxRequestStatusMessage::lxRequestStatusMessage(lxGenMessage *message) :
  lxGenMessage(message) {

    if(GenMessSize != 5 ||
       MessageID() != lxRequestStatusMessageID) {
      fprintf(stderr,
	      "lxRequestStatusMessage: constructor reasonableness check failed.\n");
    }
  }
