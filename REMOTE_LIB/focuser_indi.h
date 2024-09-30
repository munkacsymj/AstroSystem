/* This may look like C code, but it is really -*-c++-*- */
/*  focuser_indi.h -- Implements user view of color filter wheel
 *
 *  Copyright (C) 2024 Mark J. Munkacsy

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
#pragma once

#include "blocker_indi.h"
#include "astro_indi.h"
#include "scope_api.h"

class FOCUSER_INDI : public LocalDevice {
public:
  FOCUSER_INDI(AstroDevice *device, const char *connection_port);
  ~FOCUSER_INDI(void);

  bool FOCUSERPresent(void) const { return focuser_absolute.available; }

  long DoFocus(long msec, // time on some focusers, ticks on others
	       FocuserMoveType move_type);

  long CurrentFocus(void);

  void DoINDIRegistrations(void);
  void WaitForPropertiesToArrive(void);

private:
  long requested_location;
  Blocker blocker;
  AstroDevice *dev {nullptr};
  
  AstroValueSwitch focuser_dir_in{this,"FOCUS_MOTION", "FOCUS_INWARD"};
  AstroValueSwitch focuser_dir_out{this,"FOCUS_MOTION", "FOCUS_OUTWARD"};
  AstroValueNumber focuser_relative{this,"REL_FOCUS_POSITION", "FOCUS_RELATIVE_POSITION"};
  AstroValueNumber focuser_absolute{this,"ABS_FOCUS_POSITION", "FOCUS_ABSOLUTE_POSITION"};
  AstroValueNumber focuser_maxlimit{this,"FOCUS_MAX", "FOCUS_MAX_VALUE"};
  AstroValueNumber focuser_sync{this,"FOCUS_SYNC", "FOCUS_SYNC_VALUE"};
  AstroValueSwitch focuser_debug_enable {this, "DEBUG", "ENABLE"};
  AstroValueSwitch focuser_debug_disable {this, "DEBUG", "DISABLE"};
  AstroValueSwitch focuser_log_debug {this,"LOGGING_LEVEL", "LOG_DEBUG"};
  AstroValueSwitch focuser_log_file  {this,"LOG_OUTPUT", "FILE_DEBUG"};
  
  void SetupDebug(void);

  friend void FOCUSERPropertyUpdate(INDI::Property property);

};

extern FOCUSER_INDI *focuser, *coarse_focuser, *fine_focuser;


