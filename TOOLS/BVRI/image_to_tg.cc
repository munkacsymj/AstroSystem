/*  analyze_bvri.cc -- Takes photometry and assembles into photometry report
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
#include "HGSC.h"
#include "IStarList.h"
#include "Image.h"
#include "named_stars.h"
#include "dec_ra.h"
#include "julian.h"
#include <unistd.h>
#include <stdlib.h>
#include "colors.h"
#include <time.h>
#include "gendefs.h"

void usage(void) {
  fprintf(stderr,
	  "usage: image_to_tg -n starname -i image -o report.txt\n");
  exit(-2);
}

int
main(int argc, char **argv) {
  int ch;			// option character
  FILE *fp_out = 0;
  char *starname = 0;
  Image *image = 0;
  const char *image_name = 0;

  // Command line options:
  // -n star_name       Name of region around which image was taken
  // -o output_filename
  // -i image.fits

  while((ch = getopt(argc, argv, "n:o:i:")) != -1) {
    switch(ch) {
    case 'n':			// name of star
      starname = optarg;
      break;

    case 'i':
      image = new Image(optarg);
      image_name = optarg;
      break;

    case 'o':
      fp_out = fopen(optarg, "w");
      if (!fp_out) {
	fprintf(stderr, "Unable to create output file: %s\n", optarg);
      }
      break;

    case '?':
    default:
      usage();
    }
  }

  if(starname == 0 || image == 0 || fp_out == 0) {
    usage();
  }
  
  char HGSCfilename[132];
  sprintf(HGSCfilename, CATALOG_DIR "/%s", starname);
  FILE *HGSC_fp = fopen(HGSCfilename, "r");
  if(!HGSC_fp) {
    fprintf(stderr, "Cannot open catalog file for %s\n", starname);
    exit(-2);
  }
  HGSCList Catalog(HGSC_fp);

  ImageInfo *info = image->GetImageInfo();

  // PRIMARY TARGET
  fprintf(fp_out, "Primary target: %s\n", starname);

  // EXPOSURE TIME
  fprintf(fp_out, "Exposure time: %.1lf\n", info->GetExposureDuration());

  // FILTER
  Filter filter = info->GetFilter();
  fprintf(fp_out, "Filter: %c\n", filter.NameOf()[0]);

  // OBSERVATION DATE
  JULIAN obs_date = info->GetExposureMidpoint();
  time_t obs_data_time_t = obs_date.to_unix();
  const struct tm *obs_data_tm = gmtime(&obs_data_time_t);
  const int obs_date_year = obs_data_tm->tm_year + 1900;
  const int obs_date_month = obs_data_tm->tm_mon + 1;
  const int obs_date_day = obs_data_tm->tm_mday;
  const double obs_date_fraction = obs_date.day() - 0.5 - (int)obs_date.day();

  fprintf(fp_out, "Observation date/time: %d-%02d-%02d %02d:%02d:%02d\n",
	  obs_date_year, obs_date_month, obs_date_day,
	  obs_data_tm->tm_hour, obs_data_tm->tm_min, obs_data_tm->tm_sec);
  fprintf(fp_out, "JD: %.5lf\n", obs_date.day());
  fprintf(fp_out, "Decimal date: %d-%02d-%08.5lf\n",
	  obs_date_year, obs_date_month, obs_date_day + obs_date_fraction);

  // DEC/RA
  NamedStar target(starname);
  if (target.IsKnown()) {
    DEC_RA target_loc = target.Location();
    fprintf(fp_out, "R.A.: %s\n", target_loc.string_ra_of());
    fprintf(fp_out, "Dec.: %s\n", target_loc.string_dec_of());
  }

  // AIRMASS
  if (info->AirmassValid()) {
    fprintf(fp_out, "Airmass: %.4lf\n", info->GetAirmass());
  } else {
    fprintf(fp_out, "Airmass: 0.000\n");
  }

  // CALIBRATION
  fprintf(fp_out, "Calibration: BDF\n");
  fprintf(fp_out, "Aperture radius: 3.5 pixels\n");
  fprintf(fp_out, "File name: %s\n", image_name);
  fprintf(fp_out, "\n\n\n");

  // Create a local IStarList
  IStarList *List = new IStarList(image_name);

  // Print column headings
  fprintf(fp_out, "Star\tIM\tSNR\tX\tY\t");
  fprintf(fp_out, "Sky\tAir\tB-V\t%c-mag\tTarget estimate\tActive\n",
	  filter.NameOf()[0]);

  for(int i=0; i < List->NumStars; i++) {
    IStarList::IStarOneStar *this_star = List->FindByIndex(i);
    /* fprintf(stderr, "%s   %c%c\n",
	    this_star->StarName, 
	    (this_star->validity_flags & PHOTOMETRY_VALID) ? 'X' : '-',
	    (this_star->validity_flags & CORRELATED) ? 'X' : '-'); */
    if((this_star->validity_flags & PHOTOMETRY_VALID) &&
       (this_star->validity_flags & CORRELATED)) {
      HGSC *cat_entry = Catalog.FindByLabel(this_star->StarName);

      if (cat_entry->multicolor_data.IsAvailable(PHOT_V) == false ||
	  cat_entry->multicolor_data.IsAvailable(PHOT_B) == false) continue;

      // STAR
      if (cat_entry->A_unique_ID) {
	//fprintf(fp_out, "%s\t", cat_entry->label);
	fprintf(fp_out, "%s\t", cat_entry->A_unique_ID);
      } else {
	fprintf(fp_out, "%s\t", cat_entry->label);
      }

      // INSTRUMENTAL MAG
      fprintf(fp_out, "%.3lf\t", this_star->photometry);

      // SNR
      fprintf(fp_out, "%d\t", (int) (1.0857/this_star->magnitude_error));

      // X, Y
      fprintf(fp_out, "%.3lf\t", this_star->StarCenterX());
      fprintf(fp_out, "%.3lf\t", this_star->StarCenterY());

      // Sky
      fprintf(fp_out, "21\t");

      // Airmass
      fprintf(fp_out, "%.3lf\t", info->GetAirmass());

      // B-V (published)
      fprintf(fp_out, "%.3lf\t", cat_entry->multicolor_data.Get(PHOT_B) -
	      cat_entry->multicolor_data.Get(PHOT_V));

      // Phot (published)
      fprintf(fp_out, "%.3lf\t", cat_entry->multicolor_data.Get(FilterToColor(filter)));

      // Target
      fprintf(fp_out, "15.000\t");

      // Estimate
      fprintf(fp_out, "True");

      // End of line
      fprintf(fp_out, "\n");
    }
  }

  fclose(fp_out);
}
