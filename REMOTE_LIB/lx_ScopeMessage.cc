/*  lx_ScopeMessage.cc -- Mount message with general-purpose command
 *  to be passed directly to the mount controller 
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
#include "lx_ScopeMessage.h"

#define LONGEST_OUTBOUND_STRING 72 // not counting terminating null
#define MAX_SINGLE_CHAR_RESPONSES 8

//
// Message format:
//
// bytes 0-3	size
//       4	message ID
//       5      scope command length (not counting terminating null)
//       6      response count
//       7      execution time enumeration
//       8      response type enumeration
//       9-16   list of single-character responses, zero in all unused spots
//       17-33  scope message, null-terminated
// 
// The only response to this message is a StatusMessage.
//

////////////////////////////////////////////////////////////////
//        Constructors
////////////////////////////////////////////////////////////////

lxScopeMessage::lxScopeMessage(int Socket,
			       const char *MessageString,
			       ExecutionChoices ExecutionTime,
			       ResponseTypeChoices ResponseType,
			       int ResponseCharCount,
			       const char *SingleCharResponseArray) :
  lxGenMessage(Socket, 17+LONGEST_OUTBOUND_STRING) {
  content[4] = lxScopeMessageID;
  content[5] = strlen(MessageString);
  if(content[5] > LONGEST_OUTBOUND_STRING) {
    fprintf(stderr, "lx_ScopeMessage: Message too long (%d > %d)\n",
	    content[5], LONGEST_OUTBOUND_STRING);
    return;
  }
  content[6] = ResponseCharCount;
  content[7] = ExecutionTime;
  content[8] = ResponseType;
  strcpy((char *) content + 17, MessageString);

  bzero(content + 9, MAX_SINGLE_CHAR_RESPONSES);
  if(SingleCharResponseArray) {
    strncpy((char *) content + 9,
	    SingleCharResponseArray,
	    MAX_SINGLE_CHAR_RESPONSES);
  }
}

lxScopeMessage::lxScopeMessage(lxGenMessage *message) :
  lxGenMessage(message) {
  if(GenMessSize != 17+LONGEST_OUTBOUND_STRING ||
     MessageID() != lxScopeMessageID) {
    fprintf(stderr,
	    "lxScopeMessage: constructor reasonableness check failed.\n");
  }
}

////////////////////////////////////////////////////////////////
//        Utility
////////////////////////////////////////////////////////////////

const char *lxScopeMessage::GetMessageString(void) {
  return (const char *) content+17;
}

ExecutionChoices    lxScopeMessage::GetExecutionTime(void) {
  return (ExecutionChoices) content[7];
}

ResponseTypeChoices lxScopeMessage::GetResponseType(void) {
  return (ResponseTypeChoices) content[8];
}

int                 lxScopeMessage::GetResponseCharCount(void) {
  return content[6];
}

// the array of single character responses (terminated with a
// trailing null) will be put into the character array pointed to by
// "buffer".
void
lxScopeMessage::GetSingleCharacterResponses(char *buffer) {
  if(buffer == 0) {
    fprintf(stderr, "lx_ScopeMessage: GetSingleCharacterResponse: nil buffer\n");
    return;
  }

  int n;
  for(n=0; n<MAX_SINGLE_CHAR_RESPONSES; n++) {
    char c = content[9+n];
    *buffer++ = c;
    if(c == 0) break;
  }
  *buffer = 0;
}
