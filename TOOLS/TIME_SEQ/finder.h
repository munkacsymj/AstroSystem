/* This may look like C code, but it is really -*-c++-*- */
/*  focus_simulator.h -- operations to manage focus using measured bluring
 *  of star images 
 *
 *  Copyright (C) 2018 Mark J. Munkacsy

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
#ifndef _FINDER_H
#define _FINDER_H

#include <dec_ra.h>
#include <Filter.h>

int
Finder(const char *object_name,
       DEC_RA &target_location,
       double tolerance /*arc-sec*/,
       Filter &filter);

#endif
