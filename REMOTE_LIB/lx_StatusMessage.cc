/*  lx_StatusMessage.cc -- Mount message with mount's current status
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
#include "lx_StatusMessage.h"

//
// Message format: (total length == ??)
//
// bytes 0-3	size
//       4	message ID
//       5      Server Status
//       6	Scope Status
//	 7-10	Focus Setting: C14
//       11-14  Focus Setting: Esatto
// 
// There is no response to this message.
//

#define SERVERSTATUS_BYTE     5
#define SCOPESTATUS_BYTE     (SERVERSTATUS_BYTE+1)
#define FOCUSC14BYTE_LOW     (SCOPESTATUS_BYTE+1)
#define FOCUSESATTOBYTE_LOW  (FOCUSC14BYTE_LOW+4)

#define MESSAGE_SIZE (FOCUSESATTOBYTE_LOW+4+1)

#define NETFOCUS_OFFSET       1000000

////////////////////////////////////////////////////////////////
//        Constructors
////////////////////////////////////////////////////////////////
lxStatusMessage::lxStatusMessage(int Socket,
				 int ServerStatus,
				 int ScopeStatus) :
lxGenMessage(Socket, MESSAGE_SIZE) {
	     
    content[4] = lxStatusMessageID;
    content[SERVERSTATUS_BYTE]      = ServerStatus;
    content[SCOPESTATUS_BYTE]      = ScopeStatus;
  }


lxStatusMessage::lxStatusMessage(lxGenMessage *message) :
  lxGenMessage(message) {

    if(GenMessSize != MESSAGE_SIZE ||
       MessageID() != lxStatusMessageID) {
      fprintf(stderr,
	      "lxStatusMessage: constructor reasonableness check failed.\n");
    }
  }

int
lxStatusMessage::GetServerStatus(void) {
  return content[SERVERSTATUS_BYTE];
}

int
lxStatusMessage::GetScopeStatus(void) {
  return content[SCOPESTATUS_BYTE];
}

void
lxStatusMessage::SetFocusPositionC14(long FocusData) {
  lx_pack_4byte_int(content+FOCUSC14BYTE_LOW, NETFOCUS_OFFSET+FocusData);
}

long           
lxStatusMessage::GetFocusPositionC14(void) {
  return lx_get_4byte_int(content+FOCUSC14BYTE_LOW) - NETFOCUS_OFFSET;
} 

void
lxStatusMessage::SetFocusPositionEsatto(long FocusData) {
  lx_pack_4byte_int(content+FOCUSESATTOBYTE_LOW, NETFOCUS_OFFSET+FocusData);
}

long           
lxStatusMessage::GetFocusPositionEsatto(void) {
  return lx_get_4byte_int(content+FOCUSESATTOBYTE_LOW) - NETFOCUS_OFFSET;
} 
