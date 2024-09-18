/*  lx_ResyncMessage.cc -- Mount message to re-synchronize
 *  communications between computer and mount controller (unique to
 *  Gemini) 
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
#include "lx_ResyncMessage.h"

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


lxResyncMessage::lxResyncMessage(int Socket) :
  lxGenMessage(Socket, 5) {
	     
    content[4] = lxResyncMessageID;
}


lxResyncMessage::lxResyncMessage(lxGenMessage *message) :
  lxGenMessage(message) {

    if(GenMessSize != 5 ||
       MessageID() != lxResyncMessageID) {
      fprintf(stderr,
	      "lxResyncMessage: constructor reasonableness check failed.\n");
    }
  }

