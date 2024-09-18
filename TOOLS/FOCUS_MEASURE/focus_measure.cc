/*  focus_measure.cc -- Program to use PSF-fitting to print FWHM of an image.
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
#include <string.h>
#include <unistd.h> 		// for getopt()
#include <stdlib.h>		// for atof(), atoi()
#include <Image.h>		// for class Image

// Command-line options:
//
// -d dark_frame.fits		// filename of dark frame
// -i image.fits                // filename of image
//
//

int main(int argc, char **argv) {
  int ch;			// option character
  Image *dark_image = 0;
  Image *primary_image = 0;
  char image_filename[256];

  while((ch = getopt(argc, argv, "d:i:")) != -1) {
    switch(ch) {
    case 'd':			// darkfile name
      dark_image = new Image(optarg); // create image from dark file
      break;

    case 'i':			// image file name
      primary_image = new Image(optarg);
      strcpy(image_filename, optarg); // save the filename
      break;

    case '?':
    default:
	fprintf(stderr,
		"usage: %s -d dark.fits -i image.fits\n", argv[0]);
	return 2;		// error return
    }
  }

  if(primary_image == 0) {
    fprintf(stderr, "usage: %s -d dark.fits -i image.fits\n", argv[0]);
    return 2;			// error return
  }

  if(dark_image) primary_image->subtract(dark_image);

  double this_focus_value = primary_image->composite_fwhm();

  printf("focus %f %s\n", this_focus_value, image_filename);
}
