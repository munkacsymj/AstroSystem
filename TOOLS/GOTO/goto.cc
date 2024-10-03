/*  goto.cc -- Main program to move mount to a specific spot
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
#include <string.h>
#include <stdio.h>
#include <named_stars.h>
#include <bright_star.h>
#include "scope_api.h"

static void Terminate(void) {
  disconnect_scope();
  exit(-2);
}

void scope_error(char *response, ScopeResponseStatus Status) {
  const char *type = "";

  if(Status == Okay) type = "Okay";
  if(Status == TimeOut) type = "TimeOut";
  if(Status == Aborted) type = "Aborted";

  fprintf(stderr, "ERROR: %s, string = '%s'\n", type, response);
}

// 20 minutes of RA correction (*not* arc-minutes, but not RA minutes,
// either; you go figure!)
const static double RA_OVERSHOOT_RADS = (20.0/60.0)*(M_PI/180.0);
const static double DEC_OVERSHOOT_RADS = (20.0/60.0)*(M_PI/180.0);

const static double MAG_MIN_THRESHOLD = 5.5;
const static double MAG_MAX_THRESHOLD = 4.5;
DEC_RA find_focus_star(void);

static int scope_connected = 0;

int main(int argc, char **argv) {
  int conversion_status;
  int option_char;
  char *starname = 0;
  int hysteresis = 0;		// if set to 1, will make sure that we
				// always approach the commanded
				// position from the same direction
				// (to avoid position slop)

  DEC_RA commanded_pos;

  if(argc == 2 &&
     (strcmp(argv[1], "focus_star") == 0 ||
      strcmp(argv[1], "focus-star") == 0 ||
      strcmp(argv[1], "focusstar") == 0 ||
      strcmp(argv[1], "focustar") == 0)) {
    commanded_pos = find_focus_star();
  } else {
    while((option_char = getopt(argc, argv, "hn:")) > 0) {
      switch (option_char) {
      case 'n':
	starname = optarg;
	break;

      case 'h':
	hysteresis = 1;
	break;
      
      case '?':			// invalid argument
      default:
	fprintf(stderr, "Invalid argument.\n");
	Terminate();
      }
    }

    if(starname) {
      NamedStar named_star(starname);
      if(!named_star.IsKnown()) {
	fprintf(stderr, "Don't know of star named '%s'\n", starname);
	Terminate();
      }

      commanded_pos = named_star.Location();
    } else {
      if(argc != 3) {
	fprintf(stderr, "usage: goto -dd:mm.m hh:mm:ss\n");
	Terminate();
      }
      commanded_pos = DEC_RA(argv[1], argv[2], conversion_status);
      if(conversion_status != STATUS_OK) {
	fprintf(stderr, "goto: arguments wouldn't parse.\n");
	Terminate();
      }
    }
  }

  if(!scope_connected) connect_to_scope();
  DEC_RA true_commanded_pos = commanded_pos;
  if(hysteresis) {
    double commanded_ra = commanded_pos.ra_radians();
    commanded_ra += RA_OVERSHOOT_RADS;

    double commanded_dec = commanded_pos.dec();
    commanded_dec += DEC_OVERSHOOT_RADS;

    commanded_pos = DEC_RA(commanded_dec, commanded_ra);
  }
  MoveTo(&commanded_pos);

  DEC_RA final_pos;
  DEC_RA last_pos;

  // wait for the slew to finish
  WaitForGoToDone();

  if(hysteresis) {
    sleep(5);
    commanded_pos = true_commanded_pos;
    MoveTo(&commanded_pos);

    WaitForGoToDone();
  }
  sleep(3);
    
  final_pos = ScopePointsAt();
  printf("Final scope position:\nRA= %s\nDEC= %s\n",
	 final_pos.string_ra_of(),
	 final_pos.string_dec_of());
  disconnect_scope();
  return 0;
}
  
DEC_RA find_focus_star(void) {

  connect_to_scope();
  scope_connected = 1;

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
  Terminate();
  /*NOTREACHED*/
  return DEC_RA(0,0);
}
