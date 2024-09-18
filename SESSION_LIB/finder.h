// This may look like C code, but it is really -*- C++ -*-
/*  finder.h -- manages the framing of a field
 *
 *  Copyright (C) 2020 Mark J. Munkacsy

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
#ifndef _FINDER_H
#define _FINDER_H
#include <stdio.h>
#include <julian.h>
#include <dec_ra.h>
#include <Image.h>
#include <list>

class Session;			// forward declaration
class Strategy;

typedef bool FinderResult;
#define FINDER_OKAY true

class Finder {
public:
  Finder(Strategy *strategy, Session *session=nullptr);
  ~Finder(void);

  //********************************
  //        SETUP
  //********************************
  void SetBadPixelAvoidance(bool turn_on);

  //********************************
  //        EXECUTE
  //********************************
  FinderResult Execute(void);

  //********************************
  //        Results
  //********************************
  const char *final_imagename(void) { return finder_imagename; }
  const DEC_RA &final_pointing(void);

private:
  Strategy *f_strategy;
  Session *f_session;

  // setup parameters
  double exposure_time;
  DEC_RA target_location;	// object location + offset
  DEC_RA pointing_target;	// target_location + bad_pixel_offset
  bool avoid_bad_pixels{false};
  double offset_tolerance; // radians, both axes

  // result information
  char *finder_imagename{nullptr};
  DEC_RA final_position;

  // Helpers
  void SlewToTarget(void); // sets up target_location
};

#endif
