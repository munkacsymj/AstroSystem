/*  apconvolve.cc -- Gaussian convolution kernel
 *
 *  Copyright (C) 2021 Mark J. Munkacsy

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

#include <math.h>
#include <iostream>
#include <pthread.h>
#include <algorithm>		// std::max()
#include <stdio.h>
#include "egauss.h"
#include "apbfdfind.h"
#include "params.h"
#include "apconvolve.h"

#define NUM_THREADS 6

struct ThreadData {
  unsigned int thread_id;
  unsigned int thread_count;
  Image *image;
  Image *den;
  EGParams *gauss;
  RunParams *rp;
};

void *apfconvolve_thread(void *all_params) {
  // Pixel counting:
  // the pixel den.pixels[0,0] maps to the pixel
  // image.pixels[rp.boundary_x, rp.boundary_y]
  //
  // The kernel is gauss->nx pixels wide (an odd number), so it runs
  // from -nx/2 to +nx/2 (inclusive at each end)
  //
  //fprintf(stderr, "nx = %d, boundary_x = %d, image.width=%d\n",
  //	  gauss.nx, rp.boundary_x, image.width);
  //int lines = 10000;

  ThreadData *td = (ThreadData *) all_params;
  Image *den = td->den;
  Image *image = td->image;
  RunParams *rp = td->rp;
  EGParams *gauss = td->gauss;
  
  for (int y=td->thread_id; y<den->height; y+=td->thread_count) {
    // zero the output row
    for (int x=0; x<den->width; x++) {
      den->pixel(x,y) = 0.0;
    }
    const int src_low = y+rp->boundary_y-gauss->ny/2;	// y coordinate in image->pixels[]
    const int src_high = src_low + gauss->ny;
    for (int yy=src_low; yy<src_high; yy++) {
      const int kern_y = (yy-src_low)*gauss->nx;
      for (int x=0; x<den->width; x++) {
	const int src_x_low = x+rp->boundary_x-gauss->nx/2;
	const int src_x_high = src_x_low + gauss->nx;
	for (int xx=src_x_low; xx<src_x_high; xx++) {
	  const int kern_x=xx-src_x_low;
	  //fprintf(stdout, "x,y = [%d,%d], xx,yy = [%d,%d], kx,ky=[%d,%d]\n",
	  //	  x, y, xx, yy, (xx-src_x_low), (yy-src_low));
	  //if (lines-- == 0) return;
	  if (gauss->skip[kern_y+kern_x] == 0) {
	    den->pixel(x,y) += image->pixel(xx,yy)*gauss->ngkernel[kern_y+kern_x];
	  }
	}
      }
    }
  }
  return nullptr;
}


void apfconvolve(EGParams &gauss,
		 RunParams &rp,
		 Image &image,	// has boundary pixels
		 Image &den) {	// no boundary pixels
  pthread_t thread_ids[NUM_THREADS];
  ThreadData thread_data[NUM_THREADS];
  for (int i=0; i<NUM_THREADS; i++) {
    ThreadData *data = &thread_data[i];
    data->thread_id = i;
    data->thread_count = NUM_THREADS;
    data->image = &image;
    data->den = &den;
    data->gauss = &gauss;
    data->rp = &rp;

    int err = pthread_create(&thread_ids[i],
			     nullptr, // attributes for the thread
			     &apfconvolve_thread,
			     &thread_data[i]);
    if (err) {
      std::cerr << "Error creating thread in apconvolve.cc: %d\n" << err << std::endl;
    }
  }

  for (int i=0; i<NUM_THREADS; i++) {
    int s = pthread_join(thread_ids[i], nullptr);
    if (s != 0) {
      perror("pthread_join");
    }
  }
}
