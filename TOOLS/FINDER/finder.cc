/*  finder.cc -- Plate-solve and point the telescope at the desired spot
 *
 *  Copyright (C) 2023 Mark J. Munkacsy
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
#include <system_config.h>
#include "scope_api.h"
#include "camera_api.h"

void scope_error(char *response, ScopeResponseStatus Status) {
  const char *type = "";

  if(Status == Okay) type = "Okay";
  if(Status == TimeOut) type = "TimeOut";
  if(Status == Aborted) type = "Aborted";

  fprintf(stderr, "ERROR: %s, string = '%s'\n", type, response);
}

int main(int argc, char **argv) {
  double exposure_time = 10.0;
  int option_char;
  double sensitivity = 0.0;
  char *starname = 0;
  DEC_RA commanded_pos;
  while((option_char = getopt(argc, argv, "q:n:t:")) > 0) {
    switch (option_char) {
    case 'n':
      starname = optarg;
      break;

    case 'q':
      sensitivity = atof(optarg);
      break;

    case 't':
      exposure_time = atof(optarg);
      break;

    case '?':			// invalid argument
    default:
      fprintf(stderr, "Invalid argument.\n");
      exit(2);
    }
  }

  if(starname) {
    NamedStar named_star(starname);
    if(!named_star.IsKnown()) {
      fprintf(stderr, "Don't know of star named '%s'\n", starname);
      exit(2);
    }

    commanded_pos = named_star.Location();
  } else {
    fprintf(stderr, "Usage: finder -n starname [-t exp_time] [optional_offsets]\n");
    exit(2);
  }

  argc -= optind;
  argv += optind;
  if(argc >= 1) {
    double north_delta = 0.0;
    double east_delta = 0.0;
    while(argc) {
      argc--;
      char last_letter;
      const int len = strlen(argv[argc]);
      char *last_letter_ptr = argv[argc] + (len-1);

      last_letter = *last_letter_ptr;
      *last_letter_ptr = 0;

      double converted_value = atof(argv[argc]);

      fprintf(stderr, "letter = '%c', val=%.2f\n",
	      last_letter, converted_value);

      switch(last_letter) {
      case 'n':
      case 'N':
	north_delta = converted_value;
	break;

      case 's':
      case 'S':
	north_delta = -converted_value;
	break;

      case 'e':
      case 'E':
	east_delta = converted_value;
	break;

      case 'w':
      case 'W':
	east_delta = -converted_value;
	break;

      default:
	fprintf(stderr, "Motion must end with one of N, S, E, or W\n");
	exit(2);
      }
    }

    north_delta *= ((2.0 * M_PI)/360.0)/60.0;
    east_delta  *= ((2.0 * M_PI)/360.0)/60.0;
    commanded_pos.increment(north_delta, east_delta);
  }
  
  connect_to_scope();
  connect_to_camera();

  exposure_flags finder_flags("finder");
  int initial_tries = 0;	// counts failures to find anything
  int move_tries = 0;
  int status;
  DEC_RA current_center;
  static Filter Vc_Filter("Vc");
  bool initial_pointing_okay = false;
  DEC_RA raw_mount_points_at;
  const char *image_filename;

  finder_flags.SetFilter(Vc_Filter);

  MoveTo(&commanded_pos, 1 /*ENCOURAGE_FLIP*/);
  WaitForGoToDone();

  DEC_RA pointing_target = commanded_pos;

  // outer loop is trying to zero in on correct position
  // inner loop is just trying to get an image that we can correlate

  do { // loop counting by "move_tries"
    Image *finder = 0;
    
    do { // loop counting by "initial_tries"
      raw_mount_points_at = RawScopePointsAt();
      image_filename = expose_image_next(exposure_time, finder_flags, "FINDER");

      fprintf(stderr, "Finder for %s: %f secs: %s",
	      starname, exposure_time, image_filename);

      // need to do correlate() here.!!!
      char parameter_filename[32];
      strcpy(parameter_filename, "/tmp/correlate.XXXXXX");
      close(mkstemp(parameter_filename));
      {
	char command_buffer[256];
	char arg_buffer[64] {""};
	if (sensitivity > 0.0) {
	  sprintf(arg_buffer, " -q %.1lf ", sensitivity);
	}
	
	sprintf(command_buffer, COMMAND_DIR "/find_stars %s -i %s", arg_buffer, image_filename);
	if(system(command_buffer) == -1) {
	  perror("Unable to execute find_stars command");
	} else {
	  sprintf(command_buffer,
		  COMMAND_DIR "/star_match -h -e -f -n %s -i %s -p %s",
		  starname, image_filename, parameter_filename);
	  if(system(command_buffer) == -1) {
	    perror("Unable to execute star_match command");
	  }
	  unlink(parameter_filename);
	}
      }

      if (finder) delete finder;
      finder = new Image(image_filename); // pick up new starlist
      current_center = finder->ImageCenter(status);
      if(status == STATUS_OK) {
	// We add this location as the "true" spot that the scope is
	// pointing at into the mount error file being maintained for
	// this session.
	fprintf(stderr, "Finder match successful.");
	break;			// done trying for an image match
      }

      // didn't work. Any stars seen?
      IStarList *list = finder->GetIStarList();
      if(list->NumStars == 0) {
	fprintf(stderr, "Finder for %s: no stars seen.", starname);
	initial_tries++;
      } else if(list->NumStars <= 2) {
	// too few stars
	fprintf(stderr, "Finder for %s: only %d stars seen.", starname, list->NumStars);
	initial_tries++;
      } else {
	// otherwise, it just couldn't find a match. Tough.
	fprintf(stderr, "Finder for %s: couldn't match.", starname);
	initial_tries++;
      }
      // this dithering move is intended to allow a slightly different
      // starfield to be imaged; maybe we get more stars?
      fprintf(stderr, "Issuing dithering move command.");
      if(system(COMMAND_DIR "/move 1.5N 1.5W") == -1) {
	perror("Unable to execute dithering move command.");
      }
    } while (initial_tries < 3);

    if(status != STATUS_OK) {
      // couldn't even correlate the image; can't try to move
      break;
    }

    // Here's the plan:
    // The strategy specifies a center and a tolerance. We're going
    // to do the Finder() thing until that condition is
    // satisfied. Then we'll do the bad_pixel adjustment to the
    // target center, and repeat.

    bool force_move = false;
    double delta_dec, delta_ra, delta_ra_arcsec; // both in radians

    delta_dec = pointing_target.dec() - current_center.dec();
    delta_ra  = pointing_target.ra_radians() - current_center.ra_radians();
    delta_ra_arcsec = delta_ra * cos(pointing_target.dec());
      
    fprintf(stderr, "Finder offset = %.3f (arcmin S), %.3f (arcmin W)",
	    delta_dec*60.0*180.0/M_PI,
	    delta_ra_arcsec*60.0*180.0/M_PI);
    bool within_tolerance = (fabs(delta_dec) < (1.0/60.0)*M_PI/180.0 and
			     fabs(delta_ra_arcsec) < (1.0/60.0)*M_PI/180.0);
    if (!initial_pointing_okay) {
      if (within_tolerance) {
	initial_pointing_okay = true;
      } else {
	force_move = true;
      }
    }

    if (finder) {
      delete finder;
      finder = 0;
    }

    if(within_tolerance and not force_move) {
      // good enough.
      goto finished;
    } else {
      char command_buffer[256];
      move_tries++;
      if(move_tries > 3) {
	fprintf(stderr, "%s: didn't converge on proper location.", starname);
	status = !STATUS_OK;
	goto finished;
      }

      sprintf(command_buffer, COMMAND_DIR "/move %.3fN %.3fE",
	      delta_dec * (180.0/M_PI) * 60.0,
	      delta_ra_arcsec * (180.0/M_PI) * 60.0);
      fprintf(stderr, "Issuing move command: %s", command_buffer);
      if(system(command_buffer) == -1) {
	perror("Unable to execute move command.");
      }
    }
  }  while(move_tries < 4); // should never trip on this

finished:
  disconnect_camera();
  disconnect_scope();
  return 0;
}

