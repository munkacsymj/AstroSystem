/* This may look like C code, but it is really -*-c++-*- */
/*  Statistics.cc -- pixel statistics in an image, including
 *  histogram-based statistics 
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

#ifndef _STATISTICS_H
#define _STATISTICS_H

class Statistics {
public:
  double DarkestPixel;
  double BrightestPixel;
  double AveragePixel;

  int num_saturated_pixels;

  double MedianPixel;

  double StdDev;
};

void *Median(void *base,
	     int nelements,
	     int elementSize,
	     int (*Compare)(const void *, const void *));

// This is a more generic form of Median(). Instead of finding the
// median (the point with 50% of the sample at a lower value). In this
// form, we specify "lower_limit", which must be in the range of
// 0..nelements. A pointer to the element at that point is returned.
void *HistogramPoint(void *base,
		     int nelements,
		     int elementSize,
		     int (*Compare)(const void *, const void *),
		     int lower_limit);

#endif
