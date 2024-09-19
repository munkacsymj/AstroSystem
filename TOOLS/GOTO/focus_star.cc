/*  focus_star.cc -- Executes "goto focus_star"
 *
 *  Copyright (C) 2007 Mark J. Munkacsy
 *
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
#include <unistd.h>		// pick up sleep(), getopt()
#include <stdlib.h>		// pick up atof()
#include <stdio.h>
#include <named_stars.h>
#include <bright_star.h>
#include <scope_api.h>

const static double MAG_MIN_THRESHOLD = 5.5;
const static double MAG_MAX_THRESHOLD = 4.5;
DEC_RA find_focus_star(void);

int main(int argc, char **argv) {
  DEC_RA commanded_pos = find_focus_star();
    
  printf("%s %s",
	 commanded_pos.string_dec_of(),
	 commanded_pos.string_ra_of());
  return 0;
}
  
DEC_RA find_focus_star(void) {

  connect_to_scope();

  DEC_RA OrigLocation = ScopePointsAt();

  for(double range=1.0; range < 15.5; range += 1.0) {
    const double range_radians = range * M_PI/180.0;
    const double delta_hours = range * (M_PI/180.0) / cos(OrigLocation.dec());
    double ra_min = OrigLocation.ra_radians() - delta_hours;
    double ra_max = OrigLocation.ra_radians() + delta_hours;

    if(ra_min < 0.0) ra_min += (2.0*M_PI);
    if(ra_max >= (2.0*M_PI)) ra_max -= (2.0*M_PI);

    BrightStarList Trial(OrigLocation.dec() + range_radians,
			 OrigLocation.dec() - range_radians,
			 ra_max,
			 ra_min,
			 MAG_MIN_THRESHOLD,
			 MAG_MAX_THRESHOLD);

    BrightStarIterator it(&Trial);

    for(OneBrightStar *h = it.First(); h; h = it.Next()) {
      fprintf(stderr, "Found focus star at mag %.1f ", h->Magnitude());
      if(h->Name()) {
	fprintf(stderr, "named '%s'\n", h->Name());
      } else {
	fprintf(stderr, "\n");
      }
      // Found a good one!!
      return h->Location();
    }
  }
  // Bad news. Nothing found.
  fprintf(stderr, "goto: no focus stars found.!?\n");
  exit(2);
}
