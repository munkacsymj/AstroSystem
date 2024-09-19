/*  calibrate.cc -- Dark-subtract and flat-field correct an image
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
#include <stdio.h>
#include <libgen.h>		// for basename()
#include <unistd.h> 		// for getopt()
#include <stdlib.h>		// for atof()
#include <Image.h>

void usage(void) {
  fprintf(stderr, "usage: calibrate [-g] -d darkfile.fits -s flatfile.fits -i raw.fits -o calibrated.fits\n");
  fprintf(stderr, "       (Include -g to perform shutter gradient correction.)\n");
  exit(-2);
}

int main(int argc, char **argv) {
  int ch;			// option character
  char *image_filename = 0;	// filename of the output .fits image file
  char *flatfield_filename = 0;
  char *dark_filename = 0;
  char *output_filename = 0;
  char *pair_filename = 0;
  bool linearize = 0;
  bool shutter_gradient_correct = 0;

  // Command line options:
  //
  // [-g] [-d bias.fits] [-l] [-p pair.fits] [-s flatfield_file.fits] -o filename.fits   
  //

  while((ch = getopt(argc, argv, "gld:s:o:i:p:")) != -1) {
    switch(ch) {
    case 'g':
      shutter_gradient_correct = true;
      break;

    case 'l':
      linearize = true;
      break;
      
    case 'p':
      pair_filename = optarg;
      break;

    case 'o':			// image filename
      output_filename = optarg;
      break;

    case 'i':			// image filename
      image_filename = optarg;
      break;

    case 'd':
      dark_filename = optarg;	// dark (bias) filename
      break;

    case 's':			// scale (flatfield) filename
      flatfield_filename = optarg;
      break;

    case '?':
    default:
      usage();
      /*NOTREACHED*/
    }
  }

  if(image_filename == 0) {
    usage();
    /*NOTREACHED*/
  }

  Image *bias = 0;
  if (dark_filename) {
    bias = new Image(dark_filename);
  }

  Image *flat = 0;
  if (flatfield_filename) {
    flat = new Image(flatfield_filename);
  }

  Image *pair = 0;
  if (pair_filename) {
    pair = new Image(pair_filename);
  }

  Image *raw = new Image(image_filename);
  if (bias) raw->subtract(bias);

  if (pair) {
    if (bias) pair->subtract(bias);

    int pixel_count = 0;
    double pixel_sum = 0.0;

    for (int row=0; row<raw->height; row++) {
      for (int col=0; col<raw->width; col++) {
	double p1 = raw->pixel(col, row);
	double p2 = pair->pixel(col, row);
	double delta = p1 - p2;

	if (p1 > 65400.0 || p2 > 65400.0) continue;
	pixel_sum += delta;
	pixel_count++;
      }
    }

    const double diff_avg = pixel_sum/pixel_count;
    pixel_sum = 0;
    pixel_count = 0;
    double pixel_sum_sq = 0.0;

    for (int row=0; row<raw->height; row++) {
      for (int col=0; col<raw->width; col++) {
	double p1 = raw->pixel(col, row);
	double p2 = pair->pixel(col, row);
	double delta = (p1 - p2) - diff_avg;

	if (p1 > 65400.0 || p2 > 65400.0) continue;
	pixel_sum += (p1 + p2);
	pixel_sum_sq += (delta*delta);
	pixel_count++;
      }
    }

    fprintf(stderr, "Average = %lf\n", pixel_sum/(2*pixel_count));
    fprintf(stderr, "Stddev = %lf\n", sqrt(pixel_sum_sq/pixel_count));
    
  } else {
    ImageInfo *info = raw->GetImageInfo();
    if (linearize) raw->linearize();
    if (shutter_gradient_correct) {
      double exptime = info->GetExposureDuration();
      raw->RemoveShutterGradient(exptime);
    }
    if (flat) raw->scale(flat);

    info->SetValueString(string("DEC"), string(info->GetValueString("DEC_NOM")));
    info->SetValueString(string("RA"), string(info->GetValueString("RA_NOM")));

    if (output_filename) {
      raw->WriteFITSFloatUncompressed(output_filename);
      //raw->WriteFITSFloat(output_filename);
    }

    Statistics *s = raw->statistics();
    fprintf(stderr, "Darkest = %lf\n", s->DarkestPixel);
    fprintf(stderr, "Brightest = %lf\n", s->BrightestPixel);
    fprintf(stderr, "Average = %lf\n", s->AveragePixel);
    fprintf(stderr, "Median = %lf\n", s->MedianPixel);
    fprintf(stderr, "Stddev = %lf\n", s->StdDev);

  } // end if not pair-wise comparison
  
  return 0;
}
