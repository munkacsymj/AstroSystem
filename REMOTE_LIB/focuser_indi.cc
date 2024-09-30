/*  focuser_indi.cc -- Implements user view of focuser
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

#include <list>
#include <math.h>

#include "scope_api.h"
#include "focuser_indi.h"

#define FOCUS_TOLERANCE 6 // 6 ticks is good enough???

void
FOCUSER_INDI::WaitForPropertiesToArrive(void) {
  int try_count = 0;
  do {
    if (this->focuser_absolute.available) return;
    usleep(100000); // 100 msec
  } while(try_count++ < 30); // 3 seconds
  std::cerr << "Critical properties for focuser didn't arrive before timeout.\n";
}

FOCUSER_INDI::FOCUSER_INDI(AstroDevice *device, const char *connection_port) :
  LocalDevice(device, connection_port), dev(device) {
  //this->DoINDIRegistrations();
  // This picks up both values associated with this property name
  dev->indi_device.watchProperty(focuser_relative.property_name,
				 [this](INDI::Property p) {
				   std::cerr << "Focuser (rel) Property changed.\n";
				   this->blocker.Signal();
				 },
				 INDI::BaseDevice::WATCH_UPDATE);
  dev->indi_device.watchProperty(focuser_absolute.property_name,
				 [this](INDI::Property p) {
				   std::cerr << "Focuser (abs) Property changed.\n";
				   this->blocker.Signal();
				 },
				 INDI::BaseDevice::WATCH_UPDATE);
}

FOCUSER_INDI::~FOCUSER_INDI(void) {;}

void
FOCUSER_INDI::SetupDebug(void) {
  if (this->focuser_debug_enable.getState() == ISS_ON) return;
  
  this->focuser_debug_enable.setState(ISS_ON);
  this->focuser_debug_disable.setState(ISS_OFF);
  this->dev->local_client->sendNewSwitch(this->focuser_debug_enable.property->indi_property);
  sleep(1);
  this->focuser_log_file.setState(ISS_ON);
  this->dev->local_client->sendNewSwitch(this->focuser_log_file.property->indi_property);
  this->focuser_log_debug.setState(ISS_ON);
  this->dev->local_client->sendNewSwitch(this->focuser_log_debug.property->indi_property);
  sleep(1);
}

long
FOCUSER_INDI::DoFocus(long msec, // time on some focusers, ticks on others
		      FocuserMoveType move_type) {
  static bool debug_setup = false;

  this->WaitForPropertiesToArrive();

  if (not debug_setup) {
    debug_setup = true;
    this->SetupDebug();
  }
  
  const long starting_point = CurrentFocus();
  long target_point = msec; 	// correct if this is an absolute move
  // all moves are convered to absolute for INDI
  if (move_type == FOCUSER_MOVE_RELATIVE) {
    if (msec == 0) return starting_point;
    
    target_point = starting_point+msec;
  }

  std::cerr << "Focus = " << CurrentFocus()
	    << "State pre-assignment is "
	    << this->focuser_absolute.property->indi_property.getStateAsString() << std::endl;
  this->blocker.Setup();
  this->focuser_absolute.setValue((double) target_point);
  this->dev->local_client->sendNewNumber(this->focuser_absolute.property->indi_property);
    
  std::cerr << "Focus = " << CurrentFocus()
	    << "State before blocking wait is "
	    << this->focuser_absolute.getStateAsString() << std::endl;
  // ... and block
  //usleep(100000); // 100 msec
  for (int count=0 ; count<30; count++) {
    this->blocker.Wait(1000);
    IPState state = this->focuser_absolute.getINDIState();
    std::cerr << "Focus = " << CurrentFocus()
	      << "absolute_focus.state = " << this->focuser_absolute.getStateAsString() << std::endl;
    if (state != IPS_BUSY) break;
    this->blocker.Setup();
  }

#if 0
    if (fabs(this->focuser_absolute.getValue() - target_point) < FOCUS_TOLERANCE) {
      break;
    }
  }
  } else {
    std::cerr << "DoFocus() starting point = "
	      << starting_point << std::endl;
    // FOCUSER_MOVE_RELATIVE
    // (smaller numbers mean move inward)
    if (msec > 0) {
      this->focuser_dir_in.setValue(ISS_OFF);
      this->focuser_dir_out.setValue(ISS_ON);
    } else {
      this->focuser_dir_in.setValue(ISS_ON);
      this->focuser_dir_out.setValue(ISS_OFF);
    }
    this->dev->local_client->sendNewSwitch(this->focuser_dir_in.property->indi_property);
    if (msec != 0) {
      this->blocker.Setup();
      this->focuser_relative.setValue(fabs((double) msec));
      this->dev->local_client->sendNewNumber(this->focuser_relative.property->indi_property);
      // ... and block
      for (int count=0; count<30; count++) {
	this->blocker.Wait(1000);
	std::cerr << "abs pos = "
		  << this->focuser_absolute.getValue()
		  << ", rel pos = "
		  << this->focuser_relative.getValue()
		  << std::endl;
	if (fabs(this->focuser_relative.getValue() - msec) < FOCUS_TOLERANCE) {
	  break;
	}
      }
    }
  }
#endif
  return this->CurrentFocus();
}

long
FOCUSER_INDI::CurrentFocus(void) {
  return (long) (0.5 + this->focuser_absolute.getValue());
}

void
FOCUSER_INDI::DoINDIRegistrations(void) {
  std::list<AstroValue *> local_vars =
    { &focuser_dir_in,
      &focuser_dir_out,
      &focuser_relative,
      &focuser_absolute,
      &focuser_maxlimit,
      &focuser_sync };

  for (AstroValue *av : local_vars) {
    this->dev->lookups.push_back(av);
  }
}
