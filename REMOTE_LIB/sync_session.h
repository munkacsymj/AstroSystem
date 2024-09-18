// This may look like C code, but it is really -*- C++ -*-
/*  sync_session -- Implements mount pointing model
 *
 *  Copyright (C) 2017 Mark J. Munkacsy

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

#ifndef SYNC_SESSION_H
#define SYNC_SESSION_H

#include "dec_ra.h"
#include "julian.h"
#include <list>
#include <stdio.h>

//
// Above all else, remember the following convention:
// {residual error} = {obs position} - ({catalog pos} + {model adj})
//
// A negative value of Mel means the mount's axis points at a spot in
// the sky *above* the pole.
// A positive value of Maz means the mount's axis points at a spot to
// the east of the pole.

// Definitions:
//    - The mount knows only the coordinate system called {scope}
//    - True (catalog) positions are called {catalog}
//    - The difference between {scope} and {catalog} are called {delta}
//    - Imperfections in the mount model show up as {error}, which can
//    be measured in either the {scope} or {catalog} systems.
//    - The {mount parameters} describe how to calculate {delta}.
//    - Sync points capture pairs of {scope},{catalog} and are used to
//    calculate the {mount parameters}.

//****************************************************************
//        SyncPoint
//****************************************************************

// For the 10Micron GM2000, the values stored in here are the J2000
// coordinates. Must convert to Epoch of the Day prior to transmission
// to the mount.
class SyncPoint {
 public:
  double hour_angle_raw;	// what the mount thinks and reports
  double declination_raw;
  DEC_RA location_raw;
  double hour_angle_true;	// the actual location
  double declination_true;
  DEC_RA location_true;

  bool west_side_of_mount;
  bool flipped;
  
  JULIAN time_of_sync;
  char   sidereal_time_of_sync[24];
};

//****************************************************************
//        SyncSession
//****************************************************************
class SyncSession {
public:
  // create an empty sync_session
  SyncSession(void) { session_filename = 0; }
  SyncSession(const char *filename);
  ~SyncSession(void);

  void AddSyncPoint(SyncPoint *s) { all_sync_points.push_back(s); }
  void SaveSession(void);

  std::list<SyncPoint *> all_sync_points;

  const char *session_filename;
};
  
#endif
