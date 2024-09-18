/*  finder.cc -- manages the framing of a field
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

#include "finder.h"
#include "session.h"
#include "strategy.h"
#include <camera_api.h>
#include <scope_api.h>
#include <bad_pixels.h>
#include <dec_ra.h>
#include <julian.h>
#include <Image.h>
#include <system_config.h>

#include <unistd.h>		// sleep()

//****************************************************************
//        Constructor & Destructor
//****************************************************************

Finder::Finder(Strategy *strategy, Session *session) {
  f_strategy = strategy;
  f_session = session;
  //exposure_time = strategy->GetFinderExposureTime();
  exposure_time = 10.0;
  fprintf(stderr, "Finder::Finder() constructor finished. avoid_bad_pixels = %d\n",
	  avoid_bad_pixels);
}

Finder::~Finder(void) {
  if(finder_imagename) free(finder_imagename);
}

//****************************************************************
//        Setup
//****************************************************************

void
Finder::SetBadPixelAvoidance(bool turn_on) {
  avoid_bad_pixels = turn_on;
}

//****************************************************************
//        Execute
//****************************************************************

FinderResult
Finder::Execute(void) {
  exposure_flags finder_flags("finder");
  int initial_tries = 0;	// counts failures to find anything
  int move_tries = 0;
  int status;
  DEC_RA current_center;
  static Filter Vc_Filter("Vc");
  BadPixels bp;
  bool bad_pixel_adjust_completed = false;
  bool initial_pointing_okay = false;
  double sidereal_time_start;	// sidereal time measured in radians
  double sidereal_time_end;
  DEC_RA raw_mount_points_at;
  const char *image_filename;

  SystemConfig config;
  const bool camera_ST9 = config.IsST9();

  finder_flags.SetFilter(Vc_Filter);

  SlewToTarget();
  pointing_target = target_location;

  // outer loop is trying to zero in on correct position
  // inner loop is just trying to get an image that we can correlate

  if (camera_ST9) {
    finder_flags.subframe.box_bottom = 0;
    finder_flags.subframe.box_top = 511;
    finder_flags.subframe.box_left = 0;
    finder_flags.subframe.box_right = 511;
  }
  
  do { // loop counting by "move_tries"
    Image *finder = 0;
    
    do { // loop counting by "initial_tries"
      sidereal_time_start = GetSiderealTime();
      raw_mount_points_at = RawScopePointsAt();
      image_filename = expose_image_next(exposure_time, finder_flags, "FINDER");

      fprintf(stderr, "Finder for %s: %s\n",
	      f_strategy->object(), image_filename);
      sidereal_time_end = GetSiderealTime();
      f_session->log(LOG_INFO, "Finder for %s: %f secs: %s",
		   f_strategy->object(), exposure_time, image_filename);
    
      const char *this_dark = f_session->dark_name(exposure_time, 1, false);

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
		  this_dark, f_strategy->object(), image_filename, parameter_filename);
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
	f_session->log(LOG_INFO, "Finder match successful.");
	break;			// done trying for an image match
      }

      // didn't work. Any stars seen?
      IStarList *list = finder->GetIStarList();
      if(list->NumStars == 0) {
	f_session->log(LOG_ERROR, "Finder for %s: no stars seen.", f_strategy->object());
	initial_tries++;
      } else if(list->NumStars <= 2) {
	// too few stars
	f_session->log(LOG_ERROR, "Finder for %s: only %d stars seen.",
		       f_strategy->object(), list->NumStars);
	initial_tries++;
      } else {
	// otherwise, it just couldn't find a match. Tough.
	f_session->log(LOG_ERROR, "Finder for %s: couldn't match.",
		       f_strategy->object());
	initial_tries++;
      }
      // this dithering move is intended to allow a slightly different
      // starfield to be imaged; maybe we get more stars?
      f_session->log(LOG_INFO, "Issuing dithering move command.");
      if(system(COMMAND_DIR "/move 1.5N 1.5W") == -1) {
	perror("Unable to execute dithering move command.");
      }
      // sleep(20);		// wait for mount to settle
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
      
    f_session->log(LOG_INFO, "Finder offset = %.3f (arcmin S), %.3f (arcmin W)",
		   delta_dec*60.0*180.0/M_PI,
		   delta_ra_arcsec*60.0*180.0/M_PI);
    bool within_tolerance = (fabs(delta_dec) < f_strategy->GetOffsetTolerance() &&
			     fabs(delta_ra_arcsec) < f_strategy->GetOffsetTolerance());
    if (!initial_pointing_okay) {
      if (within_tolerance) {
	initial_pointing_okay = true;
      } else {
	force_move = true;
      }
    }

    if (avoid_bad_pixels and initial_pointing_okay and !bad_pixel_adjust_completed) {
      f_session->log(LOG_INFO, "Starting bad pixel avoidance.");
      bad_pixel_adjust_completed = true;
      pointing_target = bp.UpdateTargetForBadPixels(finder, f_strategy->object());
      // update the delta's based on the new target position...
      delta_dec = pointing_target.dec() - current_center.dec();
      delta_ra  = pointing_target.ra_radians() - current_center.ra_radians();
      delta_ra_arcsec = delta_ra * cos(pointing_target.dec());
      force_move = true;
    }
    if (finder) {
      delete finder;
      finder = 0;
    }

    if(within_tolerance and force_move == false) {
      // good enough.
      goto finished;
    } else {
      char command_buffer[256];
      move_tries++;
      if(move_tries > 3) {
	f_session->log(LOG_ERROR,
		       "%s: didn't converge on proper location.",
		       f_strategy->object());
	status = !STATUS_OK;
	goto finished;
      }

      sprintf(command_buffer, COMMAND_DIR "/move %.3fN %.3fE",
	      delta_dec * (180.0/M_PI) * 60.0,
	      delta_ra_arcsec * (180.0/M_PI) * 60.0);
      f_session->log(LOG_INFO, "Issuing move command: %s",
		     command_buffer);
      if(system(command_buffer) == -1) {
	perror("Unable to execute move command.");
      }
      // sleep(20);		// wait for mount to settle
    }
  }  while(move_tries < 4); // should never trip on this

finished:
  if(finder_imagename) free(finder_imagename);
  finder_imagename = strdup(image_filename);

  if(status == STATUS_OK) {
    // see if we should update the mount model
    if (f_session->GetOptions()->update_mount_model) {
      // Data that we need:
      //    1. Sidereal time at exposure midpoint (sidereal_time_end,
      //    sidereal_time_start)
      //    2. Mount-reported dec & ra (raw_mount_points_at) (current epoch)
      //    3. Mount-reported side (east/west, fetched below)
      //    4. Plate-solved image center (current_center)
      //    (current_center is in J2000 coordinates, needs to change
      //    to current epoch)
      bool scope_on_west = scope_on_west_side_of_pier();
      char alignpoint_buffer[256];
      EPOCH j2000(2000);
      DEC_RA true_plate_center = ToEpoch(current_center, j2000, EpochOfToday());
      const double mra_raw = raw_mount_points_at.ra();
      const int mra_hours = (int) mra_raw;
      const int mra_mins = (int) ((mra_raw - mra_hours)*60.0);
      const double mra_secs = 3600.0*(mra_raw - mra_hours - mra_mins/60.0);
      const double mdec_raw = fabs(raw_mount_points_at.dec()*180.0/M_PI);
      const int mdec_deg = (int) mdec_raw;
      const int mdec_mins = (int) ((mdec_raw - mdec_deg)*60.0);
      const double mdec_secs = 3600.0*(mdec_raw - mdec_deg - mdec_mins/60.0);
      const bool mdec_negative = (raw_mount_points_at.dec() < 0.0);
      const double pra_raw = true_plate_center.ra();
      const int pra_hours = (int) pra_raw;
      const int pra_mins = (int) ((pra_raw - pra_hours)*60.0);
      const double pra_secs = 3600.0*(pra_raw - pra_hours - pra_mins/60.0);
      const double pdec_raw = fabs(true_plate_center.dec()*180.0/M_PI);
      const int pdec_deg = (int) pdec_raw;
      const int pdec_mins = (int) ((pdec_raw - pdec_deg)*60.0);
      const double pdec_secs = 3600.0*(pdec_raw - pdec_deg - pdec_mins/60.0);
      const bool pdec_negative = (true_plate_center.dec() < 0.0);

      const double st_raw = (12.0/M_PI)*(sidereal_time_end + sidereal_time_start)/2.0;
      const int st_hours = (int) st_raw;
      const int st_mins = (int) ((st_raw - st_hours)*60.0);
      const double st_secs = 3600.0*(st_raw - st_hours - st_mins/60.0);

      sprintf(alignpoint_buffer,
	      "%02d:%02d:%04.1lf,%c%02d:%02d:%02.0lf,%c,%02d:%02d:%04.1lf,%c%02d:%02d:%02.0lf,%02d:%02d:%04.1lf",
	      mra_hours, mra_mins, mra_secs,
	      (mdec_negative ? '-' : '+'),
	      mdec_deg, mdec_mins, mdec_secs,
	      (scope_on_west ? 'W' : 'E'),
	      pra_hours, pra_mins, pra_secs,
	      (pdec_negative ? '-' : '+'),
	      pdec_deg, pdec_mins, pdec_secs,
	      st_hours, st_mins, st_secs);

      char align_points_filename[64];
      sprintf(align_points_filename, "%s/%s",
	      f_session->Session_Directory(),
	      "align_points.txt");
      FILE *fp = fopen(align_points_filename, "a");
      if (fp) {
	fprintf(fp, "%s\n", alignpoint_buffer);
	fclose(fp);
	f_session->log(LOG_INFO, "Adding point to align sync point file.\n");
      } else {
	f_session->log(LOG_INFO, "Cannot open align_points.txt to add point.\n");
      }
    }

    return FINDER_OKAY;
  } // end if status == OKAY
  return not FINDER_OKAY;
}

//****************************************************************
// slew to the object
//****************************************************************
void
Finder::SlewToTarget(void) {
  const double cos_dec = cos(f_strategy->GetObjectLocation().dec());
  target_location = DEC_RA(f_strategy->GetObjectLocation().dec() + f_strategy->GetOffsetNorth(),
			   f_strategy->GetObjectLocation().ra_radians() +
			   f_strategy->GetOffsetEast()/cos_dec);
  JULIAN now(time(0));
  ALT_AZ where(target_location, now);

  if(f_session) {
    f_session->log(LOG_INFO, "%s alt/az = ( %.0f, %.0f )",
		   f_strategy->object(),
		   where.altitude_of()*180.0/M_PI,
		   where.azimuth_of()*180.0/M_PI);

    f_session->log(LOG_INFO, "Slewing to DEC=%s, RA=%s",
		   target_location.string_dec_of(),
		   target_location.string_ra_of());
  }
  // encourage a meridian flip as part of this goto
  MoveTo(&target_location, 1 /*ENCOURAGE_FLIP*/);
  WaitForGoToDone();
  sleep(30);
}

