/* This may look like C code, but it is really -*-c++-*- */
/*  scope_api.h -- User's view of what the mount can do
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
#ifndef _SCOPE_API_H
#define _SCOPE_API_H

// define either GEMINI or LX200 or GM2000
#if (! defined GEMINI) and (! defined GM2000)
#define GEMINI
#endif

#include "lx_ScopeMessage.h"
#include "lx_ScopeResponseMessage.h"
#include "dec_ra.h"
#include "alt_az.h"
#include "gendefs.h"
#include <stdio.h>
#include <list>

enum FocuserName {
  FOCUSER_FINE,
  FOCUSER_COARSE,
  FOCUSER_DEFAULT};
enum FocuserMoveType {FOCUSER_MOVE_ABSOLUTE,
		      FOCUSER_MOVE_RELATIVE};

// connect_to_scope() will establish a connection to the scope server
// process running on the scope computer.  It will block for as long
// as necessary to establish the connection. If unable to establish a
// connection (for whatever reason), it will print an error message to
// stderr and will exit.
void connect_to_scope(void);
void connect_to_focuser(void);

void disconnect_scope(void);	// needed for INDI
void disconnect_focuser(void);	// needed for INDI

// scope_focus() will run the focus motor (slow speed) for the
// indicated number of msec.  Positive values move one way and
// negative numbers move the other. The function will block as long as
// needed until the focus motor stops. The total focus position will
// be returned.
long scope_focus(long msec,
		 FocuserMoveType=FOCUSER_MOVE_RELATIVE,
		 FocuserName focuser_name=FOCUSER_DEFAULT);

// How many focusers are installed. Response can be 0, 1, or 2
int NumFocusers(void);

// provides the current position of the telescope focus (in net msec).
long CumFocusPosition(FocuserName focuser_name=FOCUSER_DEFAULT);

// returns 0 on success, -1 if something went wrong.
int scope_message(const char *command_string,
		  ExecutionChoices timeout,
		  ResponseTypeChoices responseType,
		  char *response_string,
		  int response_length,
		  ScopeResponseStatus *responseStatus,
		  const char *SingleCharResponses = 0);

// returns 0 on success, -1 if something went wrong.
int MoveTo(DEC_RA *Location, int encourage_flip = 0);

// returns 0 on success, -1 if something went wrong.
int SmallMove(double delta_ra_arcmin, double delta_dec_arcmin);

// wait for scope to stop slewing after a move. This command is always
// valid. If the scope wasn't in a move, this will immediately return.
void WaitForGoToDone(void);
// Here's a non-blocking way to test whether a slew is still
// active. Works for both MoveTo() and SmallMove().
bool SlewInProgress(void);

// specifies time to guide in seconds (used by drifter)
// To use this you need to know guide speed. That value is hardcoded
// into drifter.cc (and should be fixed).
void guide(double NorthSeconds, double EastSeconds);

// set turn_off to 1 to disable tracking at the sidereal rate and stop
// the RA motor. set turn_off to 0 to resume tracking.
void ControlTrackingMotor(int turn_off);

// Sets scope's internal position. Returns 0 on success, -1 if
// something went wrong.
// 09-14-2015: This should not be used any more. Use the API found in
// mount_model, instead. 
int scope_sync(DEC_RA *Location);

// Will try to resynchronize the interface with the scope controller
int ResyncInterface(void);

// Returns 1 if the scope is on the west side of the pier. If on the
// east side of the pier, will return 0. Do not confuse this with
// dec_axis_is_flipped(), which is almost certainly what you *really*
// want to use.
int scope_on_west_side_of_pier(void);

// Returns true if the camera is inverted (north/south); also
// indicates that the declination axis is flipped.
bool dec_axis_is_flipped(void);
bool dec_axis_is_flipped(double hour_angle, bool scope_on_west);
bool dec_axis_likely_flipped(double hour_angle);

ALT_AZ ScopePointsAt_altaz(void);
double GetScopeHA(void);	// scope hour angle (0 == meridian, rads)
DEC_RA ScopePointsAt(void);
DEC_RA RawScopePointsAt(void);	// don't use this; model adjustments
				// have not been incorporated into
				// this number. Value is not epoch-adjusted.

void ParkTelescope(void);	// blocks for a long time
void UnParkTelescope(void);	// blocks for a long time
void MountGoToFlatLight(void);	// blocks for a long time; stops
				// tracking; needs
				// MountResumeTracking() to
				// undo this.
void MountResumeTracking(void); //


#ifdef GM2000
// zeroes the mount's model. Cannot be undone.
void ClearMountModel(void);
void QuickSyncMount(DEC_RA catalog_position); // J2000 posit
void GM2000AddSyncPoint(DEC_RA actual_current_pos); // J2000

// SyncPointList must be in the string format specified by the GM2000
// protocol document for the :newalpt# message
typedef std::list<const char *> SyncPointList;
void LoadSyncPoints(SyncPointList *s);
std::list<char *> *GetAlignmentPoints(void);

void GetAngularPosition(double &ra_axis_degrees, double &dec_axis_degrees);
// After setting angular position, mount will be "stopped". Need to
// send MountResumeTracking() before doing "normal" stuff again.
void SetAngularPosition(double ra_axis_degrees, double dec_axis_degrees);

// return_string must have room for "HH:MM:SS.SS"
void GetSiderealTime(char *return_string);
double GetSiderealTime(void); // value in radians

// Set local pressure and temperature for use in computing refraction
void MountSetPressure(double pressure_hPa);
void MountSetTemperature(double deg_C);
// Logging control
void MountStartLogging(void);
void MountStopLogging(void);
void MountDumpLog(FILE *fp);

double GetGuideRate(void); // arcseconds/second

//****************************************************************
//        Flat Light Box Support
//****************************************************************
#define FLATLIGHT_UP 0x01
#define FLATLIGHT_DOWN 0x02
#define FLATLIGHT_ON 0x04
void MoveFlatLight(int position); // FLATLIGHT_UP or FLATLIGHT_DOWN
void WaitForFlatLight(void); // blocks for a long time
bool FlatLightMoving(void); // doesn't block (much)
int  GetFlatLightStatus(void); // returns flag bits defined above

//****************************************************************
//        Meridian Flip Support
//****************************************************************

JULIAN PredictFlipStartWindow(DEC_RA position);
JULIAN PredictFlipEndWindow(DEC_RA position);

JULIAN GetFlipStartWindow(void);
JULIAN GetFlipEndWindow(void);

long MinsRemainingToLimit(void);
void DumpCurrentLimits(void); // prints current RA axis location to stderr

// The following command sends a command to the mount; it does not
// wait for the mount to finish moving!
bool PerformMeridianFlip(void); // return true if successful

//****************************************************************
//        Control dual-axis tracking
// (default is "enabled" until/unless SetDualAxisTracking(false) is
// issued. )
//****************************************************************
bool DualAxisTrackingEnabled(void); // true means dual-axis tracking
				    // is turned on.
void SetDualAxisTracking(bool enabled);

#else

#define MI250_GET  101
#define MI250_SET  102

int BuildMI250Command(char *buffer,
		      int Direction, // MI250_GET/SET
		      int CommandID,
		      int xx = 0); // parameter, if needed (else 0)
int BuildMI250Command(char *buffer,
		      int Direction, // MI250_GET/SET
		      int CommandID,
		      int sign,
		      int deg, int min);

// The following can only be used with PEC set/get commands
int BuildMI250PECCommand(char *buffer,
			 int Direction, // MI250_GET/SET
			 int CommandID,
			 int offset,
			 int value = 0,
			 int repeat_count = 0);
#endif
#endif
