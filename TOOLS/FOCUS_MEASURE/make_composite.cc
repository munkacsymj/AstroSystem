/*  make_composite.cc -- Program to create single, composite star image by
 *  stacking each star found in an image
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
#include <math.h>		// for M_PI and atan2()
#include "Image.h"		// for class Image

// Command-line options:
//
// -i image.fits                // filenames of images
// -o output_filename
//

int main(int argc, char **argv) {
  int ch;			// option character
  char output_filename[256];
  char *ImageInName = 0;
  char *DarkImageName = 0;
  char *FlatImageName = 0;
  
  output_filename[0] = 0;	// nothing initially

  while((ch = getopt(argc, argv, "s:d:i:o:")) != -1) {
    switch(ch) {
    case 'i':			// image file name
      ImageInName = optarg;
      break;

    case 'd':
      DarkImageName = optarg;
      break;

    case 's':
      FlatImageName = optarg;

    case 'o':
      strcpy(output_filename, optarg);
      break;

    case '?':
    default:
	fprintf(stderr,
		"usage: %s -i image.fits -o output_file.fits\n", argv[0]);
	return 2;		// error return
    }
  }

  char command_buffer[2408];
  char dark_string[712];
  char flat_string[712];

  if (DarkImageName) {
    sprintf(dark_string, "-d %s ", DarkImageName);
  } else {
    dark_string[0] = 0;
  }

  if (FlatImageName) {
    sprintf(flat_string, "-s %s ", FlatImageName);
  } else {
    flat_string[0] = 0;
  }

  if(output_filename[0] == 0 || ImageInName == 0) {
    fprintf(stderr,
	    "usage: %s -i image.fits [-d dark.fits] [-s flat.fits] -o output_file.fits\n",
	    argv[0]);
    return 2;			// error return
  }

  sprintf(command_buffer, "find_stars %s %s -i %s",
	  dark_string, flat_string, ImageInName);
  if (system(command_buffer)) {
    fprintf(stderr, "make_composite: find_stars failed.\n");
  } else {
    // fetch the image and its star_list
    Image ImageIn(ImageInName);

    // do a dark subtract
    if (DarkImageName) {
      Image dark_image(DarkImageName);
      ImageIn.subtract(&dark_image);
    }

    // scale by the flat
    if (FlatImageName) {
      Image flat_image(FlatImageName);
      ImageIn.scale(&flat_image);
    }
    
    IStarList *star_list = ImageIn.GetIStarList();
    if (star_list->NumStars >= 1) {
      CompositeImage *composite = BuildComposite(&ImageIn, star_list, 100);
      composite->WriteFITS(output_filename);
      delete composite;
    }
  }
}
