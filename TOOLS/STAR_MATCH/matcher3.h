/* This may look like C code, but it is really -*-c++-*- */
/*  matcher.h -- Correlate stars in an image with a catalog
 *
 *  Copyright (C) 2022 Mark J. Munkacsy
 *
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

#ifndef _MATCHER_H
#define _MATCHER_H

#include <wcs.h>
#include <list>
#include "correlate_internal2.h"
#include "correlate2.h"

class Grid;
// Returns the number of successful matches
int
Matcher(Context *context,
	Grid *grid,
	const WCS &wcs,
	std::vector<CAT_DATA *> &cat_list,
	std::vector<IMG_DATA *> &image_list,
	int num_img_to_use,
	double tolerance,	// arcseconds
	bool do_fixup);		// setup Dec/RA on stars that don't have matches



class Grid;
Grid *InitializeGrid(Context *context,
		     std::vector<CAT_DATA *> &cat_list,
		     double coarse_tolerance);

// CalculateWCS: Calculate a new WCS using the matches contained in
// the two star lists. This is a relatively expensive computation, so
// use sparingly. This is thread-safe as long as the caller has a
// dedicated cat_list and image_list for this thread.

WCS_Bilinear *
CalculateWCS(Context *context,
	     std::vector<CAT_DATA *> &cat_list,
	     std::vector<IMG_DATA *> &image_list,
	     const char *residual_filename);

void ComputeStatistics(std::vector<IMG_DATA *> &stars,
		       ResidualStatistics &stats);

#endif
