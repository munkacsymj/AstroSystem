/* This may look like C code, but it is really -*-c++-*- */
/*  apbfdfind.h -- Top-level algorithm
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
#ifndef _APBFDFIND_H
#define _APBFDFIND_H

#include "params.h"
#include <Image.h>

void ap_bfdfind(Image &im,	// input image
		RunParams &rp,	// input params, from params.h
		DAOStarlist &stars); // output starlist, from params.h

void ap_detect(Image &cnv,	// convolved image
	       EGParams &gauss,
	       RunParams &rp,
	       DAOStarlist &stars,
	       int *rows_to_exclude);

void ap_sharp_round(DAOStarlist &stars, Image &image, RunParams &rp);
void ap_xy_round(DAOStarlist &stars, Image &image, RunParams &rp);
void ap_test(DAOStarlist &stars, Image &image, RunParams &rp);


#endif
  
