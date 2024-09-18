/* This may look like C code, but it is really -*-c++-*- */
/*  nlls_general.h -- Non-linear least-squares modeling of star point-
 *  spread-function (PSF)
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
#ifndef _NLLS_GENERAL_H
#define _NLLS_GENERAL_H

#include "Image.h"
#include "IStarList.h"

// returns -1 if would not converge
// returns 0 if converged okay
int nlls(Image *primary_image, int star_id, IStarList *sl);

#endif
