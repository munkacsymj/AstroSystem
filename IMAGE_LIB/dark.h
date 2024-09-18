/* This may look like C code, but it is really -*-c++-*- */
/*  dark.h -- Program to manage darks for a session
 *
 *  Copyright (C) 2024 Mark J. Munkacsy
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

#ifndef _DARK_H
#define _DARK_H

#include <camera_api.h>

// exposure_time has a granularity of 1msec. Any attempt to use time
// increments smaller than that will be ignored.
const char *GetDark(double exposure_time,
		    int quantity, exposure_flags *flags=nullptr, const char *image_dir=nullptr);

#endif
