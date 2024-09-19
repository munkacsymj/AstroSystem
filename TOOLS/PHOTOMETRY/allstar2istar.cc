/*  allstar2istar.cc -- use output table from IRAF's allstars program
 *  as source of photometry for an IStarList in an Image.
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
#include <string.h>
#include <sys/types.h>		// for getpid()
#include <stdio.h>
#include <unistd.h> 		// for getopt(), getpid()
#include <stdlib.h>		// for atof()
#include <Image.h>
#include <IStarList.h>
#include <fitsio.h>
#include <gendefs.h>

void usage(void) {
      fprintf(stderr,
	      "usage: allstar2istar -t image.als -i image.fits\n");
      exit(-2);
}

IStarList::IStarOneStar *
find_by_xy(IStarList *list, double x, double y) {
  x -= 1.0;
  y -= 1.0;
  
  for (int i=0; i<list->NumStars; i++) {
    IStarList::IStarOneStar *s = list->FindByIndex(i);
    const double del_x = abs(x - s->nlls_x);
    const double del_y = abs(y - s->nlls_y);
    if (del_x < 0.5 && del_y < 0.5) return s;
  }
  return 0;
}

int main(int argc, char **argv) {
  int ch;			// option character
  char *image_filename = 0;	// filename of the .fits image file
  char *iraf_filename = 0;

  // Command line options:
  // -i imagefile.fits
  // -t image.als           Output from allstars task

  while((ch = getopt(argc, argv, "i:t:")) != -1) {
    switch(ch) {
    case 'i':			// image filename
      image_filename = optarg;
      break;

    case 't':
      iraf_filename = optarg;
      break;

    case '?':
    default:
      usage();
   }
  }

  // all command-line arguments are required
  if(image_filename == 0 || iraf_filename == 0) {
    usage();
  }

  FILE *fp_iraf = fopen(iraf_filename, "r");
  if (!fp_iraf) {
    perror("Unable to open iraf_file:");
    usage();
  }

  IStarList ilist = IStarList(image_filename);
  // Clear any existing photometry from the istarlist.
  for (int i=0; i<ilist.NumStars; i++) {
    IStarList::IStarOneStar *s = ilist.FindByIndex(i);
    s->validity_flags &= (~(PHOTOMETRY_VALID|ERROR_VALID));
  }

  char input_buffer[1024];
  int num_matched = 0;
  int num_unmatched = 0;
  while(fgets(input_buffer, sizeof(input_buffer), fp_iraf)) {
    // skip all the comment line at the start of the file
    if(input_buffer[0] == '#') continue;
  
    int als_id;
    double als_x;
    double als_y;
    double als_mag;
    double als_err;
    double als_msky;

    if (sscanf(input_buffer, "%d %lf %lf %lf %lf %lf",
	       &als_id, &als_x, &als_y, &als_mag, &als_err, &als_msky) != 6) {
      fprintf(stderr, "trouble (1) parsing '%s'\n", input_buffer);
    } else {
      // get the error field on the next line
      char err_msg[32];
      if(!fgets(input_buffer, sizeof(input_buffer), fp_iraf)) {
	fprintf(stderr, "trouble (1a) parsing '%s'\n", input_buffer);
      }
      if (sscanf(input_buffer+41, "%s", err_msg) != 1) {
	fprintf(stderr, "trouble (2) parsing '%s'\n", input_buffer);
      } else if(strcmp(err_msg, "No_error") == 0) {
	// no error: success and a good data point
	IStarList::IStarOneStar *s = find_by_xy(&ilist, als_x, als_y);
	if (s) {
	  s->photometry = als_mag;
	  s->magnitude_error = als_err;
	  s->validity_flags |= (PHOTOMETRY_VALID|ERROR_VALID);
	  num_matched++;
	} else {
	  num_unmatched++;
	} // end if an (x,y) match occurred
      } // end if photometry was found in .als file
    } // end if first of two lines parsed okay
  } // end loop over all lines in file

  fprintf(stderr, "%d stars updated, %d stars in allstars data not in IStarList.\n",
	  num_matched, num_unmatched);
  ilist.SaveIntoFITSFile(image_filename);
}
