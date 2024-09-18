// This may look like C code, but it is really -*- C++ -*-
/*  refraction.h -- corrections based on atmospheric refraction
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

#ifndef _REFRACTION_H
#define _REFRACTION_H

#include "dec_ra.h"
#include "julian.h"

// zenith angle measures distance from the zenith in radians (0.0, at
// the zenith, will give a refraction error of 0.0 radians). This
// function returns an angle in radians.
double refraction_adjustment(double zenith_angle);

void refraction_true_to_obs(DEC_RA true_loc, DEC_RA &obs_loc, JULIAN when);
void refraction_obs_to_true(DEC_RA obs_loc, DEC_RA &true_loc, JULIAN when);

// refraction depends on atmospheric pressure and on temperate. Set
// them here. (Otherwise you get default values, which are reasonable
// and will give results within a few percent (generally much better
// than 1 arcmin accuracy).)
void set_refraction_temp(double temp_deg_C);
void set_refraction_pressure(double pressure_millibars);


#endif
