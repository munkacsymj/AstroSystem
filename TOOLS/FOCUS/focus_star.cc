/*  focus_star.cc -- Find nearby star to use as focus target
 *
 *  Copyright (C) 2016 Mark J. Munkacsy
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
#include <stdio.h>
#include <gendefs.h>
#include <unistd.h>		// unlink()
#include <string.h>		// strcpy()
#include <Image.h>
#include <scope_api.h>
#include <camera_api.h>
#include <named_stars.h>
#include "focus_star.h"
#include <time.h>

struct FocusStar {
  DEC_RA location;
  char *name;
};

FocusStar *pick_focus_star(FILE *logfile) {
  // Pick the focus star that has an RA closest to the current scope's RA.
  DEC_RA where_now = ScopePointsAt();

  // Find all the predefined focus stars in the named star list.  We
  // go through the list, stopping when we see 4 in a row that are not
  // present. That allows for stars to be deleted if they aren't good
  // focus stars (e.g., double star).
  int focus_index = 0;
  int consecutive_skipped = 0;
  double closest_RA = 99999999.9;
  FocusStar *closest_star = 0;
  do {
    char focus_star_name[16];
    sprintf(focus_star_name, "focus%03d", focus_index);
    
    NamedStar star(focus_star_name);
    if (star.IsKnown()) {
      consecutive_skipped = 0; // reset the "skip" count
      FocusStar *f = new FocusStar;
      f->location = star.Location();
      f->name = strdup(focus_star_name);

      double delta_ra = fabs(where_now.ra() - f->location.ra());
      if (delta_ra < closest_RA) {
	closest_RA = delta_ra;
	if (closest_star) delete closest_star;
	closest_star = f;
      } else {
	delete f;
      }
    } else {
      consecutive_skipped++;
    }
    focus_index++;
  } while (consecutive_skipped < 5);
  return closest_star;
}

const char *
dark_name(double exposure_time_secs,
	  int num_exposures,
	  const char *session_dir) {
  char dark_command[256];

  fprintf(stderr, "Fetching dark(s).\n");

  sprintf(dark_command, COMMAND_DIR "/dark_manager -n %d -t %lf -d %s > /tmp/darkfilename",
	  num_exposures, exposure_time_secs, session_dir);
  if (system(dark_command) == -1) {
    fprintf(stderr, "focus_star: cannot invoke dark_manager\n");
  }

  FILE *fp = fopen("/tmp/darkfilename", "r");
  if (!fp) {
    perror("focus_star: unable to read /tmp/darkfilename:");
    return "";
  }
  char dark_filename[256];
  if (fgets(dark_filename, sizeof(dark_filename), fp)) {
    fprintf(stderr, "Dark is named %s\n", dark_filename);
    return strdup(dark_filename);
  } else {
    fprintf(stderr,
	    "focus_star: unable to get filename from /tmp/darkfilename\n");
    return "";
  }
}

const char *clean_gmt(void) {
  time_t clock_data;
  time(&clock_data);
  char *buffer = ctime(&clock_data);
  buffer[24] = 0; //clobber trailing '\n'
  return buffer;
}

Image *find_focus_star(bool no_auto_find,
		       FILE *logfile,
		       double exposure_time_val,
		       const char *session_dir) {
  Image *finder;
  int status = STATUS_OK;
  FocusStar *star = pick_focus_star(logfile);
  if (!star) {
    fprintf(logfile, "%s: pick_focus_star(): <nil>\n",
	    clean_gmt());
    // return <nil> since there's no star to try to focus on
    return 0;
  } else {
    fprintf(logfile, "%s: starting goto to %s\n",
	    clean_gmt(), star->name);
  }

  // we have a valid star. Search for it.
  MoveTo(&star->location, 0 /*don't encourage flip*/);
  WaitForGoToDone();
  sleep(30); // wait for mount to stabilize

  if (no_auto_find) return 0;

  const char *this_dark = dark_name(exposure_time_val, 1, session_dir);
  int move_tries = 0;
  int total_tries = 0;

  do {
    exposure_flags flags;
    flags.SetFilter(Filter("Vc"));
    
    const char *image_filename = expose_image_next(exposure_time_val, flags, "FOCUS_FIND");
    fprintf(logfile, "%s: finder exposure (%.1lf secs): %s\n",
	    clean_gmt(), exposure_time_val, image_filename);
    total_tries++;

    char parameter_filename[32];
    strcpy(parameter_filename, "/tmp/correlatef.XXXXXX");
    close(mkstemp(parameter_filename));
    
    {
      char command_buffer[256];
      sprintf(command_buffer, COMMAND_DIR "/find_stars -d %s -i %s",
	      this_dark, image_filename);
      fprintf(stderr, "executing: %s\n", command_buffer);
      if(system(command_buffer) == -1) {
	perror("Unable to execute find_stars command");
      } else {
	sprintf(command_buffer,
		COMMAND_DIR "/star_match -e -f -d %s -n %s -i %s -p %s",
		this_dark, star->name, image_filename, parameter_filename);
	if(system(command_buffer) == -1) {
	  perror("Unable to execute star_match command");
	}
	unlink(parameter_filename);
      }
    }

    finder = new Image(image_filename); // pick up new starlist
    DEC_RA current_center = finder->ImageCenter(status);
    if(status == STATUS_OK) {
      fprintf(logfile, "%s: Finder match successful.\n", clean_gmt());

      // good news: something worked!
      double delta_dec, delta_ra, delta_ra_arcsec; // both in radians

      delta_dec = star->location.dec() - current_center.dec();
      delta_ra  = star->location.ra_radians() - current_center.ra_radians();
      delta_ra_arcsec = delta_ra * cos(star->location.dec());
    
      fprintf(logfile, "Finder offset = %.1f (arcmin S), %.1f (arcmin W)\n",
	      delta_dec*60.0*180.0/M_PI,
	      delta_ra_arcsec*60.0*180.0/M_PI);

      if(fabs(delta_dec) < (4.5/60.0)*M_PI/180.0 &&
	 fabs(delta_ra_arcsec) < (4.5/60.0)*M_PI/180.0) {
	// good enough.
	goto finished;
      } else {
	char command_buffer[256];
	move_tries++;
	if(move_tries > 3) {
	  fprintf(logfile, "%s: didn't converge on proper location.\n",
		       star->name);
	  status = !STATUS_OK;
	  goto finished;
	}
	sprintf(command_buffer, COMMAND_DIR "/move %.1fN %.1fE",
		delta_dec * (180.0/M_PI) * 60.0,
		delta_ra_arcsec * (180.0/M_PI) * 60.0);
	fprintf(logfile, "Issuing move command: %s\n", command_buffer);
	if(system(command_buffer) == -1) {
	  perror("Unable to execute move command.");
	}
      }
    } else {
      // didn't work. Any stars seen?
      IStarList *list = finder->GetIStarList();
      if(list->NumStars == 0) {
	fprintf(logfile, "%s: Finder for %s: no stars seen.",
		clean_gmt(), star->name);
      } else if(list->NumStars <= 2) {
	// too few stars
	fprintf(logfile, "%s: Finder for %s: only %d stars seen.\n",
		clean_gmt(), star->name, list->NumStars);
      } else {
	// otherwise, it just couldn't find a match. Tough.
	fprintf(logfile, "%s: Finder for %s: couldn't match.\n",
		clean_gmt(), star->name);
      }

      // this dithering move is intended to allow a slightly different
      // starfield to be imaged; maybe we get more stars?
      fprintf(logfile, "%s: Issuing dithering move command.\n",
	      clean_gmt());
      if(system(COMMAND_DIR "/move 1.5N 1.5W") == -1) {
	perror("Unable to execute dithering move command.");
      }
    }
    delete finder;
  }  while(total_tries < 5); // should never trip on this

finished:
  return finder;
}
