/*  image_statistics.cc -- print statistics about an image
 *
 *  Copyright (C) 2020 Mark J. Munkacsy
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
#include <Image.h>		// get Image
#include <Statistics.h>

//
// -i image.fits		// filename of image
// -o filename.txt              // where to put the output
// -h xxx                       // histogram centered on xxx   
//

void usage(void) {
  fprintf(stderr, "image_statistics [-h] -i image.fits [-o output.txt]\n");
  exit(-2);
}

int main(int argc, char **argv) {
  int ch;			// option character
  const char *imagename = 0;
  const char *outputname = 0;
  int histogram = -1;

  while((ch = getopt(argc, argv, "h:i:o:")) != -1) {
    switch(ch) {
    case 'i':
      imagename = optarg;
      break;

    case 'h':
      histogram = atoi(optarg);
      break;

    case 'o':
      outputname = optarg;
      break;

    case '?':
    default:
      usage();
    }
  }

  if(imagename == 0) usage();

  FILE *fp_out = (outputname ? fopen(outputname, "w") : stdout);

  Image image(imagename);

  Statistics *stat = image.statistics();

  const double lim_low = image.HistogramValue(0.2);
  const double lim_high = image.HistogramValue(0.8);

  int count = 0;
  // variance = (sum(x^2))/N - average^2
  double sum_x = 0.0;
  double sum_xsq = 0.0;

  for (int row=0; row<image.height; row++) {
    for (int col=0; col<image.width; col++) {
      const double v = image.pixel(col, row);
      if (v >= lim_low && v <= lim_high) {
	count++;
	sum_x += v;
	sum_xsq += (v*v);
      }
    }
  }
  const double average = sum_x/count;
  const double background_variance = sum_xsq/count - (average*average);
  const double background_rms = sqrt(background_variance);

  fprintf(fp_out, "HEIGHT=%d\n", image.height);
  fprintf(fp_out, "WIDTH=%d\n", image.width);
  fprintf(fp_out, "MAX=%.1lf\n", stat->BrightestPixel);
  fprintf(fp_out, "MIN=%.1lf\n", stat->DarkestPixel);
  fprintf(fp_out, "AVG=%.1lf\n", stat->AveragePixel);
  fprintf(fp_out, "NUM_SATURATED=%d\n", stat->num_saturated_pixels);
  fprintf(fp_out, "MEDIAN=%.1lf\n", stat->MedianPixel);
  fprintf(fp_out, "STDDEV=%.1lf\n", stat->StdDev);
  fprintf(fp_out, "BACKGROUND_STDDEV=%.1lf\n", background_rms);

  if (histogram > 0) {
    int counts[histogram] = {};

    for (int row=0; row<image.height; row++) {
      for (int col=0; col<image.width; col++) {
	const int v = (int) (0.5 + image.pixel(col, row));
	if (v < histogram) counts[v]++;
      }
    }

    for (int i=0; i<histogram; i++) {
      fprintf(fp_out, "%d, %d\n", i, counts[i]);
    }
  }
}
