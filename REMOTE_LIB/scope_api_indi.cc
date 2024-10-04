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

#include <string.h>		// memset()
#include <stdlib.h>		// exit()
#include <stdio.h>
#include <unistd.h>		// exit(), usleep()
#include <sys/socket.h>		// socket()
#include <arpa/inet.h>		// inet_ntoa()
#include <assert.h>
#include <list>
#include "ports.h"
#include "mount_model.h"
#include "scope_api.h"
#include "mount_indi.h"
#include "focuser_indi.h"
#include "alt_az.h"
#include <system_config.h>

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

void connect_to_scope(void) {
  ConnectAstroINDI();
  if (/*extern*/mount == nullptr) {
    int tries = 1000; // 1000 * 10msec = 10sec total patience
    do {
      usleep(10000);		// 10 msec
    } while(tries-- > 0 and (mount == nullptr));
    if (mount == nullptr) {
      std::cerr << "connect_to_scope: failed.\n";
      exit(-2);
    }
  }
  if (not mount->WaitForConnect(5 /*seconds*/ )) {
    std::cerr << "Unable to connect to mount hardware.\n";
    exit(-2);
  }
}

static void connect_to_1_focuser(FOCUSER_INDI **f_dev) {
  if (*f_dev) return;
  ConnectAstroINDI();
  int tries = 1000; // 1000 * 10msec = 10sec total patience
  do {
    usleep(10000);		// 10 msec
  } while(tries-- > 0 and (*f_dev == nullptr));
  if (*f_dev == nullptr) {
    std::cerr << "connect_to_focuser: failed.\n";
  }
}

void connect_to_focuser(void) {
  const int num_focusers = /*extern*/ system_config.NumFocusers();
  if (num_focusers == 0) return;
  if (num_focusers < 2) {
    connect_to_1_focuser(&focuser);
  } else {
    connect_to_1_focuser(&coarse_focuser);
    connect_to_1_focuser(&fine_focuser);
  }
}

void disconnect_focuser(void) {
  INDIDisconnectINDI();
}

void disconnect_scope(void) {
  INDIDisconnectINDI();
}

void initialize_mount() {
  mount->InitializeMount();
}

// set turn_off to 1 to disable tracking at the sidereal rate and stop
// the RA motor. set turn_off to 0 to resume tracking.
void ControlTrackingMotor(int turn_off) {
  mount->ControlTrackingMotor(turn_off);
}

// scope_focus() will run the focus motor (slow speed) for the
// indicated number of msec.  Positive values move one way and
// negative numbers move the other. The function will block as long as
// needed until the focus motor stops. The total focus position will
// be returned.
static FOCUSER_INDI *GetFocuser(FocuserName focuser_name) {
  FOCUSER_INDI *commanded_focuser = nullptr;
  if (focuser_name == FOCUSER_COARSE) {
    if (coarse_focuser) {
      commanded_focuser = coarse_focuser;
    }
  } else {
    if (fine_focuser) {
      commanded_focuser = fine_focuser;
    }
  }

  if (commanded_focuser == nullptr) {
    if (focuser) {
      commanded_focuser = focuser;
    } else if (fine_focuser) {
      commanded_focuser = fine_focuser;
    } else {
      commanded_focuser = focuser;
    }
  }

  if (commanded_focuser == nullptr) {
    std::cerr << "ERROR: no focuser found.\n";
  }
  return commanded_focuser;
}

int scope_sync(DEC_RA *Location) {
  return mount->StarSync(Location);
}

long scope_focus(long msec,
		 FocuserMoveType move_type,
		 FocuserName focuser_name) {
  FOCUSER_INDI *commanded_focuser = GetFocuser(focuser_name);
  if (commanded_focuser == nullptr) {
    return -1;
  }

  return commanded_focuser->DoFocus(msec, move_type);
}

long CumFocusPosition(FocuserName focuser_name) {
  FOCUSER_INDI *commanded_focuser = GetFocuser(focuser_name);
  if (commanded_focuser == nullptr) {
    return -1;
  }
  return commanded_focuser->CurrentFocus();
}

bool dec_flip_assumed;		// whether last goto assumed a flip
DEC_RA desired_catalog_goto_location;

/****************************************************************/
/*        Telescope Motion					*/
/****************************************************************/
int MoveTo(DEC_RA *CatalogLocation, int encourage_flip) {
  return mount->MoveTo(*CatalogLocation, encourage_flip);
}

void WaitForGoToDone(void) {
  mount->WaitForMoveDone();
}
  
//****************************************************************
//        ParkTelescope()
//****************************************************************
 
// blocks for a long time
void ParkTelescope(void) {
  mount->Park();
}

//****************************************************************
//        UnParkTelescope()
//****************************************************************
 
// blocks for a long time
void UnParkTelescope(void) {
  mount->Unpark();
}

//****************************************************************
//        Dec_Axis_Is_Flipped()
// Returns true if the camera is inverted (north/south); also
// indicates that the declination axis is flipped.
//****************************************************************
bool dec_axis_is_flipped(double hour_angle, bool scope_on_west) {
  return !scope_on_west;
}

bool dec_axis_is_flipped(void) {
  return !scope_on_west_side_of_pier();
}

bool dec_axis_likely_flipped(double hour_angle) {
  double ha  = hour_angle;
  if(ha > M_PI) ha -= (M_PI*2.0);

  return (ha >= 0.0);
}
  
// returns 0 on success, -1 if something went wrong.
int SmallMove(double delta_ra_arcmin, double delta_dec_arcmin) {
  return mount->SmallMove(delta_ra_arcmin, delta_dec_arcmin);
}

DEC_RA ScopePointsAt(void) {
  // Get the scope's idea of where it points and run through our model
  return mount->ScopePointsAtJ2000();
}

// Returns Sidereal Time measured in radians (0..2*Pi) corresponding
// to (0..24hrs) 
double GetSiderealTime(void) {
  return mount->GetLocalSiderealTime()*(M_PI/12.0);
}

double GetScopeHA(void) {	// scope hour angle (0 == meridian, rads)
  DEC_RA current_ra = ScopePointsAt();
  double current_st = GetSiderealTime();

  double ha = current_st - current_ra.ra_radians();
  if (ha > M_PI) ha -= M_PI*2;
  if (ha < -M_PI) ha += M_PI*2;

  return ha;
}

ALT_AZ ScopePointsAt_altaz(void) {
  time_t now = time(0);
  JULIAN jnow(now);
  return ALT_AZ(ScopePointsAt(), jnow);
}

DEC_RA RawScopePointsAt(void) {
  return mount->RawScopePointsAt();
}


// specifies time to guide in seconds
void guide(double NorthSeconds, double EastSeconds) {
  mount->Guide(NorthSeconds, EastSeconds);
}

/****************************************************************/
/*        scope_on_west_side_of_pier()				*/
/****************************************************************/
// Returns 1 if the scope is on the west side of the pier. If on the
// east side of the pier, will return 0. 
int
scope_on_west_side_of_pier(void) {
  return mount->ScopeOnWestSideOfPier();
}

#if defined GM2000
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

#endif

 // return_string must have room for "HH:MM:SS.SS"
void GetSiderealTime(char *return_string) {
  double st = mount->GetLocalSiderealTime();
  
  int hours = (int) st;
  double minf = (st-hours)*60.0;
  int min = (int) minf;
  double secf = (minf-min)*60.0;
  if (secf > 59.994) {
    secf = 0.0;
    min++;
  }
  if (min > 59) {
    min = 0;
    hours++;
  }
  if (hours > 23) {
    hours = 0;
  }

  sprintf(return_string, "%02d:%02d:%05.2f", hours, min, secf);
}
  
void DisconnectINDI(void) {
  INDIDisconnectINDI();		// astro_indi.cc
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

 
