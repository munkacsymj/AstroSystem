/*  flip_image.cc -- Rotate a FITS image by 180-degrees
 *
 *  Copyright (C) 2007, 2021 Mark J. Munkacsy
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
#include <Image.h>
#include <math.h>		// for PI
#include <fitsio.h>

int main(int argc, char **argv) {
  int ch;			// option character
  char *image_filename = 0;		// filename of the .fits image file
  char *output_filename = 0;

  // Command line options:
  //
  // -i filename.fits   Image file
  // -o filename.fits   Flipped image is written to this file
  //

  while((ch = getopt(argc, argv, "i:o:")) != -1) {
    switch(ch) {
    case 'i':			// image filename
      image_filename = optarg;
      break;

    case 'o':
      output_filename = optarg;
      break;

    case '?':
    default:
      fprintf(stderr,
	      "usage: %s -i image_filename.fits -o outputname.fits\n",
	      argv[0]);
      return 2;			// error return
    }
  }

  if(image_filename == 0 || output_filename == 0) {
    fprintf(stderr,
	    "usage: %s -i image_filename.fits -o outputname.fits\n",
	    argv[0]);
    return 2;			// error return
  }

  Image input_image(image_filename);
  input_image.WriteFITSAuto(output_filename);
  Image output_image(output_filename);

  int i, j;

  for(i=0; i<input_image.height; i++) {
    for(j=0; j<input_image.width; j++) {
      output_image.pixel(input_image.width - 1 - j,
			 input_image.height - 1 - i) = input_image.pixel(j, i);
    }
  }

  if(output_image.GetImageInfo() == 0) output_image.CreateImageInfo();
  ImageInfo *i_info = input_image.GetImageInfo();
  ImageInfo *o_info = output_image.GetImageInfo();
  
  // rotation angle of PI if we don't have any other info
  o_info->SetRotationAngle(M_PI);

  // if we do have some info on rotation angle, flip it by 180 degrees
  if(i_info) {
    if(i_info->RotationAngleValid()) {
      // add pi to flip the angle by 180 degrees
      double new_angle =
	i_info->GetRotationAngle() + M_PI;
      if(new_angle >= M_PI*2) new_angle -= (M_PI*2);
      o_info->SetRotationAngle(new_angle);
    } else {
      o_info->SetRotationAngle(M_PI);
    }
    if(i_info->NorthIsUpValid()) {
      o_info->SetNorthIsUp(!i_info->NorthIsUp());
    }
  }

  output_image.WriteFITS(output_filename);
}
