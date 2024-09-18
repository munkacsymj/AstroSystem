/*  aperture_phot.cc -- (Obsolete) Perform aperture photometry on stars
 *  in an image.
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
#include "aperture_phot.h"

static int dbl_cmp(const void *d1, const void *d2) {
  const double *dd1 = (double *) d1;
  const double *dd2 = (double *) d2;

  if(*dd1 < *dd2) return -1;
  
  return *dd1 > *dd2;
}

void aperture_measure(Image *primary_image, int star_id, IStarList *sl) {
  const double center_x = sl->StarCenterX(star_id);
  const double center_y = sl->StarCenterY(star_id);

  const int radius_aperture = 4;
  const int radius_annulus = 5;

  const int left_edge   = (int) (center_x - radius_annulus + 0.5);
  const int right_edge  = left_edge + radius_annulus*2;
  const int top_edge    = (int) (center_y - radius_annulus + 0.5);
  const int bottom_edge = top_edge + radius_annulus*2;

  if(left_edge < 0 || top_edge < 0 ||
     right_edge >= primary_image->width ||
     bottom_edge >= primary_image->height) return;

  int x, y;

  // 4 pixels radius
  const double r_center_sq = (double) radius_aperture*radius_aperture;
  const double r_annulus_sq = (double) radius_annulus*radius_annulus;

  // place to hold median data for the annulus
  double annulus_data[4*radius_annulus*radius_annulus];
  int annulus_count = 0;

  int count_in = 0;
  double star_count = 0.0;

  fprintf(stderr, "Center = (%f, %f)\n", center_x, center_y);
  fprintf(stderr, "Working in box[%d, %d][%d, %d]\n",
	  left_edge, right_edge, top_edge, bottom_edge);

  for(x=left_edge; x <= right_edge; x++) {
    for(y=top_edge; y <= bottom_edge; y++) {
      const double del_x = x-center_x;
      const double del_y = y-center_y;
      const double r_sq = del_x*del_x + del_y*del_y;

      if(r_sq > r_annulus_sq) continue;
      if(r_sq <= r_center_sq) {
	// pixel goes into star measurement
	star_count += primary_image->pixel(x, y);
	count_in++;
      } else {
	annulus_data[annulus_count++] = primary_image->pixel(x, y);
      }
    }
  }

  // compute the median of the annulus
  qsort(annulus_data, annulus_count, sizeof(double), dbl_cmp);
  
  double median = annulus_data[annulus_count/2];

  star_count -= (count_in * median);

  IStarList::IStarOneStar *my_star = sl->FindByIndex(star_id);
  my_star->validity_flags |= COUNTS_VALID;
  my_star->nlls_counts     = star_count;
}
  

  

  
