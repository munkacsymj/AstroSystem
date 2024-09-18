/*  fine_focus.cc
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

#include <gaussian_fit.h>
#include <Image.h>
#include <Statistics.h>
#include <math.h>
#include <unistd.h>
#include <iostream>
using namespace std;

void usage(void) {
  cerr << "usage: fine_focus -i image.fits\n";
  exit(-2);
}

int find_blob(Image &i, double *x_center, double *y_center) {
  double brightest = -99e99;
  *x_center = *y_center = -1.0;
  
  for (int x=0; x<i.width; x++) {
    for (int y=0; y<i.height; y++) {
      const double v = i.pixel(x,y);
      if (v > brightest) {
	brightest = v;
	*x_center = x;
	*y_center = y;
	//cout << "Center set to ("
	//   << *x_center << ", "
	//   << *y_center << ")\n";
      }
    }
  }
  constexpr int offset = 10;
  int subimage_left = (*x_center) - offset;
  int subimage_right = (*x_center) + offset;
  int subimage_bottom = (*y_center) - offset;
  int subimage_top = (*y_center) + offset;

  //cout << "box = [" << subimage_left << ", "
  //   << subimage_right << "] x ["
  //   << subimage_bottom << ", "
  //   << subimage_top << "]\n";

  double centroid_x = 0.0;
  double centroid_y = 0.0;
  double sum_pixels = 0.0;
  for (int x=subimage_left; x<subimage_right; x++) {
    for (int y=subimage_bottom; y < subimage_top; y++) {
      if (x < 0 or y < 0 or x >= i.width or y >= i.height) continue;
      const double v = i.pixel(x,y);
      const double del_x = x - *x_center;
      const double del_y = y - *y_center;
      centroid_x += (del_x * v);
      centroid_y += (del_y * v);
      sum_pixels += v;
    }
  }
  double x_adj = centroid_x/sum_pixels;
  double y_adj = centroid_y/sum_pixels;
  //cout << "centroid adjustment = ("
  //   << x_adj << ", " << y_adj << ")\n";
  *x_center += x_adj;
  *y_center += y_adj;
  return 0;
}

int main(int argc, char **argv) {
  int ch;
  const char *imagefile = 0;

  while((ch = getopt(argc, argv, "i:")) != -1) {
    switch(ch) {
    case 'i':
      imagefile = optarg;
      break;

    case '?':
    default:
      usage();
    }
  }

  if (imagefile == 0) {
    usage();
  }

  Image image(imagefile);
  double x_center, y_center;
  const double median = image.statistics()->MedianPixel;

  find_blob(image, &x_center, &y_center);

  constexpr double max_r = 10;
  GRunData points;
  points.reset();
  
  for (int x=(int)(x_center - max_r); x <= (int)(x_center+max_r); x++) {
    for (int y=(int)(y_center - max_r); y <= (int)(y_center+max_r); y++) {
      const double value = image.pixel(x,y) - median;
      const double del_x = (x_center - x);
      const double del_y = (y_center - y);
      const double r = sqrt(del_x*del_x + del_y*del_y);
      if (r < max_r) {
	//cout << "Adding point: r = " << r
	//   << ", value = " << value << endl;
	points.add(r, value);
      }
    }
  }

  Gaussian g;
  g.reset();
  const int status = nlls_gaussian(&g, &points);
  cout << "Status = " << (status == 0 ? "Okay" : "No converge") << endl;
  cout << "Scaling = " << g.state_var[0] << endl;
  cout << "Shape = " << g.state_var[1] << endl;
  return 0;
}
  
