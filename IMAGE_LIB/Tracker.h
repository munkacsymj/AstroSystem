/* This may look like C code, but it is really -*-c++-*- */
/*  Tracker.h -- implements crude star-tracker to support PEC
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
#ifndef _TRACKER_H
#define _TRACKER_H

#include "Image.h"

#define TRACKER_LOCK	0
#define LOST_LOCK_TEMP	1
#define NO_LOCK 	2

class Tracker {
public:
  Tracker(Image *image);
  ~Tracker(void);

  void Update(Image *image, int depth=0);

  int TrackerStatus(void) { return tracker_status; }

  // Returns -1 if x_pos and y_pos invalid. Returns 0 if okay.
  int Position(double *x_pos,
	       double *y_pos);
private:
  double Current_pos_x;
  double Current_pos_y;

  int box_left;			// coordinates in pixels of the bounding
  int box_right;		// box holding the guiding star.
  int box_top;
  int box_bottom;

  int tracker_status;
};

#endif
