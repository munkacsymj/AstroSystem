/* This may look like C code, but it is really -*-c++-*- */
/*  bad_pixels.h -- Implements image drift management
 *
 *  Copyright (C) 2018, 2020 Mark J. Munkacsy

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
#ifndef _BAD_PIXELS_H
#define _BAD_PIXELS_H

#include <dec_ra.h>
#include <list>

class Image;
class IStarList;

struct OneDefect {
  int col;
  int row_start;
  bool single_pixel; // if true, row_end is undefined
  int row_end; // always >= row_start
};

typedef std::list<OneDefect *> DefectList;

class BadPixels {
 public:
  BadPixels(void);
  ~BadPixels(void);
  DefectList *GetDefects(void) { return all_defects; }

  DEC_RA UpdateTargetForBadPixels(Image *image, const char *object_name);
  

 private:

  struct Result {
    int shift_x, shift_y;
    double distance_from_zero;
    double worst_critical_distance;
    double worst_check_distance;
    bool IsBetterThan(Result &r);
  };

  double DistanceToClosestBadPixel(Image *i, bool mandatory, double x, double y);
  // The number returned here is good when it's big.
  Result ImageScore(Image *i, int offset_x = 0, int offset_y = 0);
  Result ImageScore(IStarList *sl, Image *i, int offset_x = 0, int offset_y = 0);

  DefectList *all_defects;
};

#endif

