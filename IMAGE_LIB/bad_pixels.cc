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

#include "bad_pixels.h"
#include "Image.h"
#include "IStarList.h"
#include "gendefs.h"
#include <math.h>
#include <stdio.h>
#include <HGSC.h>
#include <list>
#include <string.h>

BadPixels::BadPixels(void) {
  all_defects = new DefectList;

  FILE *fp = fopen(BAD_PIXEL_FILE, "r");
  if (!fp) {
    fprintf(stderr, "bad_pixels: No bad_pixel file found: %s\n",
	    BAD_PIXEL_FILE);
  } else {
    char buffer[80];
    while(fgets(buffer, sizeof(buffer), fp)) {
      for (char *s = buffer; *s; s++) {
	// Handle comments by terminating the line at the comment
	if (*s == '#') {
	  *s = 0;
	  break;
	}
      }

      // remove spaces from the line
      char *d = buffer;
      for (const char *s = buffer; *s; s++) {
	if (*s != ' ' && *s != '\n') *d++ = *s;
      }
      *d = 0; // terminate the string
      
      if (buffer[0] == 0) continue; // go to next line

      OneDefect *one = new OneDefect;
      int num_fields = sscanf(buffer, "%d,%d-%d",
			      &one->col,
			      &one->row_start,
			      &one->row_end);
      if (num_fields == 2) {
	one->single_pixel = true;
      } else if (num_fields == 3) {
	one->single_pixel = false;
      } else {
	fprintf(stderr, "Improper bad_pixel line: %s\n", buffer);
      }
      all_defects->push_back(one);
    }
    fclose(fp);
  }
}

BadPixels::~BadPixels(void) {
  DefectList::iterator it;
  for (it = all_defects->begin(); it != all_defects->end(); it++) {
    delete *it;
  }
  delete all_defects;
}

  // The number returned here is good when it's big.
BadPixels::Result
BadPixels::ImageScore(Image *i, int offset_x, int offset_y) {
  return ImageScore(i->PassiveGetIStarList(), i, offset_x, offset_y);
}

// When mandatory is set, point (x,y) must stay away from the image edge.
double
BadPixels::DistanceToClosestBadPixel(Image *i, bool mandatory, double x, double y) {
  double closest = 9.9e99;
  // PART 1: Find distance to nearest bad pixel
  for (auto bp : *all_defects) {
    // simple case first
    double distance_sq = 0.0;
    if (bp->single_pixel) {
      const double delta_x = x - bp->col;
      const double delta_y = y - bp->row_start;
      distance_sq = delta_x * delta_x + delta_y * delta_y;
    } else {
      double point_y;
      
      // not a single pixel; three cases to consider
      if (y < bp->row_start) {
	point_y = bp->row_start;
      } else if (y > bp->row_end) {
	point_y = bp->row_end;
      } else {
	point_y = y;
      }
      const double delta_x = x - bp->col;
      const double delta_y = y - point_y;
      distance_sq = delta_x * delta_x + delta_y * delta_y;
    }

    if (distance_sq < closest) {
      closest = distance_sq;
    }
  }
  closest = sqrt(closest);
  
  // PART 2: Check on distance to nearst image edge
  double left = x;
  double right = i->width - x;
  double top = y;
  double bottom = i->height - y;

  double edge_min = left;
  if (right < edge_min) edge_min = right;
  if (top < edge_min) edge_min = top;
  if (bottom < edge_min) edge_min = bottom;

  // PART 3:
  if (edge_min < closest || (mandatory && edge_min < 40.0)) {
    return edge_min;
  }
  return closest;
}

constexpr static double IMAGE_EDGE_MARGIN = 25.0; // stars this close to an
					     // edge don't count

bool
BadPixels::Result::IsBetterThan(Result &r) {
  // returns true if "self" is prefered to "r"
  if (this->worst_critical_distance > IMAGE_EDGE_MARGIN and
      r.worst_critical_distance > IMAGE_EDGE_MARGIN) {
    return (this->worst_check_distance > r.worst_check_distance);
  }
  return (this->worst_critical_distance > r.worst_critical_distance);
}

BadPixels::Result
BadPixels::ImageScore(IStarList *sl, Image *image, int offset_x, int offset_y) {
  const int width = image->width;
  const int height = image->height;
  Result result;
  result.shift_x = offset_x;
  result.shift_y = offset_y;
  result.worst_critical_distance = 9.9e99;
  result.worst_check_distance = 9.9e99;
  result.distance_from_zero = sqrt(offset_x*offset_x+offset_y*offset_y);

  // for each star, find the closest bad pixel
  for (int i=0; i<sl->NumStars; i++) {
    IStarList::IStarOneStar *star = sl->FindByIndex(i);
    const double this_x = star->StarCenterX()+offset_x;
    const double this_y = star->StarCenterY()+offset_y;
    bool inframe = (star->info_flags & STAR_IS_INFRAME);
    bool mandatory = (star->info_flags & (STAR_IS_COMP|STAR_IS_SUBMIT));
    if (this_x < 0.0 or this_y < 0.0 or
	this_x >= width or this_y >= height) {
      // off-chip
      if (mandatory and inframe) {
	result.worst_critical_distance = 0.0;
	result.worst_check_distance = 0.0;
	return result;
      } else {
	continue; // next star; don't consider this re distances
      }
    } else { // else is on-chip
      if (star->validity_flags & CORRELATED) {
	double bad_distance = DistanceToClosestBadPixel(image, mandatory, this_x, this_y);
	if (mandatory) {
	  if (bad_distance < result.worst_critical_distance) {
	    result.worst_critical_distance = bad_distance;
	  }
	} else {
	  if (bad_distance < result.worst_check_distance) {
	    result.worst_check_distance = bad_distance;
	  }
	}
      }
    }
  }
  return result;
}

DEC_RA
BadPixels::UpdateTargetForBadPixels(Image *image, const char *object_name) {

  //********************************
  // Part 1: Build an IStarList from the catalog
  //********************************
  IStarList isl;
  HGSCList catalog(object_name);
  fprintf(stdout, "Catalog fetch for %s: completed.\n", object_name);
  
  const WCS *wcs = image->GetImageInfo()->GetWCS();
  HGSCIterator it(catalog);
  for (HGSC *hgsc = it.First(); hgsc; hgsc = it.Next()) {
    if (hgsc->is_comp || hgsc->is_check || hgsc->do_submit) {
      IStarList::IStarOneStar *star = new IStarList::IStarOneStar;
      strcpy(star->StarName, hgsc->label);
      star->validity_flags = (NLLS_FOR_XY | CORRELATED);
      star->info_flags = 0;
      bool mandatory = false;
      if (hgsc->is_comp) {
	star->info_flags |= STAR_IS_COMP;
	fprintf(stdout, "P");
	mandatory = true;
      }
      if (hgsc->is_check) {
	star->info_flags |= STAR_IS_CHECK;
	fprintf(stdout, "K");
      }
      if (hgsc->do_submit) {
	star->info_flags |= STAR_IS_SUBMIT;
	fprintf(stdout, "S");
	mandatory = true;
      }
      // Be aware that this IStarList contains catalog stars that fall
      // well outside the boundaries of the image. 
      wcs->Transform(&hgsc->location, &star->nlls_x, &star->nlls_y);
      if (mandatory && star->nlls_x >= 0.0 && star->nlls_y >= 0.0 &&
	  star->nlls_x <= image->width && star->nlls_y <= image->height) {
	star->info_flags |= STAR_IS_INFRAME;
      }

      isl.IStarAdd(star);
    }
  }
  fprintf(stdout, "\n");
  fprintf(stdout, "IStarList contains %d stars.\n",
	  isl.NumStars);
  
  //********************************
  // Part 3: Set the image shift limits (in pixels)
  //********************************
  int max_shift_right = 80; // this is about 2 arcmin
  int max_shift_left = 80;
  int max_shift_up = 80;
  int max_shift_down = 80;

  fprintf(stdout, "shift limits x = (-%d, %d)\nshift limits y = (-%d, %d)\n",
	  max_shift_left, max_shift_right,
	  max_shift_down, max_shift_up);
  fprintf(stdout, "Part 3 completed.\n");
  fprintf(stdout, "----------------\n");

  //********************************
  // Part 4: Test a whole bunch of image shifts
  //********************************

  Result best_result;
  best_result.worst_critical_distance = 0.0;
  best_result.worst_check_distance = 0.0;
  constexpr int SKIP=6;

  for (int y = -max_shift_down; y < max_shift_up; y += SKIP) {
    for (int x = -max_shift_left; x < max_shift_right; x += SKIP) {
      Result result = ImageScore(&isl, image, x, y);

      if (result.IsBetterThan(best_result)) {
	best_result = result;
      }
    }
  }

  fprintf(stdout, "Best offset info:\n");
  fprintf(stdout, "  offset_x = %d, offset_y = %d\n", best_result.shift_x, best_result.shift_y);
  fprintf(stdout, "  score = %.2lf/%.2lf\n",
	  best_result.worst_critical_distance, best_result.worst_check_distance);

  //********************************
  // Part 6: Turn the offset into a DEC_RA
  //********************************
  double target_x;
  double target_y;
  DEC_RA original_target = wcs->Transform(image->width/2, image->height/2);
  wcs->Transform(&original_target, &target_x, &target_y);

  target_x -= best_result.shift_x;
  target_y -= best_result.shift_y;
  if (isnormal(target_x) and isnormal(target_y)) {
    return wcs->Transform(target_x, target_y);
  } else {
    fprintf(stderr, "bad_pixels: invalid result.\n");
    return original_target;
  }
}

