/*  assess_flat.cc -- Assess flatness of a flat
 *
 *  Copyright (C) 2021 Mark J. Munkacsy
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

// Command-line options:
//
// -i flat.fits (image to be fully flattened)
// -o table.csv (points for radial intensity plot)
//

void usage(void) {
  fprintf(stderr, "Usage: assess_flat -i flatfile.fits [-o table.csv]\n");
  exit(-2);
}

int main(int argc, char **argv) {
  int ch;			// option character
  char *image_filename = nullptr;
  char *output_table_filename = nullptr;

  while((ch = getopt(argc, argv, "o:i:")) != -1) {
    switch(ch) {
    case 'i':
      image_filename = optarg;
      break;

    case 'o':
      output_table_filename = optarg;
      break;

    case '?':
    default:
      usage();
    }
  }

  if(image_filename == 0) usage();

  // grab the image to be flattened
  Image image(image_filename);

  constexpr static int NUM_BINS = 100;
  
  const double center_x = image.width/2.0;
  const double center_y = image.height/2.0;
  const double max_r = sqrt(center_x*center_x + center_y*center_y);
  const double r_bin_width = max_r/NUM_BINS;

  int bin_counts[NUM_BINS] {0};
  double bin_sums[NUM_BINS] {0.0};
  
  for (int y=0; y<image.height; y++) {
    const double y_offset = (y-center_y);
    for (int x=0; x<image.width; x++) {
      const double x_offset = (x-center_x);
      const double r = sqrt(x_offset*x_offset + y_offset*y_offset);
      const int bin_number = (r == max_r ? (NUM_BINS-1) : (int) (r/r_bin_width));
      bin_counts[bin_number]++;
      bin_sums[bin_number] += image.pixel(x,y);
    }
  }

  double bin_avg[NUM_BINS];
  double max_bin_avg = 0.0;
  
  for (int i=0; i<NUM_BINS; i++) {
    if (bin_counts[i] == 0) {
      bin_avg[i] = 0.0;
    } else {
      bin_avg[i] = bin_sums[i]/bin_counts[i];
      if (bin_avg[i] > max_bin_avg) max_bin_avg = bin_avg[i];
    }
  }

  for (int i=0; i<NUM_BINS; i++) {
    bin_avg[i] = bin_avg[i]/max_bin_avg;
  }

  FILE *fp_out = fopen(output_table_filename, "w");
  if (!fp_out) {
    perror("Unable to open output file:");
  } else {
    for (int i=0; i<NUM_BINS; i++) {
      fprintf(fp_out, "%lf,%lf\n",
	      r_bin_width*(i+0.5),
	      bin_avg[i]);
    }
    fclose(fp_out);
    fprintf(stderr, "Finished creating output file %s\n", output_table_filename);
  }

  // Now fit to a polynomial
  double sum_x = 0.0;
  double sum_xx = 0.0;
  double sum_y = 0.0;
  double sum_yy = 0.0;
  double sum_xy = 0.0;
  for (int i=0; i<NUM_BINS; i++) {
    const double x = r_bin_width*(i+0.5)*r_bin_width*(i+0.5);
    const double &y = bin_avg[i];
    sum_x += x;
    sum_xx += x*x;
    sum_y += y;
    sum_yy += y*y;
    sum_xy += x*y;
  }
  const double b = (NUM_BINS*sum_xy - sum_x*sum_y)/(NUM_BINS*sum_xx - sum_x*sum_x);
  const double a = (sum_y - b*sum_x)/NUM_BINS;

  fprintf(stderr, "center = %lf, corner min = %lf\n",
	  a, a+b*(center_x*center_x+center_y*center_y));
    
  return 0;
}
      
  
