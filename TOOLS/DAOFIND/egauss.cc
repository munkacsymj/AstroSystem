/*  egauss.cc -- Gaussian convolution kernel
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
#include <algorithm>		// std::max()
#include <stdio.h>
#include "egauss.h"

#define RMIN 2.001f // minimum gaussian radius

EGParams *SetupEGParams(float sigma, // sigma of gaussian in x
			float ratio, // ratio of half-width in y to x
			float theta, // position angle of gaussian
			float nsigma) { // limit of convolution
  const float sx2 = sigma*sigma;
  const float sy2 = (ratio * ratio * sigma * sigma);
  const float cost = cos(theta);
  const float sint = sin(theta);
  EGParams *gauss = new EGParams;

  // handle degenerate ellipse cases first
  if (ratio == 0.0) {
    if (sint == 0.0) {
      gauss->a = 1.0/sx2;
      gauss->b = 0.0;
      gauss->c = 0.0;
    } else if (cost == 0.0) {
      gauss->a = 0.0;
      gauss->b = 0.0;
      gauss->c = 1.0/sx2;
    } else {
      fprintf(stderr, "ERROR: Cannot make 1D Gaussian.\n");
      return nullptr;
    }
    gauss->f = nsigma * nsigma / 2.0;
    gauss->nx = 2 * (int)(std::max(sigma * nsigma * fabsf(cost), RMIN)) + 1;
    gauss->ny = 2 * (int)(std::max(sigma * nsigma * fabsf(sint), RMIN)) + 1;
  } else {
    // the non-degenerate case
    gauss->a = (cost * cost / sx2) + (sint * sint / sy2);
    gauss->b = 2.0 * (1.0/sx2 - 1.0/sy2) * (cost * sint);
    gauss->c = (sint*sint/sx2) + (cost * cost / sy2);
    double discrim = gauss->b * gauss->b - 4*gauss->a*gauss->c;
    gauss->f = nsigma*nsigma / 2.0;
    gauss->nx = 2 * (int) (std::max(sqrtf(-8*gauss->c*gauss->f/discrim), RMIN)) + 1;
    gauss->ny = 2 * (int) (std::max(sqrtf(-8*gauss->a*gauss->f/discrim), RMIN)) + 1;
  }
  return gauss;
}

double SetupKernel(EGParams &p) {
  const int kernel_size = p.nx * p.ny;
  p.gkernel = new float[kernel_size];
  p.ngkernel = new float[kernel_size];
  p.dkernel = new float[kernel_size];
  p.skip = new int[kernel_size];

  const int x0 = p.nx/2;
  const int y0 = p.ny/2;

  p.gsums[GAUSS_SUMG] = 0.0;
  p.gsums[GAUSS_SUMGSQ] = 0.0;
  int num_pts = 0;

  for (int j = 0; j < p.ny; j++) {
    const int y = j - y0;
    const float rjsq = y*y;
    for (int i=0; i < p.nx; i++) {
      const int index = j*p.nx + i;
      const int x = i - x0;
      const float rsq = sqrtf(x*x + rjsq);
      const float ef = (p.a * x*x + p.c * y*y + p.b * x*y)/2.0;
      p.gkernel[index] = exp(-ef);
      if (ef <= p.f || rsq <= RMIN) {
	p.ngkernel[index] = p.gkernel[index];
	p.dkernel[index] = 1.0;
	p.gsums[GAUSS_SUMG] += p.gkernel[index];
	p.gsums[GAUSS_SUMGSQ] += p.gkernel[index]*p.gkernel[index];
	p.skip[index] = 0;
	num_pts++;
      } else {
	p.ngkernel[index] = 0.0;
	p.dkernel[index] = 0.0;
	p.skip[index] = 1;
      }
      //printf("%d,%d,%lf\n", x, y, p.gkernel[index]);
    }
  }
  //printf("-----------------------\n");

  p.gsums[GAUSS_PIXELS] = num_pts;
  p.gsums[GAUSS_DENOM] = p.gsums[GAUSS_SUMGSQ] - p.gsums[GAUSS_SUMG]*p.gsums[GAUSS_SUMG]/num_pts;
  p.gsums[GAUSS_SGOP] = p.gsums[GAUSS_SUMG]/num_pts;
  p.num_pts = num_pts;

  // Normalize the amplitude kernel
  for (int i=0; i<p.nx*p.ny; i++) {
    if (p.skip[i] == 0) {
      p.ngkernel[i] = (p.gkernel[i] - p.gsums[GAUSS_SGOP])/p.gsums[GAUSS_DENOM];
      p.dkernel[i] = p.dkernel[i]/num_pts;
    }
  }

  double relerr = 1.0/p.gsums[GAUSS_DENOM];
  return sqrt(relerr);
}
