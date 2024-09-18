/* This may look like C code, but it is really -*-c++-*- */
/*  apconvolve.h -- Gaussian convolution kernel
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
#ifndef _APCONVOLVE_H
#define _APCONVOLVE_H

#include "params.h"
#include "egauss.h"
#include <Image.h>

void apfconvolve(EGParams &gauss,
		 RunParams &rp,
		 Image &image,
		 Image &den);

#endif
