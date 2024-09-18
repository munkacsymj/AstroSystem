/*  focus.cc -- Program to perform auto-focus
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
#include <stdio.h>
#include <Image.h>
#include <scope_api.h>
#include <camera_api.h>
#include <unistd.h>		// sleep()
#include "parab2.h"
#include "nlls_simple.h"
#include "focus.h"

static int box_bottom;
static int box_top;
static int box_left;
static int box_right;

static RunData run_data;

void adjust_box(Image *image) {
  static int first_call = 1;

  if(image->GetIStarList()->NumStars == 0) return;

  int largest_star_index = image->LargestStar();
  double center_x =
    image->GetIStarList()->StarCenterX(largest_star_index);
  double center_y =
    image->GetIStarList()->StarCenterY(largest_star_index);

  fprintf(stderr, "star center at (%f, %f)\n", center_x, center_y);

  if(first_call) {
    first_call = 0;
    center_y = 512.0 - center_y;
  } else {
    center_x += box_left;
    center_y = box_top - center_y;
  }


#define BOXSIZE_H 60		// divisible by 3!
#define BOXSIZE_V 45
  box_bottom = ((int) center_y) -BOXSIZE_V/2;
  box_top = box_bottom + BOXSIZE_V;
  box_left = ((int) center_x) - BOXSIZE_H/2;
  box_left = 3*(box_left/3);
  box_right = box_left + BOXSIZE_H - 1;

  /* fprintf(stderr,
	  "imaging box at bottom = %d, top = %d, left = %d, right = %d\n",
	  box_bottom, box_top, box_left, box_right); */
}

int GetPoints(double focus_position, int count, double exposure_time) {
  double non_converge_count = 0;
  int quit;
  int good_count = 0;
  double *hold_area = (double *) malloc(sizeof(double)*count);

  while(good_count < count &&
	non_converge_count < 5) {

    exposure_flags flags;
    char *this_image_filename = expose_image(exposure_time,
					     flags,
					     box_bottom,
					     box_top,
					     box_left,
					     box_right);
    Image image(this_image_filename);

    adjust_box(&image);
    focus_state fs;
    if(image.GetIStarList()->NumStars == 0 || nlls1(&image, &fs) < 0 ||
       fs.blur() > 3.0) {
      non_converge_count++;
      fprintf(stderr, "position %d, no convergence.\n", (int) focus_position);
      if(good_count == 0 && non_converge_count >= 2) break;
    } else {
      hold_area[good_count++] = fs.blur();
      fprintf(stderr, "position %d, blur = %.2f\n",
	      (int) focus_position,
	      fs.blur());
    }
  }
  if(good_count >= count) {
    double best = 1000000.0;
    int best_j = -1;
    int worst_j = -1;
    double worst = 0.0;

    if(good_count > 3) {
      int j;
      for(j = 0; j < good_count; j++) {
	if(hold_area[j] > worst) {
	  worst = hold_area[j];
	  worst_j = j;
	}
	if(hold_area[j] < best) {
	  best  = hold_area[j];
	  best_j = j;
	}
      }
      if(worst_j >= 0) hold_area[worst_j] = 0.0;
      if(best_j >= 0) hold_area[best_j] = 0.0;
    }
	
    while(good_count--) {
      if(hold_area[good_count] != 0.0)
	run_data.add(focus_position, hold_area[good_count]);
    }
    quit = 1;			// success
  } else {
    quit = -1;
  }
  return quit;
}

int
focus_search_from_focus(int &direction,
			double exposure_time_val) {
  int non_converge_count = 0;
  int quit;
  scope_focus(direction * 100);
  sleep(3);		// let vibrations settle

  do {
    focus_state fs;

    scope_focus(direction * 100);
    sleep(3);		// let vibrations settle
    exposure_flags flags;
    char *this_image_filename = expose_image(exposure_time_val,
					     flags,
					     box_bottom,
					     box_top,
					     box_left,
					     box_right);
    Image image(this_image_filename);
    adjust_box(&image);
    quit = 0;
    if(nlls1(&image, &fs) < 0) {
      non_converge_count++;
      if(non_converge_count >= 3) quit = 1;
    } else {
      non_converge_count = 0;
      if(fs.blur() > 5.0) quit = 1;
    }
  } while(!quit);

  return (non_converge_count ? -1 : 0);
}
      
int
focus_search_from_blur(int &direction,
		       double exposure_time_val) {
  int non_converge_count = 0;
  int quit;
  scope_focus(direction * 100);
  sleep(3);		// let vibrations settle

  do {
    focus_state fs;

    scope_focus(direction * 100);
    sleep(3);		// let vibrations settle
    exposure_flags flags;
    char *this_image_filename = expose_image(exposure_time_val,
					     flags,
					     box_bottom,
					     box_top,
					     box_left,
					     box_right);
    Image image(this_image_filename);
    adjust_box(&image);
    quit = 0;
    if(nlls1(&image, &fs) < 0) {
      non_converge_count++;
      if(non_converge_count >= 3) quit = -1;
    } else {
      non_converge_count = 0;
      if(fs.blur() < 3.0) quit = 1;
    }
  } while(!quit);

  return quit;
}
      
int
focus(double exposure_time_val,
      int focus_time,
      int cycle_count,
      Image *dark_image) {
  int direction = 1;		// initial direction
  // Get an image and do an initial star assessment
  exposure_flags flags;
  char *this_image_filename = expose_image(exposure_time_val, flags);
  Image this_image(this_image_filename);
  if(dark_image) this_image.subtract(dark_image);
  adjust_box(&this_image);

  focus_state fs;
  if(nlls1(&this_image, &fs) < 0) {
    fprintf(stderr, "Focus: unable to converge.\n");
    return -1;
  }

  if(fs.blur() < 3.0) {
    fprintf(stderr, "working from initial near-focus\n");
    focus_search_from_focus(direction, exposure_time_val);
  } else {
    fprintf(stderr, "working from initial blur\n");
    if(focus_search_from_blur(direction, exposure_time_val) == 1) {
      focus_search_from_focus(direction, exposure_time_val);
    }
  }

  // reverse direction
  direction = -direction;

  // loop, passing though focus once
  int non_convergence_count = 0;
  int bad_parabola_count    = 0;
  int good_points           = 0;
  
  int starting_position = scope_focus(0);
  fprintf(stdout, "Starting calibration run.\n");

  Parabola p;
  int quit;

  do {
    scope_focus(direction * focus_time);
    sleep(3);

    if(GetPoints(CumFocusPosition(), cycle_count, exposure_time_val) < 0) {
      // did not converge
      non_convergence_count++;
      fprintf(stderr, "No useful points at %d focus\n",
	      (int) CumFocusPosition());
    } else {
      non_convergence_count = 0;
      good_points++;
      fprintf(stderr, "Finished at %d focus\n", 
	      (int) CumFocusPosition());
    }

    quit = 0;
    if(good_points >= 8) {

      fprintf(stderr, "Trying to fit points to parabola.\n");
      p.reset();
      if(nlls_parabola(&p, &run_data) >= 0) {
	// worked. At least 5 points past minimum?
	// 9/26/2001: warning: direction's sign is backwards?
	double excess = CumFocusPosition() - (5*focus_time*(-direction) +
					      p.x0());
	fprintf(stderr, "Matched. Predicted at %d, excess = %d, dir=%d\n",
		(int) p.x0(), (int) excess, direction);
	if(excess * (-direction) > 0.0) {
	  fprintf(stdout,
		  "At least 5 points past focus. Quitting calibration run.\n");
	  fprintf(stdout, "B(x0) = %f, C = %f, D = %f, Blur = %f\n",
		  /* p.state_var[PARAB_A], */
		  p.state_var[PARAB_B],
		  p.state_var[PARAB_C],
		  p.state_var[PARAB_D],
		  p.state_var[PARAB_D]+sqrt(p.state_var[PARAB_C]));
	  quit = 1;
	}
      } else {
	bad_parabola_count++;
	fprintf(stdout, "Bad parabola. Didn't converge.\n");
      }
    }

  } while(non_convergence_count < 25 && quit == 0 &&
	  bad_parabola_count < 25);

  if(!quit) {
    fprintf(stderr, "focus run terminated abnormally.\n");
    fprintf(stderr, "non_convergence_count = %d, bad_parabola_count = %d\n",
	    non_convergence_count, bad_parabola_count);
    return -1;
  }

  // change direction and reset back to the beginning
  direction = -direction;

  while(p.NoSolution()) {
    fprintf(stdout, "Trying to fix parabola by dropping point.\n");
    run_data.N--;			// drop last points
    if(run_data.N < 4) {
      fprintf(stderr, "Cannot find good parabola.\n");
      return -1;
    }
    p.reset();
    nlls_parabola(&p, &run_data);
  }

  double excess = fabs(CumFocusPosition() - p.x0());
  if(excess > 2000.0) {
    fprintf(stderr, "Seem to be %d beyond focus. Unreasonable. Quitting.\n",
	    (int) excess);
    return -1;
  }
  
  int current_position = scope_focus(0);
  fprintf(stderr, "Running focus backwards to get to restart position.\n");
  scope_focus(starting_position - current_position);

  // change direction and close in on focus point.
  direction = -direction;
  Parabola p_new;

  fprintf(stdout, "Starting final focus run.\n");
  run_data.reset();

  do {
    scope_focus(direction * focus_time);
    sleep(3);

    if(GetPoints(CumFocusPosition(), cycle_count, exposure_time_val) < 0) {
      // did not converge
      non_convergence_count++;
      fprintf(stderr, "No useful points at %d focus\n",
	      (int) CumFocusPosition());
    } else {
      non_convergence_count = 0;
      good_points++;
      fprintf(stderr, "Finished at %d focus\n", 
	      (int) CumFocusPosition());
    }

    quit = 0;
    if(good_points >= 3) {

      fprintf(stderr, "Trying to fit points to constrained parabola.\n");
      p_new.reset(&p);
      if(nlls_parabola_x0(&p_new, &run_data) >= 0) {
	double to_go = (-direction) * (p_new.x0() - CumFocusPosition());
	fprintf(stdout, "At %d with focus predicted at %d\n",
		(int) CumFocusPosition(), (int) p_new.x0());
	if(to_go < focus_time) {
	  fprintf(stdout, "Final fine-tune for focus of %f msec\n", to_go);
	  scope_focus(direction * (int) to_go);
	  quit = 1;
	} else {
	  scope_focus(direction * (int) to_go);
	  GetPoints(CumFocusPosition(), cycle_count, exposure_time_val);
	}
      } else {
	bad_parabola_count++;
	fprintf(stdout, "Bad parabola. Didn't converge.\n");
      }
    }
  } while(non_convergence_count < 20 && quit == 0 &&
	  bad_parabola_count < 25);
  return 0;
}  
    
  
