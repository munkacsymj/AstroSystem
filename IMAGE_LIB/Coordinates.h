/* This may look like C code, but it is really -*-c++-*- */
/*  Coordinates.h -- convert between pixel and North/East (arcsec) coordinate
 *  systems (only approximately)
 *
 *  Copyright (C) 2007 Mark J. Munkacsy

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
#ifndef _COORDINATES_H
#define _COORDINATES_H
#include <dec_ra.h>

void PixelToOptical(double row,
		    double col,
		    double *EW,
		    double *NS);

void RADecToOptical(DEC_RA *reference,
		    double RA_radians,
		    double DEC_radians,
		    double *EW,
		    double *NS);

#endif
