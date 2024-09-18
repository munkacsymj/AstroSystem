/*  lx_gen_message.cc -- Mount message superclass
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
#include <sys/types.h>
#include <sys/uio.h>		// for write()
#include <unistd.h>		// for write()
#include <stdlib.h>		// for malloc(), free()
#include <stdio.h>
#include <errno.h>
#include "lx_gen_message.h"
#include "lx_RequestStatusMessage.h"
#include "lx_StatusMessage.h"
#include "lx_FocusMessage.h"
#include "lx_ScopeMessage.h"
#include "lx_ScopeResponseMessage.h"
#include "lx_TrackMessage.h"
#include "lx_FlatLightMessage.h"
#include "lx_ResyncMessage.h"

lxGenMessage::lxGenMessage(int socket,
			   int size) {
  GenMessSize = size;
  SocketID    = socket;

  if(size < 2) {
    fprintf(stderr, "lxGenMessage: size %d is illegal\n", size);
    size = 2;
  }

  content = (unsigned char *)malloc(size);
  if(!content) {
    fprintf(stderr, "lxGenMessage: unable to allocate memory for message\n");
  }
  lx_pack_4byte_int(content, size);
}

lxGenMessage::~lxGenMessage(void) {
  free(content);
}

int
lxGenMessage::send(void) {
  int total_bytes_written = 0;
  const unsigned char MagicNumber = lxMagicValue; // from gen_message.h

				// Write the Magic Number first
  while(write(SocketID, &MagicNumber, 1) == 0) {
    ; // null
  }

  while(total_bytes_written < GenMessSize) {
    int bytes_written;

    bytes_written = write(SocketID,
			  content + total_bytes_written,
			  GenMessSize - total_bytes_written);
    if(bytes_written < 0) {
      perror("Error writing message to socket");
      return -1;
    }

    total_bytes_written += bytes_written;
  }
  return 0;			// success
}

int lx_fetch_bytes(int socket,
		unsigned char *buffer,
		int count) {
  int total_count = 0;

  while(total_count < count) {
    int bytes_read;

    bytes_read = read(socket, buffer + total_count, count - total_count);
    if(bytes_read < 0) {
      if(errno == EINTR) continue;
      perror("Cannot read from socket(4)");
      return bytes_read;
    } else {
      if(bytes_read == 0) {
	return 0;
      }
    }
    total_count += bytes_read;
  }

  return total_count;
}
  
lxGenMessage * lxGenMessage::ReceiveMessage(int socket) {
  lxGenMessage *new_message = 0;
  unsigned char preface[5];	// holds magic # and byte count
  int bytes_read;

  do {
    bytes_read = lx_fetch_bytes(socket, preface, 5);
  } while(bytes_read == 0 &&
	  errno == EINTR);
  if(bytes_read != 5) {
    return 0;
  }

  if(preface[0] != lxMagicValue) {
    // Uh-oh. Somehow we've lost sync. This is bad news.
    fprintf(stderr, "gen_message: message sync lost! giving up.\n");
    // If we were really smart we'd try to re-sync.
    return 0;
  }

  const int MessageSize = lx_get_4byte_int(preface+1); 
  
  if(MessageSize < 2) {
    fprintf(stderr, "gen_message: inbound msg size too small.\n");
    return 0;
  }

  // create the new message
  lxGenMessage *message = new lxGenMessage(socket, MessageSize);
  int bytes_remaining = message->GenMessSize - (sizeof(preface) - 1);

  for(int j=0; j<4; j++) {
    message->content[j] = preface[j+1];
  }

  if(lx_fetch_bytes(socket,
		 message->content+sizeof(preface)-1,
		 bytes_remaining) < 0) {
    // something went wrong.
    delete message;
    return 0;
  }

  {

    switch(message->MessageID()) {
    case lxRequestStatusMessageID:
      new_message = new lxRequestStatusMessage(message);
      break;
    case lxStatusMessageID:
      new_message = new lxStatusMessage(message);
      break;
    case lxFocusMessageID:
      new_message = new lxFocusMessage(message);
      break;
    case lxScopeMessageID:
      new_message = new lxScopeMessage(message);
      break;
    case lxScopeResponseMessageID:
      new_message = new lxScopeResponseMessage(message);
      break;
    case lxTrackMessageID:
      new_message = new lxTrackMessage(message);
      break;
    case lxResyncMessageID:
      new_message = new lxResyncMessage(message);
      break;
    case lxFlatLightMessageID:
      new_message = new lxFlatLightMessage(message);
      break;
    default:
      fprintf(stderr, "Unable to handle inbound message ID = 0x%02x\n",
	      message->MessageID());
    }

    delete message;
  }
  return new_message;
}
  

lxGenMessage::lxGenMessage(lxGenMessage *message) {
  GenMessSize = message->GenMessSize;
  SocketID    = message->SocketID;

  content = (unsigned char *)malloc(message->GenMessSize);
  if(!content) {
    fprintf(stderr, "lxGenMessage: unable to allocate memory for message\n");
  }

  for(int j=0; j<GenMessSize; j++) {
    content[j] = message->content[j];
  }
}
    
void
lx_pack_4byte_int(unsigned char *p, int val) {
  *p++ = (val & 0xff);
  *p++ = (val & 0xff00) >> 8;
  *p++ = (val & 0xff0000) >> 16;
  *p   = (val & 0xff000000) >> 24;
}

unsigned long
lx_get_4byte_int(unsigned char *p) {
  unsigned long result =
    ((*p) |
     (*(p+1) << 8) |
     (*(p+2) << 16) |
     (*(p+3) << 24));

  return result;
}
