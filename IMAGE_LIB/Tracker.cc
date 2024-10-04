/*  Tracker.cc -- implements crude star-tracker to support PEC
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
#include "Tracker.h"
#include "Image.h"

#define MIN_PIXELS_IN_STAR_FOR_TRACKING 4

Tracker::Tracker(Image *image) {
  IStarList *starList = image->GetIStarList();
  int StarIndex = -1;
  double BrightestStarSum = -1.0;

  int star;

  for(star = 0; star < starList->NumStars; star++) {
    if(starList->IStarNumberPixels(star) < MIN_PIXELS_IN_STAR_FOR_TRACKING)
      continue;
    if(starList->IStarPixelSum(star) > BrightestStarSum) {
      BrightestStarSum = starList->IStarPixelSum(star);
      StarIndex = star;
    }
  }

  if(StarIndex < 0) {
    tracker_status = NO_LOCK;
  } else {
    tracker_status = TRACKER_LOCK;

    // This is just an initial value; will be replaced
    Current_pos_x = starList->StarCenterX(StarIndex);
    Current_pos_y = starList->StarCenterY(StarIndex);

    fprintf(stderr, "Tracker: initial X = %f, initial Y = %f\n",
	    Current_pos_x, Current_pos_y);

#define TRACKER_BOX_RADIUS_PIXELS 8

    box_left   = (int) Current_pos_x - TRACKER_BOX_RADIUS_PIXELS;
    box_right  = (int) Current_pos_x + TRACKER_BOX_RADIUS_PIXELS;
    box_top    = (int) Current_pos_y + TRACKER_BOX_RADIUS_PIXELS;
    box_bottom = (int) Current_pos_y - TRACKER_BOX_RADIUS_PIXELS;
    
    Update(image, 0);
  }
}

void
Tracker::Update(Image *image, int depth) {
  double x_weighted_sum = 0.0;
  double y_weighted_sum = 0.0;
  double total_pixel_sum = 0.0;
  double brightest_tracking_pixel = -1000000.0;
  // pixel_offset is sort of a floor intensity.  It is subtracted from
  // each pixel value. We use the median pixel value in the tracking
  // box.
  double pixel_offset;
  int bright_x {0}, bright_y {0};
  int x, y;

  // detect and halt oscillation
  if(depth > 3) return;

  Image *subImage = image->CreateSubImage(box_bottom,
					  box_left,
					  TRACKER_BOX_RADIUS_PIXELS*2,
					  TRACKER_BOX_RADIUS_PIXELS*2);

  pixel_offset = subImage->statistics()->MedianPixel;
  fprintf(stderr, "Median subimage value = %f\n", pixel_offset);
  fprintf(stderr, "box_left = %d, box_bottom = %d\n",
	  box_left, box_bottom);
  subImage->PrintImage(stderr);
  
  for(y=0; y < subImage->height; y++) {
    for(x=0; x < subImage->width; x++) {
      double pixel_value = subImage->pixel(x, y) - pixel_offset;

      if(pixel_value > brightest_tracking_pixel) {
	bright_x = x;
	bright_y = y;
	brightest_tracking_pixel = pixel_value;
      }
      x_weighted_sum += (x)*pixel_value;
      y_weighted_sum += (y)*pixel_value;
      total_pixel_sum += pixel_value;
    }
  }

  Current_pos_x = box_left + x_weighted_sum/total_pixel_sum;
  Current_pos_y = box_bottom + y_weighted_sum/total_pixel_sum;
  fprintf(stderr, "Brightest tracking pixel = %f @ (%d,%d)\n",
	  brightest_tracking_pixel, bright_x, bright_y);
  fprintf(stderr, "total_pixel_sum = %f\n", total_pixel_sum);
  fprintf(stderr, "Updated position: X=%f, Y=%f\n",
	  Current_pos_x, Current_pos_y);

  // re-adjust the box?
  if(Current_pos_x < (box_left + TRACKER_BOX_RADIUS_PIXELS/2) ||
     Current_pos_x > (box_right - TRACKER_BOX_RADIUS_PIXELS/2) ||
     Current_pos_y < (box_bottom + TRACKER_BOX_RADIUS_PIXELS/2) ||
     Current_pos_y > (box_top - TRACKER_BOX_RADIUS_PIXELS/2)) {
    // Yes: new box
    fprintf(stderr, "Shifting tracker box.\n");
    box_left   = (int) Current_pos_x - TRACKER_BOX_RADIUS_PIXELS;
    box_right  = (int) Current_pos_x + TRACKER_BOX_RADIUS_PIXELS;
    box_top    = (int) Current_pos_y + TRACKER_BOX_RADIUS_PIXELS;
    box_bottom = (int) Current_pos_y - TRACKER_BOX_RADIUS_PIXELS;
    Update(image, depth+1);		// repeat
  }
  
  // now need to decide whether there is really a star here.
  /*NOT_YET_WRITTEN*/
  delete subImage;
}

int
Tracker::Position(double *x_pos,
		  double *y_pos) {
  if(TrackerStatus() != NO_LOCK) {
    (*x_pos) = Current_pos_x;
    (*y_pos) = Current_pos_y;
    return 0;
  } else {
    return -1;
  }
  /*NOTREACHED*/
}
    
