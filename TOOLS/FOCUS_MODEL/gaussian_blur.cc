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
#include <pthread.h>
#include <errno.h>
#include "gaussian_blur.h"

double gaussian (double x, double mu, double sigma) {
  return exp( -(((x-mu)/(sigma))*((x-mu)/(sigma)))/2.0 );
}

struct KernelData {
  const Image *orig;
  const Image *kernel;
  Image *result;
  int num_workers;
} kernel_worker_data;

void *apply_kernel_worker(void *my_id) {
  const Image *orig = kernel_worker_data.orig;
  const Image *kernel = kernel_worker_data.kernel;
  Image *result = kernel_worker_data.result;
  int num_workers = kernel_worker_data.num_workers;
  const int center_x = kernel->width/2;
  const int center_y = kernel->height/2;
  const int my_id_no = *((int *) my_id);
  
  //for (int row = center_y+my_id; row < orig->height-center_y; row+=num_workers) {
  //for (int col = center_x; col < orig->width-center_x; col++) {
  for (int row = my_id_no; row < orig->height; row+=num_workers) {
    for (int col = 0; col < orig->width; col++) {
      // row/col is the address of the destination. s_row/s_col are
      // the row/col of the source pixel. k_row/k_col are the row/col
      // of the kernel

      double pixel_value = 0.0;
      
      for (int k_row = 0; k_row < kernel->height; k_row++) {
	for (int k_col = 0; k_col < kernel->width; k_col++) {
	  //int s_row, s_col;
	  int s_row = row + k_row - center_y;
	  int s_col = col + k_col - center_x;
	  if (s_row < 0) s_row = 0;
	  if (s_col < 0) s_col = 0;
	  if (s_row >= orig->height) s_row = (orig->height-1);
	  if (s_col >= orig->width) s_col = (orig->width-1);
	  pixel_value += orig->pixel(s_col, s_row) *
	    kernel->pixel(k_col, k_row);
	}
      }
      result->pixel(col, row) = pixel_value;
    }
  }
  return nullptr;
}

Image *apply_kernel(const Image *orig,
		    const Image *kernel) {
  
  constexpr int num_workers = 6;
  pthread_t thread_ids[num_workers];
  int worker_numbers[num_workers];

  kernel_worker_data.orig = orig;
  kernel_worker_data.kernel = kernel;
  kernel_worker_data.result = new Image(orig->height, orig->width);
  kernel_worker_data.num_workers = num_workers;

  for (int i=0; i<num_workers; i++) {
    worker_numbers[i] = i;
    int err = pthread_create(&thread_ids[i],
			     nullptr,
			     &apply_kernel_worker,
			     &worker_numbers[i]);
    if (err) {
      fprintf(stderr, "Error creating thread in gaussian_blur: %d\n", err);
    }
  }

  for (int i=0; i<num_workers; i++) {
    void *res;
    int s = pthread_join(thread_ids[i], &res);
    if (s != 0) {
      perror("pthread_join");
    }
  }
  
  return kernel_worker_data.result;
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
  Image kernel(kernel_size, kernel_size);

  int row, col;
  const double center_x = x;
  const double center_y = x;
  double sum = 0.0;
  for (row = 0; row < kernel_size; row++) {
    for (col = 0; col < kernel_size; col++) {
      const double pixel_value = gaussian((double) row, center_x, sigma) *
	gaussian((double) col, center_y, sigma);
      kernel.pixel(col, row) = pixel_value;
      sum += pixel_value;
    }
  }

  // and normalize
  // fprintf(stderr, "Gaussian blur kernel is %dx%d\n", kernel_size, kernel_size);
  for (row = 0; row < kernel_size; row++) {
    for (col = 0; col < kernel_size; col++) {
      kernel.pixel(col, row) /= sum;
      // fprintf(stderr, "%7.3f ", kernel.pixel(col, row));
    }
    // fprintf(stderr, "\n");
  }
  kernel.WriteFITS("/tmp/blur_kernel.fits");

  /****************************************************************/
  /*        Blur kernel is now ready to use                       */
  /****************************************************************/
  Image *result =  apply_kernel(orig, &kernel);
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

  Image *result =  apply_kernel(orig, &kernel);
  return result;
}

