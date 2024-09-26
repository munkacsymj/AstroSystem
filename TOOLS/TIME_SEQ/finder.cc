/*  finder.cc -- Perform precise pointing of telescope
 *
 *  Copyright (C) 2019 Mark J. Munkacsy
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
#include <scope_api.h>
#include <camera_api.h>
#include <string.h>		// strcpy()
#include <Image.h>
#include <IStarList.h>
#include <HGSC.h>
#include <stdio.h>
#include <gendefs.h>
#include <stdlib.h>		// system(), mkstemp()
#include <unistd.h>		// unlink()
#include "finder.h"
#include <system_config.h>

const char *get_darkfilename(double how_long);

#define finder_exposure_time 20 /*secs*/

int
Finder(const char *object_name,
       DEC_RA &target_location,
       double tolerance /*arc-sec*/,
       Filter &filter) {  
  exposure_flags finder_flags("finder");
  char *image_filename;
  double exposure_time = finder_exposure_time;
  int initial_tries = 0;	// counts failures to find anything
  int move_tries = 0;
  int status;
  DEC_RA current_center;

  SystemConfig config;
  //const bool camera_ST9 = config.IsST9();

  finder_flags.SetFilter(filter);

  // outer loop is trying to zero in on correct position
  // inner loop is just trying to get an image that we can correlate

  do { // loop counting by "move_tries"
    initial_tries = 0;
    do { // loop counting by "initial_tries"
      image_filename = expose_image_next(exposure_time, finder_flags, "FINDER");
      fprintf(stderr, "Finder: %f secs: %s",
	      exposure_time, image_filename);
    
      const char *this_dark = get_darkfilename(exposure_time);

      // need to do correlate() here.!!!
      char parameter_filename[32];
      strcpy(parameter_filename, "/tmp/correlate.XXXXXX");
      close(mkstemp(parameter_filename));
      {
	char command_buffer[256];
	sprintf(command_buffer, COMMAND_DIR "/find_stars -d %s -i %s",
		this_dark, image_filename);
	if(system(command_buffer) == -1) {
	  perror("Unable to execute find_stars command");
	} else {
	  sprintf(command_buffer,
		  COMMAND_DIR "/star_match -h -e -f -d %s -n %s -i %s -p %s",
		  this_dark, object_name, image_filename, parameter_filename);
	  if(system(command_buffer) == -1) {
	    perror("Unable to execute star_match command");
	  }
	  unlink(parameter_filename);
	}
      }

      Image finder(image_filename); // pick up new starlist
      current_center = finder.ImageCenter(status);
      if(status == STATUS_OK) {
	// We add this location as the "true" spot that the scope is
	// pointing at into the mount error file being maintained for
	// this session.
	fprintf(stderr, "Finder match successful.\n");
	break;			// done trying for an image match
      }

      // didn't work. Any stars seen?
      IStarList *list = finder.GetIStarList();
      if(list->NumStars == 0) {
	fprintf(stderr, "Finder for %s: no stars seen.\n", object_name);
	initial_tries++;
      } else if(list->NumStars <= 2) {
	// too few stars
	fprintf(stderr, "Finder for %s: only %d stars seen.\n",
		object_name, list->NumStars);
	initial_tries++;
      } else {
	// otherwise, it just couldn't find a match. Tough.
	fprintf(stderr, "Finder: couldn't match.\n");
	initial_tries++;
      }

      // this dithering move is intended to allow a slightly different
      // starfield to be imaged; maybe we get more stars?
      fprintf(stderr, "Issuing dithering move command.\n");
      if(system(COMMAND_DIR "/move 1.5N 1.5W") == -1) {
	perror("Unable to execute dithering move command.");
      }
      // sleep(20);		// wait for mount to settle
    } while (initial_tries < 3);

    if(status != STATUS_OK) {
      // couldn't even correlate the image; can't try to move
      break;
    }
    // good news: something worked!
    double delta_dec, delta_ra, delta_ra_arcsec; // both in radians

    delta_dec = target_location.dec() - current_center.dec();
    delta_ra  = target_location.ra_radians() - current_center.ra_radians();
    delta_ra_arcsec = delta_ra * cos(target_location.dec());
    
    fprintf(stderr, "Finder offset = %.1f (arcmin S), %.1f (arcmin W)",
	    delta_dec*60.0*180.0/M_PI,
	    delta_ra_arcsec*60.0*180.0/M_PI);

    if(fabs(delta_dec) < tolerance &&
       fabs(delta_ra_arcsec) < tolerance) {
      // good enough.
      goto finished;
    } else {
      char command_buffer[256];
      move_tries++;
      if(move_tries > 3) {
	fprintf(stderr, "Didn't converge on proper location.\n");
	status = !STATUS_OK;
	goto finished;
      }
      if(delta_ra_arcsec > 0.0) {
	sprintf(command_buffer, COMMAND_DIR "/move %.1fN %.1fE",
		delta_dec * (180.0/M_PI) * 60.0,
		delta_ra_arcsec * (180.0/M_PI) * 60.0);
      } else {
	sprintf(command_buffer, COMMAND_DIR "/move %.1fN %.1fE",
		delta_dec * (180.0/M_PI) * 60.0,
		delta_ra_arcsec * (180.0/M_PI) * 60.0);
      }
      fprintf(stderr, "Issuing move command: %s", command_buffer);
      if(system(command_buffer) == -1) {
	perror("Unable to execute move command.");
      }
      // sleep(20);		// wait for mount to settle
    }
  }  while(move_tries < 4); // should never trip on this

finished:

  return (status == STATUS_OK);
}
