/*  alt_az.cc -- Altitude/Azimuth coordinate system
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
#include <stdio.h>
#include <iostream>
#include "alt_az.h"
#include "dec_ra.h"
#include <math.h>
#include <system_config.h>

// An "ALT_AZ" is an altitude/azimuth pair that locates something in
// the local sky at a specific time.  The only way (right now) to
// create an ALT_AZ location is to provide a DEC_RA and a time.  The
// only information you can get from an ALT_AZ is the altitude and the
// azimuth, both of which are provided in radians.  The calculation of
// ALT_AZ is obviously dependent upon your location on the earth.  An
// implied location is currently hard-coded; see the initialization of
// "hour_angle" and of "latitude".

static const double latitude = system_config.Latitude() * (M_PI/180.0);

ALT_AZ::ALT_AZ(const DEC_RA &loc, const JULIAN when) {
#if 0
  const double lst = 2.0 * M_PI / 24.0 * // kept in radians
    (when.days_since_jan_1() * (24.0/365.0) +
     // the "6.0hrs + 42min" in the following term establishes the
     // mean sidereal time at the prime meridian (GMST)
     when.hours_since_local_midnight() + (6.0 + 42.0/60.0));
#else
  const double lst = (M_PI/12.0)*SiderealTime(when);
#endif

  //fprintf(stderr, "lst(0) = %lf\n", lst*12.0/M_PI);

  const double hour_angle = lst - loc.ra_radians();
  //fprintf(stderr, "hour angle (rad) = %lf\n", hour_angle);

  const double cos_hour_angle = cos(hour_angle);

  const double sin_lat = sin(latitude);
  const double cos_lat = cos(latitude);

  azimuth = atan2(sin(hour_angle),
		  (cos_hour_angle * sin_lat -
		   tan(loc.dec()) * cos_lat));
  altitude = asin(sin_lat * sin(loc.dec()) +
		    cos_lat * cos(loc.dec()) * cos_hour_angle);
}

void
ALT_AZ::DEC_RA_of(const JULIAN when, DEC_RA &loc) {
  const double sin_lat = sin(latitude);
  const double cos_lat = cos(latitude);
  const double zenith_angle = M_PI/2.0 - altitude;
  const double cos_Z = cos(zenith_angle);
  const double sin_Z = sin(zenith_angle);
  const double cos_A = cos(azimuth);
  //const double sin_A = sin(azimuth);

  const double sin_dec = sin_lat * cos_Z - cos_A * cos_lat * sin_Z;
  const double dec = asin(sin_dec);
  const double cos_dec = cos(dec);
  const double cos_ha_numerator = cos_Z - sin_lat *sin_dec;
  const double cos_ha_denom = cos_lat*cos_dec;
  const double cos_ha = cos_ha_numerator/cos_ha_denom;
  double ha1 = acos(cos_ha);
  double ha2 = -ha1;

  DEC_RA ha1_dec_ra(dec, ha1, when);
  ALT_AZ alt_az1(ha1_dec_ra, when);
  DEC_RA ha2_dec_ra(dec, ha2, when);
  ALT_AZ alt_az2(ha2_dec_ra, when);

  if (fabs(alt_az1.azimuth_of() - this->azimuth_of()) <
      fabs(alt_az2.azimuth_of() - this->azimuth_of())) {
    loc = ha1_dec_ra;
  } else {
    loc = ha2_dec_ra;
  }
}
		     
double
ALT_AZ::airmass_of(void) const {
  const double altitude_deg = altitude_of() * 180.0 / M_PI;
  // Calculate and set airmass
  if (altitude_deg < 0.5) {
    return 99.9;
  }
  
  double airmass = 1.0/sin((M_PI/180.0)*(altitude_deg +
					 244.0/(165.0 + 47.0*pow(altitude_deg, 1.1))));
  return airmass;
}

std::string
ALT_AZ::ToString(void) {
  return std::to_string(this->altitude * 180.0 / M_PI) +
    ",  " + std::to_string(this->azimuth * 180.0 / M_PI);
}

