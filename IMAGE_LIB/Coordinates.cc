/*  Coordinates.cc -- convert between pixel and North/East (arcsec) coordinate
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
#include "Coordinates.h"

void PixelToOptical(double row,
		    double col,
		    double *EW,	// positive = EAST
		    double *NS) {
  *EW = (col - (378/2)) * 1.3805;
  *NS = (row - (242/2)) * 1.5998;
}

void RADecToOptical(DEC_RA *reference,
		    double RA_radians,
		    double DEC_radians,
		    double *EW,
		    double *NS) {
  double rect_factor = cos(reference->dec());

  *NS = (DEC_radians - reference->dec()) * (3600.0 * 180.0/M_PI);
  *EW = (RA_radians - reference->ra_radians()) *
    (3600.0 * 180.0/M_PI) * rect_factor;
}
