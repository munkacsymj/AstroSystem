/*  focus.h -- Program to perform auto-focus
 *
 *  Copyright (C) 2007 Mark J. Munkacsy
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
#ifndef _FOCUS_H
#define _FOCUS_H

void
focus(Image *initial_image,
      double exposure_time_val,
      long initial_encoder,
      int focus_time,
      Image *dark_image,
      Filter filter);

// only used for developmental testing
void do_special_test(void);

// Directions
extern int preferred_direction;
#define DIRECTION_POSITIVE 1
#define DIRECTION_NEGATIVE 2

#endif
