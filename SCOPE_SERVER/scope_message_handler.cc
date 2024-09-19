/*  scope_message_handler.cc -- Handle server-side messages with
 *  commands to be sent to the mount
 *
 *  Copyright (C) 2007,2020 Mark J. Munkacsy

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
#include <string.h>
#include <sys/time.h>		// struct timeval
#include <sys/types.h>
#include <unistd.h>		// select()
#include "lx_gen_message.h"
#include "scope_message_handler.h"
#include "lx_FocusMessage.h"
#include "lx_StatusMessage.h"
#include "lx_ScopeMessage.h"
#include "lx_ScopeResponseMessage.h"
#include "lx_TrackMessage.h"
#include "lx_FlatLightMessage.h"
#include "lx_ResyncMessage.h"
#include "flatlight.h"
#include "focus.h"
#include "track.h"
#include "lx200.h"

#define DEBUG1 1		// set to zero to shut off messages

void send_status_message(int socket_fd, int status) {
  lxStatusMessage *outbound = new lxStatusMessage(socket_fd,
						  LX_SERVER_READY,
						  status);
<<<<<<< scope_message_handler.cc
  outbound->SetFocusPosition(c14cum_focus_position());
=======
  outbound->SetFocusPositionC14(c14cum_focus_position());
  outbound->SetFocusPositionEsatto(esattocum_focus_position());
>>>>>>> 1.9
  outbound->send();
  fprintf(stderr, "status message focus position = %ld (C14), %ld (Esatto)\n",
	  outbound->GetFocusPositionC14(), outbound->GetFocusPositionEsatto());
  delete outbound;
}

/****************************************************************/
/*        handle_focus_message()				*/
/*    Handle inbound focus message.				*/
/****************************************************************/
void handle_focus_message(lxFocusMessage *msg, int socket_fd) {
  int amount_in_msec = (msg->GetFocusTravelInMsec());
  bool is_absolute_request = (msg->FocusTravelIsAbsolute());
  int direction = ((amount_in_msec < 0) ?
		   DIRECTION_IN :
		   DIRECTION_OUT);
  if (is_absolute_request) direction = NO_DIRECTION_MOVE_ABSOLUTE;
  if(amount_in_msec < 0) amount_in_msec = -amount_in_msec;

  fprintf(stderr, "Sending busy message.\n");
  send_status_message(socket_fd, SCOPE_IO_BUSY);

  if (!is_absolute_request) {
    fprintf(stderr, "Running focus motor for %d msec.\n", amount_in_msec);
  } else {
    fprintf(stderr, "Setting focus to position %d.\n", amount_in_msec);
  }
<<<<<<< scope_message_handler.cc
  c14focus(direction, amount_in_msec);
=======

  if (msg->FocuserIsC14()) {
    c14focus(direction, amount_in_msec);
  } else {
    esattofocus(direction, amount_in_msec);
  }
>>>>>>> 1.9
  fprintf(stderr, "Sending Ready message.\n");
  send_status_message(socket_fd, SCOPE_IDLE);
}

/****************************************************************/
/*        handle_flatlight_message()				*/
/*    Handle inbound track message.				*/
/****************************************************************/
void handle_flatlight_message(lxFlatLightMessage *msg, int socket_fd) {
  bool move_direction_up = msg->GetFlatLightDirUp();
  bool move_commanded = msg->MoveCommanded();
  if (move_commanded) {
    if (move_direction_up) {
      FlatLightMoveUp();
    } else {
      FlatLightMoveDown();
    }
    sleep(1); // allow motor to start moving
  }

  // SendFlatLightStatusMessage
  lxFlatLightMessage *outbound = new lxFlatLightMessage(socket_fd);
  outbound->SetStatusByte(GetFlatLightStatusByte());
  outbound->send();
  fprintf(stderr, "FlatLightResponse status = 0x%02x\n",
	  outbound->GetStatusByte());
  delete outbound;
}

/****************************************************************/
/*        handle_track_message()				*/
/*    Handle inbound track message.				*/
/****************************************************************/
void handle_track_message(lxTrackMessage *msg, int socket_fd) {
  int Northmsec = (msg->GetTrackNorthTimeInMsec());
  int Eastmsec  = (msg->GetTrackEastTimeInMsec());

  fprintf(stderr, "Sending busy message.\n");
  send_status_message(socket_fd, SCOPE_IO_BUSY);
  fprintf(stderr, "Running guide motor.\n");
  track(Northmsec, Eastmsec);
  fprintf(stderr, "Sending Ready message.\n");
  send_status_message(socket_fd, SCOPE_IDLE);
}

void read_variable_string(char *buffer, int sizeof_buffer) {
  // This is a variable-length response.  Terminated by a "#".
  int count;
  char one_char;
  int retval;

  for(count=0; count < sizeof_buffer; count++) {
    retval = read_mount(lx200_fd, &one_char, 1);
    if(retval == 0) {
      // not sure what a return value of 0 really means, but it happens.
      usleep(50000);
      retval = read_mount(lx200_fd, &one_char, 1);
    }
    if(retval != 1) {
      perror("scope_message_handler: string response error");
      fprintf(stderr, "...read() returned %d\n", retval);
      break;
    }
    *buffer++ = one_char;
    *buffer = 0;
    // This is the string-termination character for all
    // variable-length LX200 strings.
    if(one_char == '#') break;
  }
  if (count >= sizeof_buffer) {
    fprintf(stderr, "WARNING: read_variable_string failed to read '#'\n");
    fprintf(stderr, "...instead, read: %s\n", buffer);
  }
}
  

void send_scope_query(void) {
  const char *query_string = "\006";

  if(write_mount(lx200_fd, query_string, 1) != 1) {
    perror("scope SendQuery: unable to send ACK message");
  }
}

void flush_scope_data(void) {
  fd_set scope_fd_set;
  struct timeval timeout_interval;
  char buffer[32];
  char *buffer_ptr = buffer;
    
  FD_ZERO(&scope_fd_set);
  FD_SET(lx200_fd, &scope_fd_set);

  timeout_interval.tv_sec = 4;
  timeout_interval.tv_usec = 0;

#if DEBUG1
  fprintf(stderr, "    (waiting %ld seconds for response)\n",
	  timeout_interval.tv_sec);
#endif
  do {
    int retval = select(lx200_fd+1,
			&scope_fd_set,
			0, 0,
			&timeout_interval);

    if(retval == 0) break;

    if(retval < 0) {
      // This is an error.  Probably means we have an error in this
      // code somewhere.
      perror("scope message: select failed");
      break;
    }

    // Read the telescope's response
    retval = read_mount(lx200_fd, buffer_ptr, 1);
    if(retval > 0) {
      buffer_ptr += retval;
      if (buffer_ptr >= buffer + sizeof(buffer)) {
	fprintf(stderr, "WARNING: flush_buffer overflow. Dumping buffer.\n");
	buffer_ptr = buffer;
      }
    }
    FD_ZERO(&scope_fd_set);
    FD_SET(lx200_fd, &scope_fd_set);

    timeout_interval.tv_sec = 0;
    timeout_interval.tv_usec = 0;

  } while(1);

  *buffer_ptr = 0;

#if DEBUG1
  fprintf(stderr, "scope response = '%s'\n", buffer);
#endif
  
}

/****************************************************************/
/*        resync_interface()					*/
/****************************************************************/
void handle_resync_message(int socket_fd) {
  send_scope_query();
  flush_scope_data();
  send_scope_query();
  flush_scope_data();

  send_status_message(socket_fd, SCOPE_IDLE);
}

/****************************************************************/
/*        handle_scope_message()				*/
/*    Handle inbound general-purpose messages.			*/
/****************************************************************/
void handle_scope_message(lxScopeMessage *msg, int socket_fd) {
  /* first part is easy: send the command to the scope */
  ScopeResponseStatus Status = Okay;  // default
  char buffer[36];		// response from LX200 goes here

  const int message_size = strlen(msg->GetMessageString());

  buffer[0] = 0;		// default response message is nil

  // Send the user's message to the telescope
#if DEBUG1
  fprintf(stderr, "sending string to scope: '%s'\n", msg->GetMessageString());
#endif
  if(write_mount(lx200_fd,
		 msg->GetMessageString(),
		 message_size) != message_size) {
    perror("scope Message(): unable to send scope message");
  }

  /* now do a select() with a timeout to either get a response from
     the telescope or to get an error timeout. */
#if DEBUG1
  fprintf(stderr, "   (expected response = %d)\n", msg->GetResponseType());
#endif
  if(msg->GetResponseType() != Nothing) {
    fd_set scope_fd_set;
    struct timeval timeout_interval;
    
    FD_ZERO(&scope_fd_set);
    FD_SET(lx200_fd, &scope_fd_set);

    switch (msg->GetExecutionTime()) {
    case RunFast:		// RunFast = 1 second
      timeout_interval.tv_sec = 1;
      timeout_interval.tv_usec = 0;
      break;

    case RunMedium:		// RunMedium = 1 second
      timeout_interval.tv_sec = 5;
      timeout_interval.tv_usec = 0;
      break;

    case RunSlow:		// RunSlow = 20 seconds
      timeout_interval.tv_sec = 20;
      timeout_interval.tv_usec = 0;
      break;
    }

#if DEBUG1
    fprintf(stderr, "    (waiting %ld seconds for response)\n",
	    timeout_interval.tv_sec);
#endif
    int retval = select(lx200_fd+1,
			&scope_fd_set,
			0, 0,
			&timeout_interval);

    if(retval < 0) {
      // This is an error.  Probably means we have an error in this
      // code somewhere.
      perror("scope message: select failed");
      Status = Aborted;
    } else if(retval == 0) {
      // Timeout.  Probably a telescope problem.
      fprintf(stderr, "    (result is <timeout>)\n");
      Status = TimeOut;
    } else {
      // Read the telescope's response
      if(msg->GetResponseType() == FixedLength) {
	
	if(msg->GetResponseCharCount() >= sizeof(buffer)) {
	  fprintf(stderr,
		  "scope_message_handler: buffer too small (%lu vs. %lu)\n",
		  (unsigned long) msg->GetResponseCharCount(), 
		  (unsigned long) sizeof(buffer));
	  Status = Aborted;
	} else {
	  retval = read_mount(lx200_fd, buffer,
			msg->GetResponseCharCount());
	  if(retval != msg->GetResponseCharCount()) {
	    perror("Error reading response from LX200");
	    Status = Aborted;
	  }
	  buffer[retval] = 0;
#if DEBUG1
	  fprintf(stderr, "scope response = '%s'\n", buffer);
#endif
	}
      } else if(msg->GetResponseType() == MixedModeResponse) {
	char first_char;
	retval = read_mount(lx200_fd, &first_char, 1);
	if(retval != 1) {
	  perror("scope_message_handler: first_char response error");
	  buffer[0] = 0;
	} else {

	  buffer[0] = first_char;
	  // see if this character is in the list of valid
	  // single-character responses that came with the message from
	  // the user. If it is, we're done. If this char is something
	  // else, then we read a string (terminated by a "#").
	  char single_char_choices[32];
	  msg->GetSingleCharacterResponses(single_char_choices);
	  int character_was_found = 0; // flag
	  for(char *one_choice = single_char_choices;
	      *one_choice;
	      one_choice++) {
	    if(first_char == *one_choice) {
	      // Yes! Matches. This means this is the only character we
	      // will receive.
	      buffer[1] = 0;
	      character_was_found = 1;
	      break;
	    }
	  }
	  if(!character_was_found) {
	    read_variable_string(buffer+1, sizeof(buffer) - 1);
	  }
	}
      } else if(msg->GetResponseType() == StringResponse) {
	// This is a variable-length response.  Terminated by a "#".
#if DEBUG1
	fprintf(stderr, "    (reading string response.)\n");
#endif
	read_variable_string(buffer, sizeof(buffer));
      }
#if DEBUG1
      fprintf(stderr, "scope response = '%s'\n", buffer);
#endif
    }
  }
  // Remember, by the way, that the default status was set to Okay at
  // the very beginning.
  {
    lxScopeResponseMessage *outbound =
      new lxScopeResponseMessage(socket_fd,
				 buffer,
				 Status);
    outbound->send();
  }
}

/****************************************************************/
/*        handle_message					*/
/*    Inbound "case" statement handles all inbound messages     */
/*  and calls individual "handle_xxx_message()" routines.       */
/****************************************************************/
int handle_message(int socket_fd) {
  lxGenMessage *new_message = lxGenMessage::ReceiveMessage(socket_fd);

  if(new_message == 0) return -1;

  switch(new_message->MessageID()) {
  case lxRequestStatusMessageID:
    // Someone wants status. We're always ready to provide status!
    send_status_message(socket_fd, SCOPE_IDLE);
    break;

  case lxFocusMessageID:
    {
      lxFocusMessage *focus_message = new lxFocusMessage(new_message);
      handle_focus_message(focus_message, socket_fd);
      delete focus_message;
    }
    break;

  case lxScopeMessageID:
    {
      lxScopeMessage *scope_message = new lxScopeMessage(new_message);
      handle_scope_message(scope_message, socket_fd);
      delete scope_message;
    }
    break;

  case lxResyncMessageID:
    {
      lxResyncMessage *resync_message = new lxResyncMessage(new_message);
      handle_resync_message(socket_fd);
      delete resync_message;
    }
    break;

  case lxFlatLightMessageID:
    {
      lxFlatLightMessage *flatlight_message = new lxFlatLightMessage(new_message);
      handle_flatlight_message(flatlight_message, socket_fd);
      delete flatlight_message;
    }
    break;

  case lxTrackMessageID:
    {
      lxTrackMessage *track_message = new lxTrackMessage(new_message);
      handle_track_message(track_message, socket_fd);
      delete track_message;
    }
    break;

  case lxStatusMessageID:
  default:
    // We aren't allowed to receive a status message, only to
    // originate one.
    fprintf(stderr, "scope_server: bad inbound message type: 0x%x\n",
	    new_message->MessageID());
  }

  // All done with the message. Kill it!
  delete new_message;
  return 0;			// zero means success
}
