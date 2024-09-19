/*  import_vphot.cc -- Import photometry from a VPHOT text file into a
 *  local FITS image. 
 *
 *  Copyright (C) 2017 Mark J. Munkacsy
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
#include <string.h>
#include <IStarList.h>
#include <Image.h>


static void usage(void) {
  fprintf(stderr,
	  "usage: import_vphot -i image.fits -p vphot.txt\n");
  exit(-2);
}
  
int main(int argc, char **argv) {
  int option_char;
  FILE *vphot_fp = 0;
  const char *image_filename = 0;
  Image *image = 0;
  unsigned int matched_vphot_stars = 0;
  unsigned int unmatched_vphot_stars = 0;

  while((option_char = getopt(argc, argv, "p:i:")) > 0) {
    switch (option_char) {
    case 'i':
      image_filename = optarg;
      image = new Image(image_filename);
      break;

    case 'p':
      vphot_fp = fopen(optarg, "r");
      break;

    case '?':			// invalid argument
    default:
      fprintf(stderr, "Invalid argument.\n");
      exit(2);
    }
  }

  // make sure that we have both the vphot file and the image to be
  // modified
  if (vphot_fp == 0 || image == 0) usage();

  // Get the starlist
  IStarList istars(image_filename);

  // and clear all existing photometry in the starlist
  for (int i=0; i<istars.NumStars; i++) {
    IStarList::IStarOneStar *s = istars.FindByIndex(i);
    s->validity_flags &= (~PHOTOMETRY_VALID);
  }

  // and read the vphot file
  // Ignore all lines up to and including the line starting with the
  // word "Star", which is the last line of the header.
  char buffer[256];
  while(fgets(buffer, sizeof(buffer), vphot_fp)) {
    if (buffer[0] == 'S' && buffer[1] == 't' && buffer[2] == 'a' &&
	buffer[3] == 'r') break;
  }

  // Now read star lines one at a time
  while(fgets(buffer, sizeof(buffer), vphot_fp)) {
    char t_starname[32];
    double t_inst_mag;
    double t_snr;
    double t_x;
    double t_y;

    int num_scanned = sscanf(buffer, "%s\t%lf\t%lf\t%lf\t%lf",
			     t_starname,
			     &t_inst_mag,
			     &t_snr,
			     &t_x,
			     &t_y);
    // silently ignore blank lines
    if (num_scanned == 0) continue;
    if (num_scanned != 5) {
      fprintf(stderr, "Invalid star line in vphot file: %s\n",
	      buffer);
      continue;
    }

    // Match up the VPHOT star with its corresponding image star. 
    // Search through the original starlist for the closest star
    double best_r2 = 256.0 * 256.0;
    int    best_star = -1;
    for (int i=0; i<istars.NumStars; i++) {
      IStarList::IStarOneStar *s = istars.FindByIndex(i);
      const double del_x = s->StarCenterX() - t_x;
      const double del_y = s->StarCenterY() - t_y;
      // del_r2 is the square of the distance between the two stars,
      // measured in pixels.
      const double del_r2 = (del_x * del_x + del_y * del_y);
      if (del_r2 < best_r2) {
	best_r2 = del_r2;
	best_star = i;
      }
    }

    // 0.25 pixel^2 means stars were no more than 1/2 pixel away from
    // each other.
    if (best_r2 < 5.0) {
      IStarList::IStarOneStar *s = istars.FindByIndex(best_star);
      s->photometry = t_inst_mag;
      s->validity_flags |= PHOTOMETRY_VALID;
      matched_vphot_stars++;
    } else {
      unmatched_vphot_stars++;
    }
  }

  fprintf(stderr, "%d vphot stars matched (out of %d)\n",
	  matched_vphot_stars, matched_vphot_stars + unmatched_vphot_stars);
  unsigned int matched_image_stars = 0;
  for (int i=0; i<istars.NumStars; i++) {
    IStarList::IStarOneStar *s = istars.FindByIndex(i);
    if (s->validity_flags & PHOTOMETRY_VALID) matched_image_stars++;
  }
  fprintf(stderr, "%d image stars matched (out of %d)\n",
	  matched_image_stars, istars.NumStars);
  istars.SaveIntoFITSFile(image_filename, 1);
}

  
  
