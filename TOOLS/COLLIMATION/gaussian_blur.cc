/*  gaussian_blur.cc -- blur an image using a gaussian blur function
 *
 *  Copyright (C) 2015 Mark J. Munkacsy

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
#include "gaussian_blur.h"

double gaussian (double x, double mu, double sigma) {
  return exp( -(((x-mu)/(sigma))*((x-mu)/(sigma)))/2.0 );
}

Image *apply_kernel(const Image *orig,
		    const double *kernel,
		    int kernel_size) {
  const int center = kernel_size/2;

  Image *result = new Image(orig->height, orig->width);

  for (int row = 0; row < orig->height; row++) {
    for (int col = 0; col < orig->width; col++) {
      double pixel_value = 0.0;

      for (int k_i = 0; k_i < kernel_size; k_i++) {
	int s_col = col + k_i - center;
	if (s_col < 0) s_col = 0;
	if (s_col >= orig->width) s_col = (orig->width-1);
	pixel_value += orig->pixel(s_col, row) * kernel[k_i];
      }
      result->pixel(col, row) = pixel_value;
    }
  }

  for (int col = 0; col < orig->width; col++) {
    for (int row = 0; row < orig->height; row++) {
      double pixel_value = 0.0;

      for (int k_i = 0; k_i < kernel_size; k_i++) {
	int s_row = row + k_i - center;
	if (s_row < 0) s_row = 0;
	if (s_row >= orig->height) s_row = (orig->height-1);
	pixel_value += orig->pixel(col, s_row) * kernel[k_i];
      }
      result->pixel(col, row) = pixel_value;
    }
  }

  return result;
}

Image *apply_blur(const Image *orig,
		  double sigma) {
  // first pick the size of the blur kernel. To do this, we increase
  // "x" until the value of the gaussian has fallen to a really small
  // number. We choose 0.001 as our "small number".
  int x;
  for (x=0; x<25; x++) {
    double g_value = gaussian((double) x, 0.0, sigma);
    if (g_value < 0.01) break;
  }

  // The size of the blur kernel will be 2X + 1
  int kernel_size = 2*x + 1;
  //fprintf(stderr, "gaussian blur kernel size = [%d, %d]\n",
  //	  kernel_size, kernel_size);
  double kernel[kernel_size];

  const double center = x;
  double sum = 0.0;
  for (int i=0; i<kernel_size; i++) {
    const double pixel_value = gaussian((double) i, center, sigma);
    kernel[i] = pixel_value;
    sum += pixel_value;
  }
  
  // and normalize
  // fprintf(stderr, "Gaussian blur kernel is %dx%d\n", kernel_size, kernel_size);
  for (int i=0; i<kernel_size; i++) {
    kernel[i] /= sum;
  }
  
  /****************************************************************/
  /*        Blur kernel is now ready to use                       */
  /****************************************************************/
  Image *result =  apply_kernel(orig, kernel, kernel_size);
  return result;
}

Image *apply_tracking_smear(const Image *orig,
			    double width) { // width in pixels
  // first pick the size of the blur kernel. 
  int x;
  x = (int) (width + 1.0);
  // and round up until odd
  x++;
  if (x % 2 == 0) x++;

  // there are two weights needed -- the weights of the two end pixels
  // and the weight applied to all the in-between pixels.
  Image kernel(1, x); // height is 1, width was determined earlier
  for (int j=0; j<x; j++) {
    kernel.pixel(j,0) = 1.0;
  }

  double end_value = 1.0 + (width - x)/2.0;
  kernel.pixel(0,0) = end_value;
  kernel.pixel(0, x-1) = end_value;
  const double sum = (x-2)*1.0 + 2*end_value;

  // and normalize
  for (int col = 0; col < x; col++) {
    kernel.pixel(col, 0) /= sum;
  }
  fprintf(stderr, "Tracking smear kernel is %dx%d\n",
	  kernel.width, kernel.height);

  /****************************************************************/
  /*        Blur kernel is now ready to use                       */
  /****************************************************************/

  //Image *result =  apply_kernel(orig, &kernel);
  //return result;
  return 0;
}

