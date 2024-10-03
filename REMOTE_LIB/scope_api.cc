/*  scope_api.cc -- User's view of what the mount can do
 *
 *  Copyright (C) 2007, 2018 Mark J. Munkacsy

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
#include <netdb.h>		// gethostbyname()
#include <netinet/in.h>
#include <string.h>		// memset()
#include <stdlib.h>		// exit()
#include <stdio.h>
#include <unistd.h>		// exit()
#include <sys/socket.h>		// socket()
#include <arpa/inet.h>		// inet_ntoa()
#include <assert.h>
#include <list>
#include "lx_FocusMessage.h"
#include "lx_StatusMessage.h"
#include "lx_gen_message.h"
#include "lx_ScopeMessage.h"
#include "lx_ScopeResponseMessage.h"
#include "lx_TrackMessage.h"
#include "lx_ResyncMessage.h"
#include "lx_FlatLightMessage.h"
#include "ports.h"
#include "mount_model.h"
#include "scope_api.h"

static int comm_socket;		// file descriptor of socket with
				// scope server

static int comm_initialized = 0;
static long cum_focus_position_C14 = 0;
static long cum_focus_position_Esatto = 0;


const static char *mount_status_text[] = {
  "Tracking",			// 0
  "Stopped",			// 1
  "Slewing",			// 2
  "Unparking",			// 3
  "Slewing to home",		// 4
  "Parked",			// 5
  "Slewing",			// 6
  "Tracking off",		// 7
  "Low-temp inhibit",		// 8
  "Outside limits",		// 9
  "Satellite tracking",		// 10
  "User intervention needed",	// 11
};
#define num_status_texts (sizeof(mount_status_text)/sizeof(char *))

const char *MountStatusText(int status) {
  if (status < 0) return "<negative>";
  if (status == 98) return "<unknown>";
  if (status == 99) return "<error>";
  if (status >= (int) num_status_texts) return "<invalid>";
  return mount_status_text[status];
}
  
// Forward declaration
bool CheckForStuckInLimit(void);

void initialize_mount() {
  char response[64];
  ScopeResponseStatus Status;
#if defined GEMINI
  // find out which "mode" it is in: "long" or "short"

  if(scope_message(":GR#",	// get right ascension
		   RunFast,
		   StringResponse,
		   response,
		   0,
		   &Status)) {
    fprintf(stderr, "Cannot communicate with scope.\n");
    return;
  }

  // determine current mode by counting ":" characters. If one, means
  // we are in "short" mode. If two, means we are in "long".
  int colon_count = 0;
  char *s;

  for(s=response; *s; s++) {
    if(*s == ':') colon_count++;
  }

  if(colon_count == 2) {
    // perfect: do nothing
    ;
  } else if(colon_count == 1) {
    if(scope_message(":U#",	// change mode to "long"
		     RunFast,
		     Nothing,
		     response,
		     0,
		     &Status)) {
      fprintf(stderr, "Cannot set scope mode to 'long'.\n");
    }
  } else {
    fprintf(stderr, "Weird response to RA command: '%s'\n", response);
  }
  if(scope_message(":p0#", // no precession or refraction adjustments
		   RunFast,
		   Nothing,
		   response,
		   0,
		   &Status)) {
    fprintf(stderr, "Cannot disable precession/refraction.\n");
  }
#elif defined GM2000
  if (scope_message(":U2#",	// ultra precision
		    RunFast,
		    Nothing,
		    response,
		    0,
		    &Status)) {
    fprintf(stderr, "Cannot set mount precision to 'ultra'.\n");
  }
  if (scope_message(":CMCFG0#",	// ultra precision
		    RunFast,
		    StringResponse,
		    response,
		    sizeof(response),
		    &Status)) {
    fprintf(stderr, "Cannot set mount precision to 'ultra'.\n");
  }
  
#endif
}

// connect_to_scope() will establish a connection to the scope server
// process running on the scope computer.  It will block for as long
// as necessary to establish the connection. If unable to establish a
// connection (for whatever reason), it will print an error message to
// stderr and will exit.
void connect_to_scope(void) {
  struct hostent *jellybean;
  struct sockaddr_in my_address;

  memset(&my_address, 0, sizeof(my_address));
  jellybean = gethostbyname(SCOPE_HOST);
  if(jellybean == 0) {
    herror("Cannot lookup jellybean host name:");
    exit(2);
  } else {
    my_address.sin_addr = *((struct in_addr *)(jellybean->h_addr_list[0]));
    my_address.sin_port = htons(SCOPE_PORT); // port number
    my_address.sin_family = AF_INET;
    fprintf(stderr, "Connecting to scope @ %s\n",
	    inet_ntoa(my_address.sin_addr));
  }

  comm_socket = socket(PF_INET, SOCK_STREAM, 0);
  if(comm_socket < 0) {
    perror("Error creating scope socket");
    exit(2);
  }

  if(connect(comm_socket,
	     (struct sockaddr *) &my_address,
	     sizeof(my_address)) < 0) {
    perror("Error connecting to scope socket");
    exit(2);
  }
  comm_initialized = 1;
  initialize_mount();
}

void disconnect_scope(void) {
  ; // noop for native interface
}

void disconnect_focuser(void) {
  ; // noop for native interface
}

void connect_to_focuser(void) {
  connect_to_scope();
}


// Will try to resynchronize the interface with the scope controller
int ResyncInterface(void) {
  lxResyncMessage *message;
  lxGenMessage *inbound_message;
  lxStatusMessage *status;
  int finished_ok = 0;

  fprintf(stderr, "Initiating resync of scope interface\n");

  message = new lxResyncMessage(comm_socket);
  message->send();

repeat:
  // Receive the message from the socket
  inbound_message = lxGenMessage::ReceiveMessage(comm_socket);
  switch(inbound_message->MessageID()) {

    /********************/
    /*  StatusMessage   */
    /********************/
  case lxStatusMessageID:
    status = (lxStatusMessage *) inbound_message;
    if(status->GetScopeStatus() != SCOPE_IDLE) {
      delete inbound_message;
      goto repeat;
    } else {
      cum_focus_position_C14 = status->GetFocusPositionC14();
      cum_focus_position_Esatto = status->GetFocusPositionEsatto();
      finished_ok = 1;
    }
    break;

    /*************************/
    /*  RequestStatusMessage */
    /*************************/
  case lxFocusMessageID:
  case lxRequestStatusMessageID:
  default:
    // Makes absolutely no sense for us to receive these.
    fprintf(stderr, "Illegal message received by scope_api().\n");
    break;

  }
  delete inbound_message;
  delete message;

  return finished_ok;
}

// set turn_off to 1 to disable tracking at the sidereal rate and stop
// the RA motor. set turn_off to 0 to resume tracking.
void ControlTrackingMotor(int turn_off) {
  if(!comm_initialized) {
    fprintf(stderr, "scope_api: comm link never initialized.\n");
    exit(2);
  }

#if defined GEMINI
  const char *message_string = (turn_off ? ":hN#" : ":hW#");
#elif defined GM2000
  const char *message_string = (turn_off ? ":RT9#" : ":RT2#");
#endif
  
  ScopeResponseStatus Status;
  char response[64];

  scope_message(message_string,
		RunFast,
		Nothing,
		response,		// response string
		0,	        	// response length
		&Status);

  return;
}

// scope_focus() will run the focus motor (slow speed) for the
// indicated number of msec.  Positive values move one way and
// negative numbers move the other. The function will block as long as
// needed until the focus motor stops. The total focus position will
// be returned.
long scope_focus(long msec,
		 FocuserMoveType move_type,
		 FocuserName focuser_name) {
  lxFocusMessage *message;
  lxGenMessage *inbound_message;
  lxStatusMessage *status;

  int flags = 0;

  flags |= (move_type == FOCUSER_MOVE_ABSOLUTE ?
	    FOCUS_FLAG_ABSOLUTE : FOCUS_FLAG_RELATIVE);
  flags |= (focuser_name == FOCUSER_COARSE ?
	    FOCUS_FLAG_C14 : FOCUS_FLAG_ESATTO);

  if(!comm_initialized) {
    fprintf(stderr, "scope_api: comm link never initialized.\n");
    exit(2);
  }
  message = new lxFocusMessage(comm_socket,
			       flags, // flags
			       msec);
  message->send();

  // That's the easy part.  Now wait for a response.
  
repeat:
  // Receive the message from the socket
  inbound_message = lxGenMessage::ReceiveMessage(comm_socket);
  switch(inbound_message->MessageID()) {

    /********************/
    /*  StatusMessage   */
    /********************/
  case lxStatusMessageID:
    status = (lxStatusMessage *) inbound_message;
    if(status->GetScopeStatus() != SCOPE_IDLE) {
      delete inbound_message;
      goto repeat;
    }
    cum_focus_position_C14 = status->GetFocusPositionC14();
    cum_focus_position_Esatto = status->GetFocusPositionEsatto();
    break;

    /*************************/
    /*  RequestStatusMessage */
    /*************************/
  case lxFocusMessageID:
  case lxRequestStatusMessageID:
  default:
    // Makes absolutely no sense for us to receive these.
    fprintf(stderr, "Illegal message received by scope_api().\n");
    break;

  }
  delete inbound_message;
  delete message;

  return (focuser_name == FOCUSER_COARSE ?
	  cum_focus_position_C14 :
	  cum_focus_position_Esatto);
}

// provides the current position of the telescope focus (in net msec).
long CumFocusPosition(FocuserName focuser_name) {
  return (focuser_name == FOCUSER_COARSE ?
	  cum_focus_position_C14 :
	  cum_focus_position_Esatto);
}

int scope_message(const char *command_string,
		  ExecutionChoices timeout,
		  ResponseTypeChoices responseType,
		  char *response_string,
		  int response_length,
		  ScopeResponseStatus *responseStatus,
		  const char *SingleCharResponses) {
  int api_response = 0;		// assume OK to start with
  lxScopeResponseMessage *status;

  if(!comm_initialized) {
    fprintf(stderr, "scope_api: comm link never initialized.\n");
    exit(2);
  }
  lxScopeMessage *outbound_command =
    new lxScopeMessage(comm_socket,
		       command_string,
		       timeout,
		       responseType,
		       response_length,
		       SingleCharResponses);
  outbound_command->send();
  delete outbound_command;

  // That's the easy part.  Now wait for a response.
  
  // Receive the message from the socket
  lxGenMessage *inbound_message = lxGenMessage::ReceiveMessage(comm_socket);
  switch(inbound_message->MessageID()) {

    /********************/
    /*  StatusMessage   */
    /********************/
  case lxScopeResponseMessageID:
    status = (lxScopeResponseMessage *) inbound_message;
    strcpy(response_string, status->GetMessageString());
    *responseStatus = status->GetStatus();

    if(*responseStatus != Okay) {
      fprintf(stderr, "scope_api: message = %s\n", response_string);
      api_response = -1;
    }
    break;

    /*************************/
    /*  RequestStatusMessage */
    /*************************/
  case lxFocusMessageID:
  case lxRequestStatusMessageID:
  default:
    // Makes absolutely no sense for us to receive these.
    fprintf(stderr, "Illegal message received by scope_api().\n");
    break;

  } // end of switch on message type
  delete inbound_message;

  return api_response;
}

bool dec_flip_assumed;		// whether last goto assumed a flip
DEC_RA desired_catalog_goto_location;

/****************************************************************/
/*        DumpCurrentLimits() RA Limits				*/
/****************************************************************/
void DumpCurrentLimits(void) {
  char MI250response[32];
  ScopeResponseStatus Status;

#if defined GEMINI
  char buffer[32];
  int east_limit_ticks;
  int west_limit_ticks;
  int current_ra_ticks;
  int current_dec_ticks;

  // Use command 230 to fetch the current east and west limits.
  BuildMI250Command(buffer, MI250_GET, 230);
  if(scope_message(buffer,
		   RunMedium,
		   StringResponse,
		   MI250response,
		   sizeof(MI250response),
		   &Status)) {
    fprintf(stderr, "Get Safety Limits responnse error: Status = %d\n",
	    Status);
    return;
  }
  if (sscanf(MI250response, "%d;%d",
	     &east_limit_ticks, &west_limit_ticks) != 2) {
    fprintf(stderr, "Cannot extract east/west limits: %s\n",
	    MI250response);
    return;
  }

  // Then use command 235 to fetch the current RA axis location (also
  // provides dec axis location, but we will ignore it).
  BuildMI250Command(buffer, MI250_GET, 235);
  if(scope_message(buffer,
		   RunMedium,
		   StringResponse,
		   MI250response,
		   sizeof(MI250response),
		   &Status)) {
    fprintf(stderr, "Get current axis location responnse error: Status = %d\n",
	    Status);
    return;
  }
  if (sscanf(MI250response, "%d;%d",
	     &current_ra_ticks, &current_dec_ticks) != 2) {
    fprintf(stderr, "Cannot extract dec/ra ticks: %s\n",
	    MI250response);
    return;
  }
  fprintf(stderr, "RA axis limit/limit, current = %d/%d, %d\n",
	  east_limit_ticks, west_limit_ticks, current_ra_ticks);
#elif defined GM2000
  if(scope_message(":GaXa#", // Get RA axis angular position
		   RunMedium,
		   StringResponse,
		   MI250response,
		   sizeof(MI250response),
		   &Status)) {
    fprintf(stderr, "Get RA axis angular position error: Status = %d\n",
	    Status);
    return;
  }
  fprintf(stderr, "RA axis location: %s\n", MI250response);
#endif
}

/****************************************************************/
/*        SetTargetPosition					*/
/*  This loads the specified position into the mount as the     */
/*  "current target".                                           */
/****************************************************************/
int SetTargetPosition(DEC_RA *CatalogLocation) {
  // Scope needs hh:mm:ss for RA
  char RAstring[32];
  char DECstring[32];
  char LX200response[32];
  ScopeResponseStatus Status;
  JULIAN RightNow(time(0));
  desired_catalog_goto_location = *CatalogLocation;

  DEC_RA Location = (*CatalogLocation);
#if defined JELLYBEAN
  ; // nothing else
#else

  //****************
  // PRECESSION
  //****************
#if defined INTERNAL_PRECESSION
  EPOCH j2000(2000);
  EPOCH epoch_now(RightNow);

  Location = ToEpoch(Location, j2000, epoch_now);
#endif

  //****************
  // MOUNT MODEL
  //****************
#if defined INTERNAL_MOUNT_MODEL
  double ha = CatalogLocation->hour_angle(RightNow);
  DEC_RA orig_request(Location);
  Location = mount_coords(Location, RightNow);
  fprintf(stderr, "Desired loc = %s, %s\n",
	  orig_request.string_dec_of(),
	  orig_request.string_ra_of());
  fprintf(stderr, "Mount raw loc will be = %s, %s\n",
	  Location.string_dec_of(),
	  Location.string_ra_of());
  dec_flip_assumed = dec_axis_likely_flipped(ha);
#endif
#endif

  int in_retry = 0;
  
repeat:

  sprintf(RAstring, ":Sr%s#", Location.string_ra_of());

  sprintf(DECstring, ":Sd%s#", Location.string_longdec_of());

  if(scope_message(RAstring,
		   RunFast,
		   FixedLength,
		   LX200response,
		   1,
		   &Status)) {
    return -1;			// error return
  } else {
    if(LX200response[0] != '1') {
      fprintf(stderr, "Error response to set RA: %s\n", RAstring);
      ResyncInterface();
      if(!in_retry) {
	in_retry = 1;
	goto repeat;
      }
    }
  }

  if(scope_message(DECstring,
		   RunFast,
		   FixedLength,
		   LX200response,
		   1,
		   &Status)) {
    return -1;			// error return
  } else {
    if(LX200response[0] != '1') {
      fprintf(stderr, "Error response to set Dec: %s\n", DECstring);
      ResyncInterface();
      if(!in_retry) {
	in_retry = 1;
	goto repeat;
      }
    }
  }
  return 0;
}


/****************************************************************/
/*        Telescope Motion					*/
/****************************************************************/
int MoveTo(DEC_RA *CatalogLocation, int encourage_flip) {
  // Scope needs hh:mm:ss for RA
  char LX200response[32];
  ScopeResponseStatus Status;

  if(!comm_initialized) {
    fprintf(stderr, "scope_api: comm link never initialized.\n");
    exit(2);
  }

#ifdef GEMINI
  DumpCurrentLimits();
  CheckForStuckInLimit(); // check and free if stuck
#endif

  if (SetTargetPosition(CatalogLocation)) {
    return -1;
  }
  
#if defined GEMINI
  // before we start, find out which side of the pier the telescope is
  // on. This becomes significant later if we are going to encourage a
  // flip.
  const int scope_on_west = scope_on_west_side_of_pier();
  // if a flip is encouraged and the scope is already on the east side
  // of the mount, do *not* suggest that the mount flip, since that
  // would put the scope on the wrong side.
  if(scope_on_west == 0) encourage_flip = 0;

#endif // GEMINI

  // Now execute the "goto". Use the :MM command if we are encouraged
  // to do a meridian flip as part of the goto, otherwise use the :MS
  // command.
#if defined GM2000
  encourage_flip = 0;
#endif
  
  int in_retry = 0;
 repeat:
  if(scope_message((encourage_flip ? ":MM#" : ":MS#"),
		   RunSlow,
		   MixedModeResponse,
		   LX200response,
		   1,
		   &Status,
		   "0")) {
    return -1;
  } else {
    if(LX200response[0] != '0') {
      fprintf(stderr, "Error response to GOTO command. (%d)\n",
	      LX200response[0]);
      ResyncInterface();
      if(!in_retry) {
	in_retry = 1;
	goto repeat;
      }
    }
  }
  return 0;
}

// returns True if stuck
bool CheckForStuckInLimit(void) {
  char buffer[32];
  char MI250response[32];
  ScopeResponseStatus Status;

#if defined GEMINI
  BuildMI250Command(buffer, MI250_GET, 99);
#elif defined GM2000
  sprintf(buffer, ":Gstat#");
#endif
  if(scope_message(buffer,
		   RunMedium,
		   StringResponse,
		   MI250response,
		   sizeof(MI250response),
		   &Status)) {
    fprintf(stderr, "Status query response error: Status = %d\n", Status);
    fprintf(stderr, "     (%s)\n", MountStatusText(Status));
    return false;
  }

  int response_int;
  if(sscanf(MI250response, "%d", &response_int)  == 1) {
#if defined GEMINI
    if((response_int & 0x10)) {
#elif defined GM2000
    if(response_int == 9) {
#endif
      // Yes!! Stuck!!
      fprintf(stderr, "WARNING: Mount stuck against RA limit!!\n");
      fprintf(stderr, "... attempting to get off limit with East move.\n");
      //SmallMove(10.0, 0.0); // 10 arcmin east
      if (scope_message(":Me#",
			RunSlow,
			Nothing,
			MI250response,
			sizeof(MI250response),
			&Status)) {
	fprintf(stderr, "Unstick: 'Me' command error.\n");
	return false;
      }
      sleep(6);
      if (scope_message(":Q#",
			RunSlow,
			Nothing,
			MI250response,
			sizeof(MI250response),
			&Status)) {
	fprintf(stderr, "Unstick: 'Q' command error.\n");
	return false;
      }      
      return true;
    }
  }
  return false; // all is okay
}

void WaitForGoToDoneRaw(void) {
  do {
    if (SlewInProgress() == false) break; // slew done
    else {
      fprintf(stderr, ".");
    }
    sleep(2);
  } while(1);
  fprintf(stderr, "\n");
  //DumpCurrentLimits();
}

void WaitForGoToDone(void) {
  // wait for scope to stop moving
  WaitForGoToDoneRaw();
#if defined INTERNAL_MOUNT_MODEL
  if (dec_axis_is_flipped() != dec_flip_assumed) {
    // need to recalculate corrections
    DEC_RA corrected_location = mount_coords(desired_catalog_goto_location,
					     JULIAN(time(0)),
					     dec_axis_is_flipped());
    fprintf(stderr, "Adjusting goto location due to meridian flip.\n");
    MoveTo(&corrected_location, 0);
    WaitForGoToDoneRaw();
  }
#endif
  sleep(5);
}
  
//****************************************************************
//        GetMountStatus()
// The integer that this returns is the mount's status word. Depending
// on whether this is a Gemini or a GM2000, the flags in that status
// word will have different definitions.
//****************************************************************

int
GetMountStatus(void) {
  char buffer[32]; // outgoing message
  char MI250response[32];
  ScopeResponseStatus Status;

#if defined GEMINI
  BuildMI250Command(buffer, MI250_GET, 99);
#elif defined GM2000
  sprintf(buffer, ":Gstat#");
#else
#error NEITHER GEMINI nor GM2000 DEFINED
#endif
  if(scope_message(buffer,
		   RunMedium,
		   StringResponse,
		   MI250response,
		   sizeof(MI250response),
		   &Status)) {
    fprintf(stderr, "Status query response error: Status = %d\n", Status);
    fprintf(stderr, "     (%s)\n", MountStatusText(Status));
    return false;
  }

  int response_int;
  if(sscanf(MI250response, "%d", &response_int)  == 1) {
    return response_int;
  } else {
    fprintf(stderr, "Error parsing response to Status query: %s\n",
	    MI250response);
  }
  return -1;
}
 
//****************************************************************
//        SlewInProgress()
// Here's a non-blocking way to test whether a slew is still
// active. Works for both MoveTo() and SmallMove().
//****************************************************************

bool SlewInProgress(void) {
  const int response_int = GetMountStatus();

  if (response_int < 0) { // something went wrong
    if (response_int == -1) {
      ; // message was already printed by GetMountStatus()
    } else {
      fprintf(stderr, "GetMountStatus() returned error (%d).\n", response_int);
      fprintf(stderr, "     (%s)\n", MountStatusText(response_int));
    }
    fprintf(stderr, "Resyncing scope interface.\n");
    ResyncInterface();
    return SlewInProgress();
  }

#if defined GEMINI
  if((response_int & 8) == 0) return true; // slew done
#elif defined GM2000
  if(response_int == 2 ||	// parking
     response_int == 3 ||	// unparking
     response_int == 4 ||	// homing
     response_int == 6) {	// slewing
    return true;
  }
#endif
  return false;
}

//****************************************************************
//        MountGoToFlatLight()
// Blocks for a long time; stops tracking; any other MoveTo() will
// undo this. 
//****************************************************************
void MountGoToFlatLight(void) {
#if 0
  ScopeResponseStatus Status;
  char response_string[16];

  fprintf(stderr, "Sending GoTo [flatlight] command.\n");

  if(scope_message(":Sa+00*07:30#",
		   RunFast,
		   Nothing,
		   response_string,
		   0,
		   &Status)) {
    fprintf(stderr, "Error trying to send :Sa command.\n");
    return;
  }

  if(scope_message(":Sz180*00#",
		   RunFast,
		   Nothing,
		   response_string,
		   0,
		   &Status)) {
    fprintf(stderr, "Error trying to send :Sz command.\n");
    return;
  }

  if(scope_message(":MA#",
		   RunSlow,
		   MixedModeResponse,
		   response_string,
		   1,
		   &Status)) {
    fprintf(stderr, "Error trying to send :MA command.\n");
    return;
  } else {
    if(response_string[0] != '0') {
      response_string[15]=0;
      fprintf(stderr, "Error response to :MA Goto command (%d): %s\n",
	      response_string[0], response_string+1);
      //ResyncInterface();
    }
    return;
  }
  WaitForGoToDoneRaw();
  PerformMeridianFlip();
#else
  SetAngularPosition(0.0, /*-135.6674*/ -138.0 /*5.0, -138.8*/);
  WaitForGoToDoneRaw();
#endif
}

void MountResumeTracking(void) {
  ScopeResponseStatus Status;
  char response_string[16];

  fprintf(stderr, "Sending resume-tracking command.\n");

  if(scope_message(":AP#",
		   RunFast,
		   Nothing,
		   response_string,
		   0,
		   &Status)) {
    fprintf(stderr, "Error trying to send :AP command.\n");
  }
}  

//****************************************************************
//        ParkTelescope()
//****************************************************************
 
// blocks for a long time
void ParkTelescope(void) {
  ScopeResponseStatus Status;
  char response_string[16];

  fprintf(stderr, "Sending park command.\n");

  if(scope_message(":hP#",
		   RunFast,
		   Nothing,
		   response_string,
		   0,
		   &Status)) {
    fprintf(stderr, "Error trying to send park command.\n");
    return;
  }

#if defined GEMINI
  const int time_to_wait = 45;	// time in seconds

  for(int timer = 0; timer < time_to_wait; timer++) {
    sleep(1);

    if(scope_message(":h?#",
		     RunFast,
		     FixedLength,
		     response_string,
		     1,
		     &Status)) {
      fprintf(stderr, "Error getting status of completing park command.\n");
    } else {
      if(response_string[0] == '1') {
	fprintf(stderr, "Park completed.\n");
	break; // done
      } else if(response_string[0] == '0') {
	fprintf(stderr, "Gemini reports that Park command failed.\n");
	break;
      }
    }
  }

#elif defined GM2000
  const int time_to_wait = 45;	// time in seconds

  for(int timer = 0; timer < time_to_wait; timer++) {
    sleep(1);
    int response_int = GetMountStatus();
    if (response_int == 5) {
      // finished
      fprintf(stderr, "Park completed.\n");
      break;
    } else if (response_int == 2) {
      // still parking
      continue;
    } else {
      fprintf(stderr, "GM2000 reports inproper status to park command: %d\n",
	      response_int);
      fprintf(stderr, "     (%s)\n", MountStatusText(response_int));
      break;
    }
  }
  
#endif
}

//****************************************************************
//        UnParkTelescope()
//****************************************************************
 
// blocks for a long time
void UnParkTelescope(void) {
  ScopeResponseStatus Status;
  char response_string[16];

  fprintf(stderr, "Sending unpark command.\n");

  if(scope_message(":PO#",
		   RunFast,
		   Nothing,
		   response_string,
		   0,
		   &Status)) {
    fprintf(stderr, "Error trying to send unpark command.\n");
    return;
  }

#if defined GEMINI
  const int time_to_wait = 45;	// time in seconds

  for(int timer = 0; timer < time_to_wait; timer++) {
    sleep(1);

    if(scope_message(":h?#",
		     RunFast,
		     FixedLength,
		     response_string,
		     1,
		     &Status)) {
      fprintf(stderr, "Error getting status of completing unpark command.\n");
    } else {
      if(response_string[0] == '1') {
	fprintf(stderr, "Unpark completed.\n");
	break; // done
      } else if(response_string[0] == '0') {
	fprintf(stderr, "Gemini reports that Unpark command failed.\n");
	break;
      }
    }
  }

#elif defined GM2000
  const int time_to_wait = 45;	// time in seconds

  for(int timer = 0; timer < time_to_wait; timer++) {
    sleep(1);
    int response_int = GetMountStatus();
    if (response_int == 0) {
      // finished
      fprintf(stderr, "Unpark completed.\n");
      break;
    } else if (response_int == 3) {
      // still unparking
      continue;
    } else {
      fprintf(stderr, "GM2000 reports inproper status to unpark command: %d\n",
	      response_int);
      fprintf(stderr, "     (%s)\n", MountStatusText(response_int));
      break;
    }
  }
  
#endif
}

//****************************************************************
//        Dec_Axis_Is_Flipped()
// Returns true if the camera is inverted (north/south); also
// indicates that the declination axis is flipped.
//****************************************************************
bool dec_axis_is_flipped(double hour_angle, bool scope_on_west) {
  return !scope_on_west;
#if 0
  double ha = hour_angle;
  bool flipped = false;
  if(ha > M_PI) ha -= (M_PI*2.0);

  // mount's default setup allows travel 20-deg beyond meridian. Since
  // 20-deg ~= 0.35 radians, we will use 0.4 radians in the following
  // tests to allow some comfort.

  if(ha > (M_PI - 0.4) || ha < (-M_PI + 0.4)) {
    if(scope_on_west) {
      flipped = true;
    } else {
      flipped = false;
    }
  } else if(ha < 0.4 && ha > -0.4) {
    if(scope_on_west) {
      flipped = false;
    } else {
      flipped = true;
    }
  } else if(ha < 0.0) {
    flipped = false;
  } else {
    flipped = true;
  }
  return flipped;
#endif
}

bool dec_axis_is_flipped(void) {
#if 0
  const double ha = GetScopeHA();
  const int scope_on_west = scope_on_west_side_of_pier();
  return dec_axis_is_flipped(ha, scope_on_west);
#else
  return !scope_on_west_side_of_pier();
#endif
}

bool dec_axis_likely_flipped(double hour_angle) {
  double ha  = hour_angle;
  if(ha > M_PI) ha -= (M_PI*2.0);

  return (ha >= 0.0);
}
  
// returns 0 on success, -1 if something went wrong.
int SmallMove(double delta_ra_arcmin, double delta_dec_arcmin) {
  // Set centering speed
  ScopeResponseStatus Status;
  char scope_msg[80];
  char message[80];

#if defined GEMINI
  if(scope_message(":RC#",
		   RunFast,
		   Nothing,
		   scope_msg,
		   sizeof(scope_msg),
		   &Status)) {
    fprintf(stderr, "Failed to set centering speed.\n");
    return -1;
  }

  const long ra_ticks = delta_ra_arcmin * (106.0 + 2.0/3.0);
  const long dec_ticks = delta_dec_arcmin * (106.0 + 2.0/3.0) *
    (dec_axis_is_flipped() ? +1 : -1);
  
  if (ra_ticks > 65535 || ra_ticks < -65536 ||
      dec_ticks > 65536 || dec_ticks < -65535) {
    fprintf(stderr, "Error: SmallMove(%lf, %lf): excessive.\n",
	    delta_ra_arcmin, delta_dec_arcmin);
    return -1;
  }

  char msg_response[80];
  sprintf(message, ":mi%ld;%ld#", ra_ticks, dec_ticks);
  if (scope_message(message,
		    RunFast,
		    Nothing,
		    msg_response,
		    sizeof(msg_response),
		    &Status)) {
    fprintf(stderr, "Error sending :mi# command\n");
    return -1;
  }
#elif defined GM2000
  int dir = (dec_axis_is_flipped() ? +1 : -1);

  sprintf(message, ":NUDGE%d,%d#",
	  -(int) (delta_ra_arcmin*60.0 + 0.5),
	  dir * (int) (delta_dec_arcmin*60.0 + 0.5));
  if (scope_message(message,
		    RunFast,
		    MixedModeResponse,
		    scope_msg,
		    sizeof(scope_msg),
		    &Status,
		    "0")) {
    fprintf(stderr, "Error sending :NUDGE command\n");
    return -1;
  }
  if (scope_msg[0] != '0') {
    fprintf(stderr, "Nudge refused by mount: %s\n",
	    scope_msg);
  }
#endif
  WaitForGoToDoneRaw();
  return 0;
}
		   

DEC_RA ScopePointsAt(void) {
  // Get the scope's idea of where it points and run through our model

  DEC_RA raw_position = RawScopePointsAt();
  DEC_RA position = raw_position;

#if defined INTERNAL_MOUNT_MODEL
  bool dec_axis_flip = dec_axis_is_flipped();
  position = true_coords(raw_position, JULIAN(time(0)), dec_axis_flip);
#endif

#if defined INTERNAL_PRECESSION
  EPOCH j2000(2000);
  position = ToEpoch(position, EpochOfToday(), j2000);
#endif

  return position;
}

// Returns Sidereal Time measured in radians (0..2*Pi) corresponding
// to (0..24hrs) 
double GetSiderealTime(void) {
  char string_st[24];
  ScopeResponseStatus Status;

  if(!comm_initialized) {
    fprintf(stderr, "scope_api: comm link never initialized.\n");
    exit(2);
  }

  if(scope_message(":GS#",
		   RunFast,
		   StringResponse,
		   string_st,
		   0,
		   &Status)) {
    fprintf(stderr, "Failed to get sidereal time\n");
    return 0.0;
  }

  double st_in_rads;
  int hours, min;
  double sec;

  sscanf(string_st, "%d:%d:%lf", &hours, &min, &sec);
  st_in_rads = (hours + min/60.0 + sec/3600.0)*M_PI/12.0;

  // fprintf(stderr, "hrs = %d, min = %d, sec = %d, ha(rads) = %lf\n",
  // hours, min, sec, ha_in_rads);
  return st_in_rads;
}

double GetScopeHA(void) {	// scope hour angle (0 == meridian, rads)
  if(!comm_initialized) {
    fprintf(stderr, "scope_api: comm link never initialized.\n");
    exit(2);
  }

  DEC_RA current_ra = RawScopePointsAt();
  double current_st = GetSiderealTime();

  double ha = current_st - current_ra.ra_radians();
  if (ha > M_PI) ha -= M_PI*2;
  if (ha < -M_PI) ha += M_PI*2;

  return ha;
}

ALT_AZ ScopePointsAt_altaz(void) {
  char scope_az[24];
  char scope_el[24];
  ScopeResponseStatus Status;

  if(!comm_initialized) {
    fprintf(stderr, "scope_api: comm link never initialized.\n");
    exit(2);
  }

  if(scope_message(":GA#",
		   RunFast,
		   StringResponse,
		   scope_el,
		   0,
		   &Status)) {
    fprintf(stderr, "Failed to get scope Altitude\n");
    return ALT_AZ(0.0,0.0);
  }

  if(scope_message(":GZ#",
		   RunFast,
		   StringResponse,
		   scope_az,
		   0,
		   &Status)) {
    fprintf(stderr, "Failed to get scope Azimuth\n");
    return ALT_AZ(0.0, 0.0);
  }

  double az_in_rads;
  int deg, min, sec;

  sscanf(scope_az, "%d:%d:%d", &deg, &min, &sec);
  // varies from +PI to -PI
  az_in_rads = (deg + min/60.0 + sec/3600.0)*M_PI/180.0 - M_PI;

  double alt_in_rads;
  double alt_sign = 1;
  if(scope_el[0] == '-') alt_sign = -1;
  sscanf(scope_el+1, "%d:%d:%d", &deg, &min, &sec);
  alt_in_rads = alt_sign * (deg + min/60.0 + sec/3600.0)*M_PI/180.0;

  return ALT_AZ(alt_in_rads, az_in_rads);
}

DEC_RA RawScopePointsAt(void) {
  char scope_ra[32];
  char scope_dec[32];
  ScopeResponseStatus Status;

  if(!comm_initialized) {
    fprintf(stderr, "scope_api: comm link never initialized.\n");
    exit(2);
  }
  if(scope_message(":GR#",
		   RunFast,
		   StringResponse,
		   scope_ra,
		   0,
		   &Status)) {
    fprintf(stderr, "Failed to get scope RA.\n");
    return DEC_RA(0.0, 0.0);
  } else {
    // This reference to "Jnow" is correct for the GM2000, but is
    // mount-specific 
    //fprintf(stderr, "Scope's RA (Jnow) = %s\n", scope_ra);
    ;
  }

  if(scope_message(":GD#",
		   RunFast,
		   StringResponse,
		   scope_dec,
		   0,
		   &Status)) {
    fprintf(stderr, "Failed to get scope DEC.\n");
    return DEC_RA(0.0, 0.0);
  } else {
    // fprintf(stderr, "Scope's DEC (Jnow) =%s\n", scope_dec);
    ;
  }


  // RA can be used with only a change to remove the trailing '#'
  if (strlen(scope_ra) > 32 or strlen(scope_dec) > 32) {
    fprintf(stderr, "Response too long from mount: %s %s\n", scope_ra, scope_dec);
    return DEC_RA(0.0, 0.0);
  }
  scope_ra[strlen(scope_ra)-1] = 0;

  // DEC must be adjusted
  {
    int sign = 1;
    int deg;
    int min;
    float secs;
#ifdef LX200
    char deg_string[3];
#endif
    char declination_string[32];
    int convert_status;

    char *s = scope_dec;
    if(scope_dec[0] == '-') sign = -1;

    // end result of the next conditional block must be assignment to
    // variables deg, min, and secs.
#ifdef LX200
    while(*s && *s != (char) 0337) s++;
    if(*s != (char) 0337) {
      fprintf(stderr, "DEC string has no degree symbol: '%s'.\n", scope_dec);
      return DEC_RA(0.0, 0.0);
    }
    deg_string[0] = *(s-2);
    deg_string[1] = *(s-1);
    deg_string[2] = 0;

    sscanf(deg_string, "%d", &deg);
    sscanf(s+1, "%d:%d", &min, &secs);
#else
#if defined GEMINI or defined GM2000
    int fields_read = sscanf(s+1, "%d:%d:%f", &deg, &min, &secs);
    if (fields_read != 3) {
      fprintf(stderr, "RawScopePointsAt: invalid scope string: %s\n",
	      s+1);
      return DEC_RA(0.0, 0.0);
    }
#else
#error "Neither LX200 nor GEMINI defined"
#endif
#endif

    if (deg < 0 or deg > 360 or min < 0 or min > 60 or secs > 61.0) {
      fprintf(stderr, "RawScopePointsAt: invalid dec: %d, %lf\n",
	      deg, min+((double)secs)/60.0);
      return DEC_RA(0.0, 0.0);
    } 
    sprintf(declination_string, "%s%02d:%09.6lf",
	    (sign == -1 ? "-" : ""), deg, min + ((double)secs)/60.0);

    //fprintf(stderr, "Calling DEC_RA with (%s, %s)\n",
    //	    declination_string, scope_ra);
    return DEC_RA(declination_string, scope_ra, convert_status);
  }
}

// int Tweak(double North_arc_seconds, double East_arc_seconds);

// int Tweak(DEC_RA *DeltaLocation);

// specifies time to guide in seconds
void guide(double NorthSeconds, double EastSeconds) {
  lxTrackMessage *message;
  lxGenMessage *inbound_message;
  lxStatusMessage *status;

  if(!comm_initialized) {
    fprintf(stderr, "scope_api: comm link never initialized.\n");
    exit(2);
  }
  message = new lxTrackMessage(comm_socket,
			       (int) (NorthSeconds * 1000.0 + 0.5),
			       (int) (EastSeconds * 1000.0 + 0.5));
  message->send();

  // that's the easy part. Now wait for a response

repeat:
  // Receive the message from the socket
  inbound_message = lxGenMessage::ReceiveMessage(comm_socket);
  switch(inbound_message->MessageID()) {

    /********************/
    /*  StatusMessage   */
    /********************/
  case lxStatusMessageID:
    status = (lxStatusMessage *) inbound_message;
    if(status->GetScopeStatus() != SCOPE_IDLE) {
      delete inbound_message;
      goto repeat;
    }

    cum_focus_position_C14 = status->GetFocusPositionC14();
    cum_focus_position_Esatto = status->GetFocusPositionEsatto();
    break;

    /*************************/
    /*  RequestStatusMessage */
    /*************************/
  case lxFocusMessageID:
  case lxRequestStatusMessageID:
  default:
    // Makes absolutely no sense for us to receive these.
    fprintf(stderr, "Illegal message received by scope_api().\n");
    break;

  }
  delete inbound_message;
  delete message;
}
    


/****************************************************************/
/*        Telescope Position Sync				*/
/****************************************************************/
int scope_sync(DEC_RA *Location) {
  // Scope needs hh:mm:ss for RA
  char RAstring[32];
  char DECstring[32];
  char LX200response[64];
  ScopeResponseStatus Status;

  if(!comm_initialized) {
    fprintf(stderr, "scope_api: comm link never initialized.\n");
    exit(2);
  }
  sprintf(RAstring, ":Sr%s#", Location->string_ra_of());

  sprintf(DECstring, ":Sd%s#", Location->string_longdec_of());

  // Set the Right Ascension
  if(scope_message(RAstring,
		   RunFast,
		   FixedLength,
		   LX200response,
		   1,
		   &Status)) {
    return -1;			// error return
  } else {
    if(LX200response[0] != '1') {
      fprintf(stderr, "Error response to set RA: %s\n", RAstring);
      return -1;
    }
  }

  // Set the Declination
  if(scope_message(DECstring,
		   RunFast,
		   FixedLength,
		   LX200response,
		   1,
		   &Status)) {
    return -1;			// error return
  } else {
    if(LX200response[0] != '1') {
      fprintf(stderr, "Error response to set Dec: %s\n", DECstring);
      return -1;
    }
  }

  // Now execute the "sync"
  if(scope_message(":CM#",
		   RunSlow,
		   StringResponse,
		   LX200response,
		   0,
		   &Status)) {
    return -1;
  } else {
    fprintf(stderr, "SYNC response = '%s'\n", LX200response);
  }

  return 0;
}
/****************************************************************/
/*        scope_on_west_side_of_pier()				*/
/****************************************************************/
// Returns 1 if the scope is on the west side of the pier. If on the
// east side of the pier, will return 0. 
int
scope_on_west_side_of_pier(void) {
  char LX200response[32];
  ScopeResponseStatus Status;
#if defined GEMINI
  const bool is_gemini = true;
#elif defined GM2000
  const bool is_gemini = false;
#endif

  if(scope_message((is_gemini ? ":Gm#" : ":pS#"),
		   RunFast,
		   StringResponse,
		   LX200response,
		   sizeof(LX200response),
		   &Status)) {
    fprintf(stderr, "Query for scope pier side: no scope response\n");
    ResyncInterface();
    return 0;
  }

  if(LX200response[0] != 'E' &&
     LX200response[0] != 'W') {
    fprintf(stderr, "Query for scope pier side: bad response: %s\n",
	    LX200response);
    ResyncInterface();
    return 0;
  }

  return (LX200response[0] == 'E') ? 0 : 1;
}

#if defined GEMINI
 
void BuildMI250String(char *buffer) {
  // calculate checksum
  unsigned char cksum = buffer[0];
  for(char *s = buffer+1; *s; s++) {
    cksum = cksum^ (*s);
  }
  // append the checksum
  int pos = strlen(buffer);
  buffer[pos] = 0x40 + (cksum & 0x7f);
  buffer[pos+1] = '#';
  buffer[pos+2] = 0;
}

int BuildMI250Command(char *buffer,
		      int Direction,
		      int CommandID,
		      int xx) {
  int use_param = 0;
  if(Direction == MI250_GET) {
    if(CommandID == 511 ||
       CommandID == 512)
      use_param = 1;
  } else {
    if((CommandID >= 21 && CommandID <= 26) ||
       CommandID == 100 ||
       CommandID == 110 ||
       CommandID == 120 ||
       CommandID == 140 ||
       CommandID == 150 ||
       CommandID == 170 ||
       (CommandID >= 200 && CommandID <= 211) ||
       CommandID == 221 ||
       CommandID == 223 ||
       CommandID == 311 ||
       (CommandID >= 411 && CommandID <= 415) ||
       CommandID == 501 ||
       CommandID == 503 ||
       CommandID == 504 ||
       CommandID == 509)
      use_param = 1;
  }

  if(use_param) {
    sprintf(buffer, "%c%d:%d", (Direction == MI250_GET ? '<' : '>'),
	    CommandID, xx);
  } else {
    sprintf(buffer, "%c%d:",  (Direction == MI250_GET ? '<' : '>'),
	    CommandID);
  }
  BuildMI250String(buffer);
  return 1;
}

// The following can only be used with PEC set/get commands
int BuildMI250PECCommand(char *buffer,
			 int Direction, // MI250_GET/SET
			 int CommandID,
			 int offset,
			 int value,
			 int repeat_count) {
  if(Direction == MI250_GET) {
    sprintf(buffer, "<%d:%d", CommandID, offset);
  } else {
    sprintf(buffer, ">%d:%d;%d;%d", CommandID, value, offset, repeat_count);
  }
  BuildMI250String(buffer);
  return 1;
}

#elif defined GM2000
#include "sync_session.h"
void ClearMountModel(void) {
  char response[16];
  ScopeResponseStatus Status;

  if (!comm_initialized) {
    fprintf(stderr, "scope_api: comm link never initialized.\n");
    exit(2);
  }

  if(scope_message(":delalig#",
		   RunFast,
		   FixedLength,
		   response,
		   1,
		   &Status)) {
    fprintf(stderr, "ClearMountModel: error deleting model.\n");
  }
}

void QuickSyncMount(DEC_RA catalog_position) { // J2000 posit
  char response[64];
  ScopeResponseStatus Status;

  if (SetTargetPosition(&catalog_position)) {
    return; // error trying to set position
  }
  
  if(scope_message(":CM#",
		   RunFast,
		   StringResponse,
		   response,
		   sizeof(response),
		   &Status)) {
    fprintf(stderr, "QuickSyncMount: error performing quick sync.\n");
  }
  /*NO_RETURN_VALUE*/
}

 void LoadSyncPoints(SyncPointList *s) {
  // Three step process:
  // 1. ":newalig#" to start creating a new alignment spec
  // 2. ":newalpt#" to send each alignment point
  // 3. ":endalig#" to end the alignment sequence
  char response[64];
  ScopeResponseStatus Status;

  //****************
  // STEP 1
  //****************
  if(scope_message(":newalig#",
		   RunFast,
		   StringResponse,
		   response,
		   sizeof(response),
		   &Status)) {
    fprintf(stderr, "NewAlign: error creating new align spec.\n");
    return;
  }

  //****************
  // STEP 2
  //****************
  for (auto p : *s) {
    char align_point[128];

    sprintf(align_point, ":newalpt%s#", p);
    if (scope_message(align_point,
		      RunFast,
		      StringResponse,
		      response,
		      sizeof(response),
		      &Status)) {
      fprintf(stderr, ":newalpt command transmission failed.\n");
    } else {
      if (response[0] == 'E') {
	fprintf(stderr, "Point '%s' rejected by mount.\n", p);
      } else {
	int point_number;
	sscanf(response, "%d", &point_number);
	fprintf(stderr, "Point %d accepted by mount.\n", point_number);
      }
    }
  }

  //****************
  // STEP 3
  //****************
  if(scope_message(":endalig#",
		   RunFast,
		   StringResponse,
		   response,
		   sizeof(response),
		   &Status)) {
    fprintf(stderr, "EndAlign: error sending end align command.\n");
    return;
  } else {
    if (response[0] == 'V') {
      fprintf(stderr, "Alignment model updated successfully.\n");
    } else {
      fprintf(stderr, "Alignment model update failed.\n");
    }
  }
}

//****************************************************************
//        MountSetPressure()
//****************************************************************

// Set local pressure and temperature for use in computing refraction
void MountSetPressure(double pressure_hPa) {
  char response[64];
  ScopeResponseStatus Status;
  char buffer[132];

  sprintf(buffer, ":SRPRS%06.1lf#", pressure_hPa);

  if(scope_message(buffer,
		   RunFast,
		   FixedLength,
		   response,
		   1,
		   &Status)) {
    fprintf(stderr, "MountSetPressure: error sending message to mount.\n");
    return;
  } else {
    if (response[0] == '0') {
      fprintf(stderr, "MountSetPressure: %lf rejected by mount.\n",
	      pressure_hPa);
    } else if(response[0] == '1') {
      fprintf(stderr, "MountSetPressure: Accepted by mount.\n");
    } else {
      fprintf(stderr, "MountSetPressure: Unrecognized mount response: %s\n",
	      response);
    }
  }
}

void GM2000AddSyncPoint(DEC_RA actual_current_pos) { // J2000
  char response[64];
  ScopeResponseStatus Status;

  SetTargetPosition(&actual_current_pos);

  if(scope_message(":CMS#",
		   RunFast,
		   StringResponse,
		   response,
		   sizeof(response),
		   &Status)) {
    fprintf(stderr, "GM2000AddSyncPoint(): error sending message to mount.\n");
    return;
  } else {
    if (response[0] == 'E') {
      fprintf(stderr, "Sync point rejected by mount.\n");
    } else if(response[0] == 'V') {
      fprintf(stderr, "Sync point accepted by mount.\n");
    } else {
      fprintf(stderr, "GM2000AddSyncPoint(): Unrecognized mount response: %s\n",
	      response);
    }
  }
}

//****************************************************************
//        MountSetTemperature()
//****************************************************************

void MountSetTemperature(double deg_C) {
  char response[64];
  ScopeResponseStatus Status;
  char buffer[132];

  sprintf(buffer, ":SRTMP%+06.1lf#", deg_C);

  if(scope_message(buffer,
		   RunFast,
		   FixedLength,
		   response,
		   1,
		   &Status)) {
    fprintf(stderr, "MountSetTemperature: error sending message to mount.\n");
    return;
  } else {
    if (response[0] == '0') {
      fprintf(stderr, "MountSetTemperature: %lf rejected by mount.\n",
	      deg_C);
    } else if(response[0] == '1') {
      fprintf(stderr, "MountSetTemperature: Accepted by mount.\n");
    } else {
      fprintf(stderr, "MountSetTemperature: Unrecognized mount response: %s\n",
	      response);
    }
  }
}
#endif

 // return_string must have room for "HH:MM:SS.SS"
void GetSiderealTime(char *return_string) {
  char response[64];
  ScopeResponseStatus Status;

  if(scope_message(":GS#",
		   RunFast,
		   StringResponse,
		   response,
		   sizeof(response),
		   &Status)) {
    fprintf(stderr, "GetSiderealTime(): Error fetching time from GM2000.\n");
    *return_string = 0; // no response string
  } else {
    strcpy(return_string, response);
  }
  /*NO_RETURN_VALUE*/
}
  
// GetAlignmentPoints() pulls alignment points out of the GM2000
std::list<char *>  *GetAlignmentPoints(void) {
  char response[64];
  ScopeResponseStatus Status;
  int num_align_stars;
  std::list<char *> *starlist = new std::list<char *>;

  if(scope_message(":getalst#",
		   RunFast,
		   StringResponse,
		   response,
		   sizeof(response),
		   &Status)) {
    fprintf(stderr, "Get # align stars: command not accepted.\n");
  } else {
    sscanf(response, "%d#", &num_align_stars);

    for (int i=0; i<num_align_stars; i++) {
      char request[64];
      sprintf(request, ":getali%d#", i+1);
    
      if(scope_message(request,
		       RunFast,
		       StringResponse,
		       response,
		       sizeof(response),
		       &Status)) {
	fprintf(stderr, "%s: command not accepted.\n", request);
	break;
      } else {
	char *star_string = strdup(response);
	starlist->push_back(star_string);
      }
    }
  }
  return starlist;
}
//****************************************************************
//        Meridian Flip Support
//****************************************************************

JULIAN PredictFlipStartWindow(DEC_RA position) {
  JULIAN right_now(time(0));
  double current_ha = position.hour_angle(right_now);
  double delta_radians = (-20.0 * M_PI/180.0 - current_ha);
  // if delta_radians is positive, window is sometime in the future
  if (delta_radians > 0.0) {
    return right_now.add_days(delta_radians*(12/M_PI)/24.0);
  } else {
    return JULIAN(0.0); // indicates error
  }
  /*NOTREACHED*/
}

JULIAN PredictFlipEndWindow(DEC_RA position) {
  JULIAN right_now(time(0));
  double current_ha = position.hour_angle(right_now);
  double delta_radians = (+20.0 * M_PI/180.0 - current_ha);
  // if delta_radians is positive, window is sometime in the future
  if (delta_radians > 0.0) {
    return right_now.add_days(delta_radians*(12/M_PI)/24.0);
  } else {
    return JULIAN(0.0); // indicates error
  }
  /*NOTREACHED*/
}

long MinsRemainingToLimit(void) {
  char response[64];
  ScopeResponseStatus Status;

  if(scope_message(":Gmte#",
		   RunFast,
		   StringResponse,
		   response,
		   sizeof(response),
		   &Status)) {
    fprintf(stderr, "GetFlipStartWindow(): Error fetching remaining time from GM2000.\n");
    response[0] = 0;
  }
  long mins_remaining;
  if (sscanf(response, "%ld#", &mins_remaining) != 1) {
    fprintf(stderr, "GetFlipStartWindow(): bad scope response: %s\n", response);
    return -1;
  }
  return mins_remaining;
}

JULIAN GetFlipTime(long adjustment) {
  long mins_remaining = MinsRemainingToLimit();
  // convert 40 degrees of rotation to minutes
  if (mins_remaining < adjustment) {
    return JULIAN(0.0);
  }
  return(JULIAN(time(0)).add_days((mins_remaining - adjustment)/(60.0*24.0)));
}

JULIAN GetFlipStartWindow(void) {
  const long window_size = (long) (40*24*60/360);
  return GetFlipTime(window_size);
}
 
JULIAN GetFlipEndWindow(void) {
  return GetFlipTime(0);
}

bool PerformMeridianFlip(void) { // return true if successful
  char response[1];
  ScopeResponseStatus Status;

  if(scope_message(":FLIP#",
		   RunSlow,
		   FixedLength,
		   response,
		   sizeof(response),
		   &Status)) {
    fprintf(stderr, "PerformMeridianFlip(): Error fetching response from GM2000.\n");
    response[0] = 0;
  }

  fprintf(stderr, "Meridian flip command returned '%c'\n",
	  response[0]);

  if (response[0] == '1') {
    // Wait for the mount to finish...
    while(SlewInProgress()) {
      sleep(3);
    }
    return true; //successful return
  }

  return false; // something went wrong
}

 
double GetGuideRate(void) { // arcseconds/second
  char response[80];
  ScopeResponseStatus Status;

  if(scope_message(":Ggui#",
		   RunFast,
		   StringResponse,
		   response,
		   sizeof(response),
		   &Status)) {
    fprintf(stderr, "GetGuideRate(): Error fetching response from GM2000.\n");
    response[0] = 0;
  }

  return atof(response);
}

//****************************************************************
//        Flat Light Box Support
//****************************************************************
int GetFlatLightStatus(void) {
  lxFlatLightMessage *message;
  lxGenMessage *inbound_message;
  lxFlatLightMessage *status;
  int status_byte = 0;

  if(!comm_initialized) {
    fprintf(stderr, "scope_api: comm link never initialized.\n");
    exit(2);
  }
  // with no command set, this becomes a status request message
  message = new lxFlatLightMessage(comm_socket);
  message->send();

  // That's the easy part.  Now wait for a response.
  
  // Receive the message from the socket
  inbound_message = lxGenMessage::ReceiveMessage(comm_socket);
  switch(inbound_message->MessageID()) {

    /********************/
    /*  StatusMessage   */
    /********************/
  case lxFlatLightMessageID:
    status = (lxFlatLightMessage *) inbound_message;
    status_byte = status->GetStatusByte();
    break;

  default:
    // Makes absolutely no sense for us to receive these.
    fprintf(stderr, "Illegal message received by scope_api().\n");
    break;

  }
  delete inbound_message;
  delete message;

  bool is_up = status_byte & 0x04;
  bool is_down = status_byte & 0x08;
  int response = 0;
  if (is_up) response += FLATLIGHT_UP;
  if (is_down) response += FLATLIGHT_DOWN;

  return response;
}

void MoveFlatLight(int position) {  // FLATLIGHT_UP or FLATLIGHT_DOWN
  lxFlatLightMessage *message;
  lxGenMessage *inbound_message;

  // Validate argument passed in by user
  if (position != lxFlatLightMessage::FLAT_MOVE_UP &&
      position != lxFlatLightMessage::FLAT_MOVE_DOWN) {
    fprintf(stderr, "MoveFlatLight: illegal commanded position: %d\n",
	    position);
    return;
  }

  if(!comm_initialized) {
    fprintf(stderr, "scope_api: comm link never initialized.\n");
    exit(2);
  }

  message = new lxFlatLightMessage(comm_socket);
  message->SetDirectionByte((position == FLATLIGHT_UP) ?
			    lxFlatLightMessage::FLAT_MOVE_UP :
			    lxFlatLightMessage::FLAT_MOVE_DOWN);
  message->send();

  // That's the easy part.  Now wait for a response.

  // Receive the message from the socket
  inbound_message = lxGenMessage::ReceiveMessage(comm_socket);
  switch(inbound_message->MessageID()) {

    /********************/
    /*  StatusMessage   */
    /********************/
  case lxFlatLightMessageID:
    // This is what we're expecting. But do nothing with it.
    break;

  default:
    // Makes absolutely no sense for us to receive these.
    fprintf(stderr, "Illegal message received by scope_api().\n");
    break;

  }
  delete inbound_message;
  delete message;
}

void WaitForFlatLight(void) { // blocks for a long time
  int loop_count = 30;
  do {
    int status = GetFlatLightStatus();
    if (status & (FLATLIGHT_UP | FLATLIGHT_DOWN)) return;
    sleep(2);
  } while(loop_count--);
}

bool FlatLightMoving(void) { // doesn't block (much)
  int status = GetFlatLightStatus();
  return (status & (FLATLIGHT_UP | FLATLIGHT_DOWN))==0;
} 


//****************************************************************
//        Angular Position API
//****************************************************************
void GetAngularPosition(double &ra_axis_degrees, double &dec_axis_degrees) {
  ScopeResponseStatus Status;
  char response[80];

  if(scope_message(":GaXa#", // Get RA axis angular position
		   RunMedium,
		   StringResponse,
		   response,
		   sizeof(response),
		   &Status)) {
    fprintf(stderr, "Get RA axis angular position error: Status = %d\n",
	    Status);
    return;
  }
  if(sscanf(response, "%lf#", &ra_axis_degrees) != 1) {
    fprintf(stderr, "ERR: RA axis location: %s\n", response);
  }

  if(scope_message(":GaXb#", // Get Dec axis angular position
		   RunMedium,
		   StringResponse,
		   response,
		   sizeof(response),
		   &Status)) {
    fprintf(stderr, "Get Dec axis angular position error: Status = %d\n",
	    Status);
    return;
  }
  if(sscanf(response, "%lf#", &dec_axis_degrees) != 1) {
    fprintf(stderr, "ERR: Dec axis location: %s\n", response);
  }
}

// After setting angular position, mount will be "stopped". Need to
// send MountResumeTracking() before doing "normal" stuff again.
void SetAngularPosition(double ra_axis_degrees, double dec_axis_degrees) {
  ScopeResponseStatus Status;
  char response[80];
  char buffer[80];

  sprintf(buffer, ":SaXa%+9.4lf#", ra_axis_degrees);

  if(scope_message(buffer,
		   RunFast,
		   FixedLength,
		   response,
		   1,
		   &Status)) {
    fprintf(stderr, "Error sending AngularPosition(RA) command: %s\n",
	    buffer);
    return;
  } else {
    if(response[0] != '1') {
      fprintf(stderr, "Error response to set RA: %s\n", buffer);
      ResyncInterface();
      return;
    }
  }

  sprintf(buffer, ":SaXb%+9.4lf#", dec_axis_degrees);

  if(scope_message(buffer,
		   RunFast,
		   FixedLength,
		   response,
		   1,
		   &Status)) {
    fprintf(stderr, "Error sending AngularPosition(Dec) command: %s\n",
	    buffer);
    return;
  } else {
    if(response[0] != '1') {
      fprintf(stderr, "Error response to set Dec: %s\n", buffer);
      ResyncInterface();
      return;
    }
  }

  if(scope_message(":MaX#",
		   RunSlow,
		   MixedModeResponse,
		   response,
		   1,
		   &Status,
		   "0")) {
    fprintf(stderr, "Error sending :MaX# command.\n");
  } else {
    if(response[0] != '0') {
      fprintf(stderr, "Error response to :MaX# command. (%d)\n",
	      response[0]);
      response[23] = 0;
      fprintf(stderr, "%s\n", response+1);
      ResyncInterface();
    }
  }
}

//****************************************************************
//        Control dual-axis tracking
// (default is "enabled" until/unless SetDualAxisTracking(false) is
// issued. )
//****************************************************************
bool DualAxisTrackingEnabled(void) { // true means dual-axis tracking
				// is turned on.
  ScopeResponseStatus Status;
  char response[8];
  
  if(scope_message(":Gdat#",
		   RunFast,
		   FixedLength,
		   response,
		   1,
		   &Status)) {
    fprintf(stderr, "Error sending dual-axis query\n");
    return false; //potentially misleading
  } else {
    return (response[0] == '1');
  }
}
		   
void SetDualAxisTracking(bool enabled) {
  char buffer[80];
  char response[8];
  ScopeResponseStatus Status;

  sprintf(buffer, ":Sdat%c#", (enabled ? '1' : '0'));

  if(scope_message(buffer,
		   RunFast,
		   FixedLength,
		   response,
		   1,
		   &Status)) {
    fprintf(stderr, "Error setting dual-axis mode\n");
  } else {
    if (response[0] != '1') {
      fprintf(stderr, "Response msg error setting dual-axis mode: %c\n",
	      response[0]);
    }
  }
}

 
