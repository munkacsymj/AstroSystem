/*  refraction.cc -- Refraction adjustments
 *
 *  Copyright (C) 2015 Mark J. Munkacsy

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
#include <math.h>
#include "refraction.h"
#include "alt_az.h"

static double RefractionTempDegK = 288.0;
static double RefractionPressMillibar = 1010.0;
static double latitude = (41.568795 * M_PI / 180.0);

// zenith angle measures distance from the zenith in radians (0.0, at
// the zenith, will give a refraction error of 0.0 radians). This
// function returns an angle in radians.
double refraction_adjustment(double zenith_angle) {
  double z_deg = 180.0 * zenith_angle / M_PI;

  const double env_term = RefractionPressMillibar/RefractionTempDegK;
  const double arcsin_term = (180.0/M_PI)*asin(0.9986047*sin(0.9967614*zenith_angle));
  const double result = env_term *
    (3.430289 * (z_deg - arcsin_term) - 0.01115929*z_deg);
  return (M_PI/180.0) * (result/60.0);
}

static void refraction_delta(DEC_RA loc, JULIAN when,
			     double &delta_dec,
			     double &delta_ra) {
  const double ha = loc.hour_angle(when);
  ALT_AZ loc_alt_az(loc, when);
  double z = M_PI/2.0 - loc_alt_az.altitude_of();
  double R = refraction_adjustment(z);
  const double cos_dec = cos(loc.dec());
  const double tan_dec = tan(loc.dec());
  const double tan_lat = tan(latitude);
  const double denom = tan(z)*(tan_dec*tan_lat + cos(ha));

  delta_ra = R*(sin(ha)/(cos_dec*cos_dec*denom));
  delta_dec = R*(tan_lat - tan_dec*cos(ha))/denom;
}

void refraction_true_to_obs(DEC_RA true_loc, DEC_RA &obs_loc, JULIAN when) {
  double delta_dec, delta_ra;
  refraction_delta(true_loc, when, delta_dec, delta_ra);
  obs_loc = true_loc;
  obs_loc.increment(delta_dec, delta_ra);
}
  
void refraction_obs_to_true(DEC_RA obs_loc, DEC_RA &true_loc, JULIAN when) {
  double delta_dec, delta_ra;
  refraction_delta(obs_loc, when, delta_dec, delta_ra);
  true_loc = obs_loc;
  true_loc.increment(-delta_dec, -delta_ra);
}

//****************************************************************
//        test_refraction
// (Compare the calculated value of refraction angle against the
//  table of refractions found in Norton's Star Atlas.)
//****************************************************************

// [Note that the test_angles are elevation angles, not zenith
// angles. This gets corrected below.]
static double test_angles[] = {80.0, 65.0, 50.0, 40.0, 30.0, 20.0, 10.0 };
static double norton_refract[] = { 10.0/60.0, // 10 arcsec at 80-deg
				   27.0/60.0, // 27 arcsec at 65-deg
				   48.0/60.0, // 48 arcsec at 50-deg
				   69.0/60.0, // 1'9" at 40-deg
				   100.0/60.0, // 1'40" at 30-deg
				   (120+37)/60.0, // 2'37" at 20-deg
				   (300+16)/60.0, // 5'16" at 10-deg
};

void test_refraction(void) {
  const int NUM_TESTS = sizeof(test_angles)/sizeof(test_angles[0]);

  fprintf(stderr, "Zenith angle (rad)     Refraction Norton (arcmin)  Refraction calc\n");
  for (int i=0; i< NUM_TESTS; i++) {
    double z = (90.0 - test_angles[i])*M_PI/180.0;
    double R = refraction_adjustment(z);
    fprintf(stderr, "   %lf                   %lf               %lf\n",
	    z, norton_refract[i], R*(180.0/M_PI)*60.0);
  }

  const double zenith_angle = (90.0-25.0)*M_PI/180.0;
  const double R = refraction_adjustment(zenith_angle);
  fprintf(stderr, "In both of the next pairs, the second spot should\n");
  fprintf(stderr, "be %.2lf minutes north of the first spot.\n",
	  R*(180.0/M_PI)*60.0);
  ALT_AZ ref_pos(25.0*M_PI/180.0, 0.0);
  DEC_RA ref_dec_ra;
  JULIAN ref_now(time(0));
  ref_pos.DEC_RA_of(ref_now, ref_dec_ra);
  DEC_RA refracted_dec_ra;
  refraction_true_to_obs(ref_dec_ra, refracted_dec_ra, ref_now);
  fprintf(stderr, "[%s, %s]",
	  ref_dec_ra.string_dec_of(), ref_dec_ra.string_ra_of());
  fprintf(stderr, " south of [%s, %s]\n",
	  refracted_dec_ra.string_dec_of(), refracted_dec_ra.string_ra_of());

  refraction_obs_to_true(refracted_dec_ra, ref_dec_ra, ref_now);
  fprintf(stderr, "[%s, %s]",
	  ref_dec_ra.string_dec_of(), ref_dec_ra.string_ra_of());
  fprintf(stderr, " south of [%s, %s]\n",
	  refracted_dec_ra.string_dec_of(), refracted_dec_ra.string_ra_of());

  ref_pos = ALT_AZ(25.0*M_PI/180.0, M_PI/2.0);
  ALT_AZ obs_pos = ALT_AZ(R+25.0*M_PI/180.0, M_PI/2.0);
  ref_pos.DEC_RA_of(ref_now, ref_dec_ra);
  fprintf(stderr, "Next:\nTrue position is [%s, %s]\n",
	  ref_dec_ra.string_dec_of(), ref_dec_ra.string_ra_of());
  fprintf(stderr,
	  "Two different estimates of the refracted (apparent) position:\n");
  obs_pos.DEC_RA_of(ref_now, refracted_dec_ra);
  fprintf(stderr, "    Apparent position should be [%s, %s]\n",
	  refracted_dec_ra.string_dec_of(), refracted_dec_ra.string_ra_of());
  refraction_true_to_obs(ref_dec_ra, refracted_dec_ra, ref_now);
  fprintf(stderr, "    Calculated position is [%s, %s]\n",
	  refracted_dec_ra.string_dec_of(), refracted_dec_ra.string_ra_of());
}
