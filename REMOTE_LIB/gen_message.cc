/*  gen_message.cc -- Camera message superclass
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
#include <errno.h>		// for EINTR
#include <sys/uio.h>		// for write()
#include <unistd.h>		// for write()
#include <stdlib.h>		// for malloc(), free()
#include <stdio.h>
#include "gen_message.h"
#include "camera_message.h"
#include "RequestStatusMessage.h"
#include "StatusMessage.h"
#include "FITSMessage.h"
#include <iostream>
using namespace std;

GenMessage::GenMessage(int socket,
		       int size) {
  GenMessSize = size;
  SocketID    = socket;

  if(size < 2) {
    fprintf(stderr, "GenMessage: size %d is illegal\n", size);
    size = 2;
  }

  content = (unsigned char *)malloc(size);
  if(!content) {
    fprintf(stderr, "GenMessage: unable to allocate memory for message\n");
  }
  pack_4byte_int(content, size);
}

void
GenMessage::Resize(int newsize) {
  free(content);
  content = (unsigned char *) malloc(newsize);
  if (!content) {
    fprintf(stderr, "GenMessage; unable to allocate memory for resize\n");
  }
  pack_4byte_int(content, newsize);
  GenMessSize = newsize;
}

GenMessage::~GenMessage(void) {
  free(content);
}

int GenMessage::send(void) {
  int total_bytes_written = 0;
  const unsigned char MagicNumber = MagicValue; // from gen_message.h

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

  //cerr << "GenMessage::send() wrote Magic Number + "
  //     << total_bytes_written << " bytes." << endl;
  return 0;
}

int fetch_bytes(int socket,
		unsigned char *buffer,
		int count) {
  int total_count = 0;
  int zero_count = 0;

  while(total_count < count) {
    int bytes_read;

    bytes_read = read(socket, buffer + total_count, count - total_count);
    if(bytes_read < 0) {
      if(errno == EINTR) continue;
      perror("Cannot read from socket(4)");
      return bytes_read;
    } else {
      if(bytes_read == 0) {
	fprintf(stderr, "socket read returned 0.\n");
	if(zero_count++ > 30) return 0;
      }
    }
    total_count += bytes_read;
  }

  return total_count;
}
  
GenMessage * GenMessage::ReceiveMessage(int socket) {
  GenMessage *new_message = 0;
  unsigned char preface[5];	// holds magic # and byte count

  if(fetch_bytes(socket, preface, 5) != 5) {
    return 0;
  }

  if(preface[0] != MagicValue) {
    // Uh-oh. Somehow we've lost sync. This is bad news.
    fprintf(stderr, "gen_message: message sync lost! giving up.\n");
    // If we were really smart we'd try to re-sync.
    return 0;
  }

  const int MessageSize = get_4byte_int(preface+1);
  
  if(MessageSize < 2) {
    fprintf(stderr, "gen_message: inbound msg size too small.\n");
    return 0;
  }

  // create the new message
  GenMessage *message = new GenMessage(socket, MessageSize);
  int bytes_remaining = message->GenMessSize - (sizeof(preface) - 1);

  //cerr << "gen_message: fetching 5 + " << bytes_remaining << "bytes." << endl;

  for(int j=0; j<4; j++) {
    message->content[j] = preface[j+1];
  }

  if(fetch_bytes(socket,
		 message->content+sizeof(preface)-1,
		 bytes_remaining) < 0) {
    // something went wrong.
    delete message;
    return 0;
  }

  {

    switch(message->MessageID()) {
    case CameraMessageID:
      new_message = new CameraMessage(message);
      break;
    case RequestStatusMessageID:
      new_message = new RequestStatusMessage(message);
      break;
    case StatusMessageID:
      new_message = new StatusMessage(message);
      break;
    case FITSMessageID:
      new_message = new FITSMessage(message);
      break;
    default:
      fprintf(stderr, "Unable to handle inbound message ID = 0x%02x\n",
	      message->MessageID());
    }

    delete message;
  }
  return new_message;
}
  

GenMessage::GenMessage(GenMessage *message) {
  GenMessSize = message->GenMessSize;
  SocketID    = message->SocketID;

  content = (unsigned char *)malloc(message->GenMessSize);
  if(!content) {
    fprintf(stderr, "GenMessage: unable to allocate memory for message\n");
  }

  for(int j=0; j<GenMessSize; j++) {
    content[j] = message->content[j];
  }
}
    
void
pack_4byte_int(unsigned char *p, int val) {
  *p++ = (val & 0xff);
  *p++ = (val & 0xff00) >> 8;
  *p++ = (val & 0xff0000) >> 16;
  *p   = (val & 0xff000000) >> 24;
}

unsigned long
get_4byte_int(unsigned char *p) {
  unsigned long result =
    ((*p) |
     (*(p+1) << 8) |
     (*(p+2) << 16) |
     (*(p+3) << 24));

  return result;
}
