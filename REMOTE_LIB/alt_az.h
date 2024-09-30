/* This may look like C, but it's really -*-c++-*- */
/*  alt_az.h -- Altitude/Azimuth coordinate system
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
#ifndef _ALT_AZ_H
#define _ALT_AZ_H

#include "dec_ra.h"
#include "julian.h"
#include <string>

// An "ALT_AZ" is an altitude/azimuth pair that locates something in
// the local sky at a specific time.  The only way (right now) to
// create an ALT_AZ location is to provide a DEC_RA and a time.  The
// only information you can get from an ALT_AZ is the altitude and the
// azimuth, both of which are provided in radians.  The calculation of
// ALT_AZ is obviously dependent upon your location on the earth.  An
// implied location is currently hard-coded into alt_az.cc.

class ALT_AZ {
 public:
  ALT_AZ(double alt_radians, double az_radians) {
    altitude = alt_radians;
    azimuth = az_radians;
  }
  ALT_AZ(void) { altitude = 0.0; azimuth = 0.0; }
  ALT_AZ(const DEC_RA &loc, const JULIAN when);
  double altitude_of(void) const { return altitude; }
  double azimuth_of(void)  const { return azimuth; }
  void DEC_RA_of(const JULIAN when, DEC_RA &loc);

  double airmass_of(void) const;
 private:
  double altitude;		// altitude in radians
  double azimuth;		// azimuth in radians, S=0, W=+, E=-
  std::string ToString(void);
};

#endif
