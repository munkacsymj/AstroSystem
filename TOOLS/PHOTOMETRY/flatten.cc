/*  flatten.cc -- Force a flat to have 0 gradient in the x/y direction
 *
 *  Copyright (C) 2015 Mark J. Munkacsy
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
#include <stdio.h>
#include <unistd.h> 		// for getopt()
#include <stdlib.h>		// for atof(), atoi()
#include <Image.h>		// get Image
#include <gendefs.h>
#include <background.h>

// Command-line options:
//
// -i flat.fits (image to be fully flattened
// -n           (don't actually change the image)
//

void usage(void) {
  fprintf(stderr, "Usage: flatten [-n] -i flatfile.fits\n");
  exit(-2);
}

int main(int argc, char **argv) {
  int ch;			// option character
  char *image_filename = 0;
  bool no_write = false;

  while((ch = getopt(argc, argv, "ni:")) != -1) {
    switch(ch) {
    case 'i':
      image_filename = optarg;
      break;

    case 'n':
      no_write = true;
      break;

    case '?':
    default:
      usage();
    }
  }

  if(image_filename == 0) usage();

  // grab the image to be flattened
  Image image(image_filename);
  // compute the background gradients
  Background b(&image);
  // find the nominal 50% point of the image
  const double median = image.HistogramValue(0.5);

  // now loop through and modify every pixel
  double biggest = 0.0;
  double smallest = 99.9;
  double gradient_max = -1.0;
  double gradient_min = 99.9;

  for (int x=0; x < image.width; x++) {
    for (int y=0; y < image.height; y++) {
      const double orig_value = image.pixel(x, y);
      const double gradient = b.Value(x, y);
      const double factor = median/gradient;
      image.pixel(x, y) = orig_value * factor;
      if (factor > biggest) biggest = factor;
      if (factor < smallest) smallest = factor;
      if (gradient > gradient_max) gradient_max = gradient;
      if (gradient < gradient_min) gradient_min = gradient;
    }
  }

  fprintf(stderr, "gradient min/max = %.3lf/%.3lf\n",
	  gradient_min/b.Mean(), gradient_max/b.Mean());

  fprintf(stderr, "After flattening....\n");
  Background new_b(&image);

  if (!no_write) image.WriteFITSFloat(image_filename);
  
  fprintf(stderr, "median value was %lf\n", median);
  fprintf(stderr, "max adjustment was %lf\n", biggest);
  fprintf(stderr, "min adjustment was %lf\n", smallest);

  return 0;
}
      
  
