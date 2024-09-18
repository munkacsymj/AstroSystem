/* This may look like C code, but it is really -*-c++-*- */
/*  params.h -- Global input parameters
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
#ifndef _PARAMS_H
#define _PARAMS_H

#include <list>
#include <vector>
#include <Image.h>

struct EGParams;		// forward declaration

#define LEN_GAUSS              7

#define	GAUSS_SUMG		0
#define	GAUSS_SUMGSQ		1
#define	GAUSS_PIXELS		2
#define	GAUSS_DENOM		3
#define	GAUSS_SGOP		4

struct RunParams {
  double fwhm_psf;		// user input
  double threshold;		//
  double nsigma;		// size of the gaussian kernel
  double ratio;			// for elliptical PSF; y/x ratio
  double theta;			// for elliptical PSF; radians
  double data_min;		// min valid pixel value
  double data_max;		// max valid pixel value
  double gain_e_per_ADU;
  double readnoise;
  int boundary_x;		// number of extra columns on each side
  int boundary_y;		// number of extra rows at top/bottom
  Image *convolution;		// the convolved image
  EGParams *gauss;		// gaussian parameters and kernels
  double median;		// original image median value
  double sharplo;		// min value of "sharp"
  double sharphi;		// max value of "sharp"
  double roundlo;		// min value of "round"
  double roundhi;		// max value of "round"
};

struct DAOStar {
  double x, y;			// centroid location
  int nx, ny;			// intensity peak
  double mag;
  double sharp;
  double round1;
  double round2;
  double peak_value;
  bool valid;
};

typedef std::vector<DAOStar *> DAOStarlist;

#endif
