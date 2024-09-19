/*  aperture_phot.cc -- Perform aperture photometry on a star in an image
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

  double pixel_scale = 1.52;
  ImageInfo *info = primary_image->GetImageInfo();
  if (info and info->CDeltValid()) pixel_scale = info->GetCDelt1();

  constexpr double aperture_radius_arcsec = 6.0;
  constexpr double annulus_radius_arcsec = 10.0;

  const double radius_aperture = aperture_radius_arcsec/pixel_scale;
  const double radius_annulus = annulus_radius_arcsec/pixel_scale;

  const int left_edge   = (int) (center_x - radius_annulus + 0.5);
  const int right_edge  = left_edge + (int) (0.5 + radius_annulus*2);
  const int top_edge    = (int) (center_y - radius_annulus + 0.5);
  const int bottom_edge = top_edge + (int) (0.5 + radius_annulus*2);

  if(left_edge < 0 || top_edge < 0 ||
     right_edge >= primary_image->width ||
     bottom_edge >= primary_image->height) return;

  int x, y;

  // 3 arcsec radius
  const double r_center_sq = radius_aperture*radius_aperture;
  const double r_annulus_sq = radius_annulus*radius_annulus;

  // place to hold median data for the annulus
  double annulus_data[1 + (int) (4*r_annulus_sq)];
  int annulus_count = 0;

  int count_in = 0;
  double star_count = 0.0;

  // fprintf(stderr, "Center = (%f, %f)\n", center_x, center_y);
  // fprintf(stderr, "Working in box[%d, %d][%d, %d]\n",
  // left_edge, right_edge, top_edge, bottom_edge);

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
  

  

  
