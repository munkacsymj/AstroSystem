/*  measure_glow.cc -- 
 *
 *  Copyright (C) 2020 Mark J. Munkacsy

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

#include <Image.h>
#include <Statistics.h>
#include <HGSC.h>
#include <stdlib.h>		// exit()
#include <unistd.h>		// getopt()
#include <cstring>
#include <string>
#include <math.h>

int main(int argc, char **argv) {
  int ch;			// option character
  const char *filename = 0;

  // Command line options:
  // -i imagefile.fits

  while((ch = getopt(argc, argv, "i:")) != -1) {
    switch(ch) {
    case 'i':
      filename = optarg;
      break;

    case '?':
    default:
      fprintf(stderr, "usage: measure_banding -i filename\n");
      exit(-2);
   }
  }

  if (!filename) {
    fprintf(stderr, "usage: measure_banding -i filename\n");
    exit(-2);
  }

  Image image(filename);
  double full_sum = 0.0;

  double row_avg[image.height];
  for (int row=0; row < image.height; row++) {
    double sum = 0.0;
    for (int col=0; col < image.width; col++) {
      sum += image.pixel(col, row);
    }
    row_avg[row] = sum/image.width;
    full_sum += row_avg[row];
  }

  const double full_avg = full_sum/image.height;

  double sumsq = 0.0;
  for (int row=0; row < image.height; row++) {
    const double err = row_avg[row] - full_avg;
    sumsq += (err*err);
  }

  fprintf(stderr, "%s: banding = %lf\n", filename, sqrt(sumsq/image.height));
  return 0;
}
  
