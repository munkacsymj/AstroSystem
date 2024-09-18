/* This may look like C code, but it is really -*-c++-*- */
/*  egauss.h -- Gaussian convolution kernel
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


#include "params.h"
#ifndef _EGAUSS_H
#define _EGAUSS_H

struct EGParams {
  float a, b, c, f;		// ellipse parameters
  int nx, ny;			// dimensions of the kernel
  float *gkernel;		// index [y*width+x]
  float *ngkernel;		// index [y*width+x]
  float *dkernel;		// index [y*width+x]
  int *skip;			// index [y*width+x] -- skip subraster
  float gsums[LEN_GAUSS];	// see params.h for indices
  double relerr;
  int num_pts;
};

EGParams *SetupEGParams(float sigma, // sigma of gaussian in x
			float ratio, // ratio of half-width in y to x
			float theta, // position angle of gaussian
			float nsigma); // limit of convolution

double SetupKernel(EGParams &p);

#endif
