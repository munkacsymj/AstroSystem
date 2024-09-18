// This may look like C code, but it is really -*- C++ -*-
/*  dec_ra.h -- Declination/RightAscension coordinate system
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

#ifndef _DEC_RA_H
#define _DEC_RA_H

#include <math.h>
#include "julian.h"

// A DEC_RA is a location in the sky.  It is independent of observer's
// position and independent of time.  We support two constructors: one
// takes strings holding the declination and right ascension, the other
// creates a "null" location (I don't remember why this was needed.).
// The declination and right ascension strings are assumed to be in
// normal "degrees" and "hours" format, e.g., "-12:12.0" for
// declination and "02:16:12" for right ascension.  If the strings can
// be parsed successfully, "status" will be set to STATUS_OK.
// Otherwise, "status" will be set to "! STATUS_OK".

// Beware that the right ascension is NOT stored in radians -- it is
// in hours.  Sorry if you don't like that.  Although this made sense
// once upon a time, it no longer seems smart. (It's also private, so
// you shouldn't care.) 

const double DEGREES = (M_PI / 180.0);
const int STATUS_OK = 1;

class EPOCH {
public:
  EPOCH(JULIAN when);
  EPOCH(int Jyear) { epoch_ref = (double) Jyear; }; // e.g., EPOCH(2000);

  friend inline double YearsBetween(EPOCH E1, EPOCH e2);
  double YearsAfter2000(void);
private:
  double epoch_ref; // 2000.0 e.g.
};

EPOCH EpochOfToday(void);

inline double YearsBetween(EPOCH e1, EPOCH e2) {
  return e1.epoch_ref - e2.epoch_ref;
}

class DEC_RA {
public:
  DEC_RA(const char *dec_string, const char *ra_string, int &status);
  DEC_RA(void) { dr_dec = 0.0; dr_ra = 0.0; }
  DEC_RA(double dec_in_radians, double ra_in_radians) {
    dr_dec = dec_in_radians;
    dr_ra = ra_in_radians * 24.0 / (2.0 * M_PI); }
  // Use this when you know time and hour angle
  DEC_RA(double dec_in_radians, double ha_in_radians, JULIAN when);

  // We also support querying a DEC_RA to get back strings holding the
  // declination and the right ascension in "normal" format (e.g.,
  // "-12:11.0" and "03:17:22").
  char *string_dec_of(void) const;
  char *string_ra_of(void) const;
  char *string_longra_of(void) const; // prints a decimal point for seconds

  // We also support getting declination as -DD:MM:SS (we call it
  // "long" format)
  char *string_longdec_of(void) const; // contains funny degree symbol
  char *string_fulldec_of(void) const; // contains : instead of deg symbol

  double dec(void) const { return dr_dec; } // radians
  double ra(void)  const { return dr_ra; }  // hours (always positive)
  double ra_radians(void) const { return dr_ra * (2.0 * M_PI / 24.0); }

  void increment(double delta_dec, // in radians
		 double delta_ra); // in radians

  // hour angle is in radians, and has been normalized into the range
  // -pi..+pi
  double hour_angle(const JULIAN when) const;

  void normalize(void);

private:
  double dr_dec;		// declination in radians
  double dr_ra;			// right ascension in hours
};

// converting across Epochs; dec_ra is assumed to be in EPOCH "from"
// and we will return a DEC_RA in EPOCH "to" 
DEC_RA ToEpoch(DEC_RA &dec_ra, EPOCH from, EPOCH to);

double SiderealTime(const JULIAN when);
#endif
