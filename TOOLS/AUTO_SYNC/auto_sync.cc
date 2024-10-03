/*  auto_sync.cc -- Automatically build a mount model
 *
 *  Copyright (C) 2017, 2020 Mark J. Munkacsy

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

#include <unistd.h>		// getopt()
#include <string.h>		// strcmp
#include <ctype.h>		// isspace()
#include <stdio.h>
#include "bright_star.h"
#include "alt_az.h"
#include "visibility.h"
#include "scope_api.h"
#include "camera_api.h"
#include <list>
#include <stdlib.h>		// random()

using namespace std;

char *last_image_filename;
double exposure_time = 20.0;
char *darkfilename = 0;
char *align_points_filename = 0;

//****************************************************************
//        base_align_stars
//****************************************************************

// "Base" stars are not really stars. They are arbitrarily-selected
// spots in the sky for which there is a "catalog" file. The center of
// the field is given the name of a "fake star" (such as "align_C).

struct BaseAlignStar {
  const char *lookup_name;
  DEC_RA location;
  int    ra_band;
  int    dec_band;
  bool   visible;
  int    on_west_side;
  ALT_AZ loc_alt_az;
};

std::list<BaseAlignStar *> all_base_stars;

struct BaseStarlist {
  const char *lookup_name;
  DEC_RA location;
  bool visible;
  bool imaged;
  bool solved;
} base_catalog[] = {
  // +15-deg band
  { "align_A", DEC_RA(15.0*M_PI/180.0, (2.0/12.0)*M_PI)},
  { "align_B", DEC_RA(15.0*M_PI/180.0, (6.0/12.0)*M_PI)},
  { "align_C", DEC_RA(15.0*M_PI/180.0, (10.0/12.0)*M_PI)},
  { "align_D", DEC_RA(15.0*M_PI/180.0, (14.0/12.0)*M_PI)},
  { "align_E", DEC_RA(15.0*M_PI/180.0, (18.0/12.0)*M_PI)},
  { "align_F", DEC_RA(15.0*M_PI/180.0, (22.0/12.0)*M_PI)},
  { "align_G", DEC_RA(15.0*M_PI/180.0, (4.0/12.0)*M_PI)},
  { "align_H", DEC_RA(15.0*M_PI/180.0, (8.0/12.0)*M_PI)},
  { "align_I", DEC_RA(15.0*M_PI/180.0, (12.0/12.0)*M_PI)},
  { "align_J", DEC_RA(15.0*M_PI/180.0, (16.0/12.0)*M_PI)},
  { "align_K", DEC_RA(15.0*M_PI/180.0, (20.0/12.0)*M_PI)},
  { "align_L", DEC_RA(15.0*M_PI/180.0, (0.0/12.0)*M_PI)},
  // Equator
  { "align_0a", DEC_RA(0.0, (0.0/12.0)*M_PI)},
  { "align_0b", DEC_RA(0.0, (1.33/12.0)*M_PI)},
  { "align_0c", DEC_RA(0.0, (2.67/12.0)*M_PI)},
  { "align_0d", DEC_RA(0.0, (4.0/12.0)*M_PI)},
  { "align_0e", DEC_RA(0.0, (5.33/12.0)*M_PI)},
  { "align_0f", DEC_RA(0.0, (6.67/12.0)*M_PI)},
  { "align_0g", DEC_RA(0.0, (8.0/12.0)*M_PI)},
  { "align_0h", DEC_RA(0.0, (9.33/12.0)*M_PI)},
  { "align_0i", DEC_RA(0.0, (10.67/12.0)*M_PI)},
  { "align_0j", DEC_RA(0.0, (12.0/12.0)*M_PI)},
  { "align_0k", DEC_RA(0.0, (13.33/12.0)*M_PI)},
  { "align_0l", DEC_RA(0.0, (14.67/12.0)*M_PI)},
  { "align_0m", DEC_RA(0.0, (16.0/12.0)*M_PI)},
  { "align_0n", DEC_RA(0.0, (17.33/12.0)*M_PI)},
  { "align_0o", DEC_RA(0.0, (18.67/12.0)*M_PI)},
  { "align_0p", DEC_RA(0.0, (20.0/12.0)*M_PI)},
  { "align_0q", DEC_RA(0.0, (21.33/12.0)*M_PI)},
  { "align_0r", DEC_RA(0.0, (22.67/12.0)*M_PI)},
  // Dec -15 deg
  { "align-15a", DEC_RA(-15.0*M_PI/180.0, (0.0/12.0)*M_PI)},
  { "align-15b", DEC_RA(-15.0*M_PI/180.0, (4.0/12.0)*M_PI)},
  { "align-15c", DEC_RA(-15.0*M_PI/180.0, (8.0/12.0)*M_PI)},
  { "align-15d", DEC_RA(-15.0*M_PI/180.0, (12.0/12.0)*M_PI)},
  { "align-15e", DEC_RA(-15.0*M_PI/180.0, (16.0/12.0)*M_PI)},
  { "align-15f", DEC_RA(-15.0*M_PI/180.0, (20.0/12.0)*M_PI)},
  { "align-15a1", DEC_RA(-15.0*M_PI/180.0, (2.0/12.0)*M_PI)},
  { "align-15b1", DEC_RA(-15.0*M_PI/180.0, (6.0/12.0)*M_PI)},
  { "align-15c1", DEC_RA(-15.0*M_PI/180.0, (10.0/12.0)*M_PI)},
  { "align-15d1", DEC_RA(-15.0*M_PI/180.0, (14.0/12.0)*M_PI)},
  { "align-15e1", DEC_RA(-15.0*M_PI/180.0, (18.0/12.0)*M_PI)},
  { "align-15f1", DEC_RA(-15.0*M_PI/180.0, (22.0/12.0)*M_PI)},
  // Dec +30
  { "align+30a", DEC_RA(30.0*M_PI/180.0, (0.0/12.0)*M_PI)},
  { "align+30b", DEC_RA(30.0*M_PI/180.0, (1.6/12.0)*M_PI)},
  { "align+30c", DEC_RA(30.0*M_PI/180.0, (3.2/12.0)*M_PI)},
  { "align+30d", DEC_RA(30.0*M_PI/180.0, (4.8/12.0)*M_PI)},
  { "align+30e", DEC_RA(30.0*M_PI/180.0, (6.4/12.0)*M_PI)},
  { "align+30f", DEC_RA(30.0*M_PI/180.0, (8.0/12.0)*M_PI)},
  { "align+30g", DEC_RA(30.0*M_PI/180.0, (9.6/12.0)*M_PI)},
  { "align+30h", DEC_RA(30.0*M_PI/180.0, (11.2/12.0)*M_PI)},
  { "align+30i", DEC_RA(30.0*M_PI/180.0, (12.8/12.0)*M_PI)},
  { "align+30j", DEC_RA(30.0*M_PI/180.0, (14.4/12.0)*M_PI)},
  { "align+30k", DEC_RA(30.0*M_PI/180.0, (16.0/12.0)*M_PI)},
  { "align+30l", DEC_RA(30.0*M_PI/180.0, (17.6/12.0)*M_PI)},
  { "align+30m", DEC_RA(30.0*M_PI/180.0, (19.2/12.0)*M_PI)},
  { "align+30n", DEC_RA(30.0*M_PI/180.0, (20.8/12.0)*M_PI)},
  { "align+30o", DEC_RA(30.0*M_PI/180.0, (22.4/12.0)*M_PI)},
  // Dec +50
  { "align+50a", DEC_RA(50.0*M_PI/180.0, (0.0/12.0)*M_PI)},
  { "align+50b", DEC_RA(50.0*M_PI/180.0, (1.6/12.0)*M_PI)},
  { "align+50c", DEC_RA(50.0*M_PI/180.0, (3.2/12.0)*M_PI)},
  { "align+50d", DEC_RA(50.0*M_PI/180.0, (4.8/12.0)*M_PI)},
  { "align+50e", DEC_RA(50.0*M_PI/180.0, (6.4/12.0)*M_PI)},
  { "align+50f", DEC_RA(50.0*M_PI/180.0, (8.0/12.0)*M_PI)},
  { "align+50g", DEC_RA(50.0*M_PI/180.0, (9.6/12.0)*M_PI)},
  { "align+50h", DEC_RA(50.0*M_PI/180.0, (11.2/12.0)*M_PI)},
  { "align+50i", DEC_RA(50.0*M_PI/180.0, (12.8/12.0)*M_PI)},
  { "align+50j", DEC_RA(50.0*M_PI/180.0, (14.4/12.0)*M_PI)},
  { "align+50k", DEC_RA(50.0*M_PI/180.0, (16.0/12.0)*M_PI)},
  { "align+50l", DEC_RA(50.0*M_PI/180.0, (17.6/12.0)*M_PI)},
  { "align+50m", DEC_RA(50.0*M_PI/180.0, (19.2/12.0)*M_PI)},
  { "align+50n", DEC_RA(50.0*M_PI/180.0, (20.8/12.0)*M_PI)},
  { "align+50o", DEC_RA(50.0*M_PI/180.0, (22.4/12.0)*M_PI)},
  // Dec +70
  { "align_N1", DEC_RA(70*M_PI/180.0, (4.0/12.0)*M_PI)},
  { "align_N2", DEC_RA(70*M_PI/180.0, (12.0/12.0)*M_PI)},
  { "align_N3", DEC_RA(70*M_PI/180.0, (20.0/12.0)*M_PI)},
  { "align_N4", DEC_RA(70*M_PI/180.0, (8.0/12.0)*M_PI)},
  { "align_N5", DEC_RA(70*M_PI/180.0, (16.0/12.0)*M_PI)},
  { "align_N6", DEC_RA(70*M_PI/180.0, (0.0/12.0)*M_PI)},
};
#define num_base_catalog (sizeof(base_catalog)/sizeof(base_catalog[0]))

//****************************************************************
//        do_exposure()
// Local invocation of expose_image()
//****************************************************************
void do_exposure(BaseStarlist *star) {
  exposure_flags flags;
  flags.SetFilter("Clear");
  flags.SetDoNotTrack();
  fprintf(stdout, "Starting exposure."); fflush(stdout);
  const double sidereal_time_start = GetSiderealTime();
  last_image_filename = expose_image(exposure_time, flags);
  const double sidereal_time_end = GetSiderealTime();
  fprintf(stdout, " (Done: %s.)\n", last_image_filename); fflush(stdout);

  star->imaged = true;

  char command[256];
  sprintf(command, COMMAND_DIR "/find_stars -d %s -i %s > /tmp/find.txt 2>&1",
	  darkfilename, last_image_filename);
  fprintf(stderr, "executing %s\n", command);
  if(system(command) == -1) {
    perror("Unable to execute find_stars command");
  } else {
    sprintf(command,
	    COMMAND_DIR "/star_match -h -e -f -d %s -n %s -i %s > /tmp/match.txt 2>&1",
	    darkfilename, star->lookup_name, last_image_filename);
    fprintf(stderr, "executing %s\n", command);
    if(system(command) == -1) {
      perror("Unable to execute star_match command");
    }
  }
  Image image(last_image_filename);
  int status;
  DEC_RA current_center = image.ImageCenter(status);
  if (current_center.dec() != 0.0 && current_center.ra() != 0.0) {
    star->solved = true;
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
    DEC_RA raw_mount_points_at = RawScopePointsAt();
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

    FILE *fp = fopen(align_points_filename, "a");
    if (fp) {
      fprintf(fp, "%s\n", alignpoint_buffer);
      fclose(fp);
      fprintf(stderr, "Adding point to align sync point file.\n");
    } else {
      fprintf(stderr, "Cannot open align_points.txt to add point.\n");
    }
  } else {
    fprintf(stderr, "star_match failed to generate valid Dec/RA.\n");
  }
}

//****************************************************************
//        main()
//****************************************************************

void usage(void) {
  fprintf(stderr, "usage: auto_sync -t exposure_time\n");
  exit(-2);
}

int main(int argc, char **argv) {
  int option_char;
  
  while((option_char = getopt(argc, argv, "t:")) > 0) {
    switch (option_char) {
    case 't':
      if(sscanf(optarg, "%lf", &exposure_time) != 1) {
	fprintf(stderr, "Illegal exposure_time\n");
	usage();
      }
      break;

    case '?':
    default:
      usage();
    }
  }
  
  connect_to_scope();
  connect_to_camera();

  {
    char command[256];
    const char *dirname = DateToDirname();
    int int_exposure_time = (int) (exposure_time+0.5);
    sprintf(command, "dark_manager -n 5 -t %d -d %s",
	    int_exposure_time,
	    DateToDirname());
    if (system(command) < 0) {
      fprintf(stderr, "Error invoking dark_manager command.\n");
      DisconnectINDI();
      exit(-1);
    } 
    char dark_file_name[256];
    sprintf(dark_file_name, "%s/dark%d.fits",
	    dirname, int_exposure_time);
    darkfilename = strdup(dark_file_name);

    char align_point_file[256];
    sprintf(align_point_file, "%s/align_points.txt",
	    dirname);
    align_points_filename = strdup(align_point_file);
  }

  for (unsigned int i=0; i<num_base_catalog; i++) {
    BaseStarlist *star = &(base_catalog[i]);
    JULIAN now(time(0));

    DEC_RA star_location = star->location;
    ALT_AZ star_altaz(star_location, now);
    star->imaged = false;
    star->solved = false;
    star->visible = IsVisible(star_altaz, now);
    fprintf(stderr, "Field %s at DEC/RA = (%s, %s)\n",
	    star->lookup_name, star_location.string_longdec_of(), star_location.string_ra_of());
    if (!star->visible) {
      fprintf(stderr, "  Field %s below horizon.\n",
	      star->lookup_name);
      continue;
    }

    fprintf(stdout, "Starting slew to field %s.\n", star->lookup_name);
    fflush(stdout);

    MoveTo(&star_location);
    WaitForGoToDone();

    do_exposure(star);
  }

  int num_fields = 0;
  int num_fields_exposed = 0;
  int num_fields_visible = 0;
  int num_fields_solved = 0;
  for (unsigned int i=0; i<num_base_catalog; i++) {
    BaseStarlist *star = &(base_catalog[i]);
    num_fields++;
    if (star->visible) num_fields_visible++;
    if (star->imaged) num_fields_exposed++;
    if (star->solved)  num_fields_solved++;
  }

  fprintf(stderr, "%d fields exist\n", num_fields);
  fprintf(stderr, "%d fields visible\n", num_fields_visible);
  fprintf(stderr, "%d fields exposed\n", num_fields_exposed);
  fprintf(stderr, "%d fields solved\n", num_fields_solved);

  DisconnectINDI();
  return 0;
}
