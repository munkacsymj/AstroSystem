/*  lx_ScopeResponseMessage.cc -- Mount message with mount's response to a
 * general-purpose command
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
#include <string.h>
#include <stdio.h>
#include "lx_ScopeResponseMessage.h"

#define LONGEST_RESPONSE_STRING 36 // not counting terminating null

//
// Message format:
//
// bytes 0-3	size
//       4	message ID
//       5      response string length (not counting terminating null)
//       6      response status enumeration
//       7-23   scope message, null-terminated
// 
//

////////////////////////////////////////////////////////////////
//        Constructors
////////////////////////////////////////////////////////////////

lxScopeResponseMessage::lxScopeResponseMessage(int Socket,
					       char *MessageString,
					       ScopeResponseStatus Status) :
  lxGenMessage(Socket, 7+LONGEST_RESPONSE_STRING) {
  content[4] = lxScopeResponseMessageID;
  content[5] = strlen(MessageString);
  if(content[5] > LONGEST_RESPONSE_STRING) {
    fprintf(stderr, "lx_ScopeResponseMessage: Message too long (%d > %d)\n",
	    content[5], LONGEST_RESPONSE_STRING);
    return;
  }
  content[6] = Status;
  strcpy((char *) content + 7, MessageString);
}

lxScopeResponseMessage::lxScopeResponseMessage(lxGenMessage *message) :
  lxGenMessage(message) {
  if(GenMessSize != 7+LONGEST_RESPONSE_STRING ||
     MessageID() != lxScopeResponseMessageID) {
    fprintf(stderr,
	    "lxScopeResponseMessage: constructor reasonableness check failed.\n");
  }
}

////////////////////////////////////////////////////////////////
//        Utility
////////////////////////////////////////////////////////////////

char *lxScopeResponseMessage::GetMessageString(void) {
  return (char *) content+7;
}

ScopeResponseStatus    lxScopeResponseMessage::GetStatus(void) {
  return (ScopeResponseStatus) content[6];
}

