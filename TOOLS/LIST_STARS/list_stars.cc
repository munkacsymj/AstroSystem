/*  list_stars.cc -- List all stars in a FITS file's star table.
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
#include <stdio.h>
#include <unistd.h> 		// for getopt()
#include <stdlib.h>		// for atof()
#include <IStarList.h>
#include <fitsio.h>

int main(int argc, char **argv) {
  int ch;			// option character
  char *image_filename = 0;		// filename of the .fits image file

  // Command line options:
  //
  // -i filename.fits   Image file
  //

  while((ch = getopt(argc, argv, "i:")) != -1) {
    switch(ch) {
    case 'i':			// image filename
      image_filename = optarg;
      break;

    case '?':
    default:
      fprintf(stderr, "usage: %s -i image_filename.fits \n", argv[0]);
      return 2;			// error return
    }
  }

  if(image_filename == 0) {
    fprintf(stderr, "usage: %s -i image_filename.fits \n", argv[0]);
    return 2;			// error return
  }

  // Pull the list from the file
  IStarList List(image_filename);

  // Refine the stars and update in the List.
  int i;
  for(i=0; i < List.NumStars; i++) {
    IStarList::IStarOneStar *oneStar = List.FindByIndex(i);

    fprintf(stdout, "%-16s (%8.2f, %8.2f) ",
	    oneStar->StarName,
	    oneStar->nlls_x,
	    oneStar->nlls_y);
    if(oneStar->validity_flags & DEC_RA_VALID) {
      fprintf(stdout, "Dec/RA=(%s %s) ",
	      oneStar->dec_ra.string_dec_of(),
	      oneStar->dec_ra.string_ra_of());
      fprintf(stdout, "=(%.10lf, %.10lf) [rad] ",
	      oneStar->dec_ra.dec(),
	      oneStar->dec_ra.ra_radians());
    }
    if(oneStar->validity_flags & MAG_VALID) {
      fprintf(stdout, "Mag=%8.4f ", oneStar->magnitude);
    }
    if(oneStar->validity_flags & COUNTS_VALID) {
      fprintf(stdout, "Counts=%.1f ", oneStar->nlls_counts);
    }
    if(oneStar->validity_flags & PHOTOMETRY_VALID) {
      fprintf(stdout, "Phot=%.3f ", oneStar->photometry);
      //fprintf(stdout, "Flux=%.1f ", oneStar->flux); // flux isn't in the .fits table
    }
    if(oneStar->info_flags & STAR_IS_COMP) {
      fprintf(stdout, "COMP ");
    }
    if(oneStar->info_flags & STAR_IS_CHECK) {
      fprintf(stdout, "CHECK ");
    }
    if(oneStar->info_flags & STAR_IS_SUBMIT) {
      fprintf(stdout, "SUBMIT ");
    }
    
    fprintf(stdout, "\n");
  }
}
