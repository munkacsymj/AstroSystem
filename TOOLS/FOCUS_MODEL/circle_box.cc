#include <assert.h>
#include <stdio.h>
#include <math.h>
#include "circle_box.h"

//****************************************************************
//        area_in_circle()
//   This function returns a number that is the area of the specified
//   box that falls within circle_radius of the point (circle_x,
//   circle_y).
//****************************************************************

#define letter_debug false

bool point_in_circle(double circle_x, double circle_y, double circle_radius,
		     double x, double y) {
  const double delta_x = (x - circle_x);
  const double delta_y = (y - circle_y);
  return (delta_x * delta_x + delta_y * delta_y) <= circle_radius*circle_radius;
}

bool point_in_box(double left, double right, double top, double bottom,
		  double x, double y) {
  const bool x_ok = (x >= left && x <= right);
  const bool y_ok = (y >= bottom && y <= top);
  return x_ok && y_ok;
}

bool point_in_square(double box_top, double box_bottom, double box_left, double box_right,
		     double x, double y) {
  return x >= box_left &&
    x <= box_right &&
    y >= box_bottom &&
    y <= box_top;
}

void find_point_vert(double circle_x, double circle_y, double circle_radius,
		     double x_in, double &y_out1, double &y_out2) {
  const double delta_x = (x_in - circle_x);
  const double y_sq = sqrt(circle_radius*circle_radius - delta_x*delta_x);
  y_out1 = y_sq + circle_y;
  y_out2 = circle_y - y_sq;
}

void find_point_horiz(double circle_x, double circle_y, double circle_radius,
		      double y_in, double &x_out1, double &x_out2) {
  const double delta_y = (y_in - circle_y);
  const double x_sq = sqrt(circle_radius*circle_radius - delta_y*delta_y);
  x_out1 = x_sq + circle_x;
  x_out2 = circle_x - x_sq;
}

double chord_area(double chord, double radius) {
  const double theta = 2.0 * asin(chord/(2*radius));
  const double area = radius*radius*0.5*(theta - sin(theta));
  return area;
}

bool circle_intersects_vertical(double circle_x, double circle_y, double circle_radius,
				double x, double y_low, double y_high) {
  double intersect1, intersect2;
  find_point_vert(circle_x, circle_y, circle_radius, x, intersect1, intersect2);
  if (!isnan(intersect1)) {
    if (intersect1 >= y_low && intersect1 <= y_high) return true;
  }

  if (!isnan(intersect2)) {
    if (intersect2 >= y_low && intersect2 <= y_high) return true;
  }

  return false;
}

bool circle_intersects_horizontal(double circle_x, double circle_y, double circle_radius,
				  double y, double x_low, double x_high) {
  double intersect1, intersect2;
  find_point_horiz(circle_x, circle_y, circle_radius, y, intersect1, intersect2);
  if (!isnan(intersect1)) {
    if (intersect1 >= x_low && intersect1 <= x_high) return true;
  }

  if (!isnan(intersect2)) {
    if (intersect2 >= x_low && intersect2 <= x_high) return true;
  }

  return false;
}

// Case 1 is the case where a single corner of the box falls within
// the circle
double do_case_1(double box_area,
		 double circle_radius,
		 double x_coord,
		 double y_coord,
		 double x_intercept,
		 double y_intercept,
		 bool invert) {

  const double del_x = x_coord - x_intercept;
  const double del_y = y_coord - y_intercept;
  const double height = fabs(del_x);
  const double width  = fabs(del_y);
  const double triangle = (height * width)/2.0;
  const double chord = sqrt(del_x*del_x + del_y*del_y);

  const double area = chord_area(chord, circle_radius);
  // The "chord area" is subtracted if "invert" is true
  if (invert) {
    return box_area - (triangle - area);
  } else {
    return triangle + area;
  }
  /*NOTREACHED*/
}

double do_clipped_corners(double box_top, double box_bottom,
			  double box_left, double box_right,
			  double circle_radius,
			  double circle_x, double circle_y,
			  bool top_left_inside, bool top_right_inside,
			  bool bottom_left_inside, bool bottom_right_inside) {
  double upper_left = 0.0;	// these four are the amount of loss
				// due to clipping of each of the four
				// corners by the circle
  double upper_right = 0.0;
  double lower_left = 0.0;
  double lower_right = 0.0;

  if (!top_left_inside) {
    double intersect1, intersect2, dummy;
    find_point_horiz(circle_x, circle_y, circle_radius,
		     box_top, dummy, intersect1);
    find_point_vert(circle_x, circle_y, circle_radius,
		    box_left, intersect2, dummy);
    // by setting the first argument (box_area) to 0.0, we set up
    // do_case_1 to return a negative value, which is the area of
    // the chunk cut off at the top_left corner.
    upper_left = -do_case_1(0.0, circle_radius, box_left, box_top, intersect1, intersect2, true);
    if (letter_debug) fprintf(stderr, "z");
  }
  if (!top_right_inside) {
    double intersect1, intersect2, dummy;
    find_point_horiz(circle_x, circle_y, circle_radius,
		     box_top, intersect1, dummy);
    find_point_vert(circle_x, circle_y, circle_radius,
		    box_right, intersect2, dummy);
    upper_right = -do_case_1(0.0, circle_radius, box_right, box_top, intersect1, intersect2, true);
    if (letter_debug) fprintf(stderr, "y");
  }
  if (!bottom_left_inside) {
    double intersect1, intersect2, dummy;
    find_point_horiz(circle_x, circle_y, circle_radius,
		     box_bottom, dummy, intersect1);
    find_point_vert(circle_x, circle_y, circle_radius,
		    box_left, dummy, intersect2);
    lower_left = -do_case_1(0.0, circle_radius, box_left, box_bottom, intersect1, intersect2, true);
    if (letter_debug) fprintf(stderr, "x");
  }
  if (!bottom_right_inside) {
    double intersect1, intersect2, dummy;
    find_point_horiz(circle_x, circle_y, circle_radius,
		     box_bottom, intersect1, dummy);
    find_point_vert(circle_x, circle_y, circle_radius,
		    box_right, dummy, intersect2);
    lower_right = -do_case_1(0.0, circle_radius, box_right, box_bottom, intersect1, intersect2, true);
    if (letter_debug) fprintf(stderr, "w");
  }

  return (box_right - box_left)*(box_top - box_bottom) -
    (upper_left + upper_right + lower_left + lower_right);
    
}  

double do_case_9(double radius, double box_width,
		 double box_side1, double box_side2) {
  // three parts: one is a triangle, one a rectangle,
  // and the other is a chord area
  const double rect_side = (box_side1 < box_side2 ? box_side1 : box_side2);
  const double rect_area = rect_side * box_width;
  const double triangle_side = fabs(box_side1 - box_side2);
  const double triangle_area = (triangle_side * box_width)/2;
  const double chord = sqrt(box_width * box_width +
			    triangle_side * triangle_side);
  const double third_area = chord_area(chord, radius);
  return rect_area + triangle_area + third_area;
}

double area_in_circle(double circle_x, double circle_y, double circle_radius,
		      double box_top, double box_bottom, double box_left, double box_right) {
  // Check the circle extreme points, see if they're in the box
  bool top_is_in = point_in_box(box_left, box_right, box_top, box_bottom,
				circle_x, circle_y + circle_radius);
  bool bottom_is_in = point_in_box(box_left, box_right, box_top, box_bottom,
				   circle_x, circle_y - circle_radius);
  bool left_is_in = point_in_box(box_left, box_right, box_top, box_bottom,
				 circle_x - circle_radius, circle_y);
  bool right_is_in = point_in_box(box_left, box_right, box_top, box_bottom,
				  circle_x + circle_radius, circle_y);
  bool center_is_in = point_in_box(box_left, box_right, box_top, box_bottom,
				   circle_x, circle_y);
  // count number of box corners inside the circle
  bool top_left_inside = point_in_circle(circle_x, circle_y, circle_radius,
					 box_left, box_top);
  bool top_right_inside = point_in_circle(circle_x, circle_y, circle_radius,
					  box_right, box_top);
  bool bottom_left_inside = point_in_circle(circle_x, circle_y, circle_radius,
					    box_left, box_bottom);
  bool bottom_right_inside = point_in_circle(circle_x, circle_y, circle_radius,
					     box_right, box_bottom);
  int num_inside = 0;
  const double box_area = (box_top - box_bottom)*(box_right - box_left);
  if (top_left_inside) num_inside++;
  if (top_right_inside) num_inside++;
  if (bottom_left_inside) num_inside++;
  if (bottom_right_inside) num_inside++;

  int num_circle_extremes_outside = 0;
  if (!top_is_in) num_circle_extremes_outside++;
  if (!bottom_is_in) num_circle_extremes_outside++;
  if (!left_is_in) num_circle_extremes_outside++;
  if (!right_is_in) num_circle_extremes_outside++;

  // all corners inside the circle: easy
  if (num_inside == 4) {
    return (box_top-box_bottom)*(box_right-box_left);
  }

  bool do_inversion = false;
  if (num_inside == 3) {
    // turn this into a version of case 1
    do_inversion = true;
    top_left_inside = !top_left_inside;
    top_right_inside = !top_right_inside;
    bottom_left_inside = !bottom_left_inside;
    bottom_right_inside = !bottom_right_inside;
  }

  // case 12
  if (num_inside == 1 && num_circle_extremes_outside == 4 &&
      center_is_in) {
    return do_clipped_corners(box_top, box_bottom,
			      box_left, box_right,
			      circle_radius, circle_x, circle_y,
			      top_left_inside, top_right_inside,
			      bottom_left_inside, bottom_right_inside);
  }

  // one corner inside the circle
  if (num_inside == 1 || num_inside == 3) {
    double top_intersect, bottom_intersect, left_intersect, right_intersect;
    // always < 180-deg chord
    if (top_left_inside || top_right_inside) {
      if(letter_debug) fprintf(stderr, "A");
      find_point_horiz(circle_x, circle_y, circle_radius,
		       box_top, right_intersect, left_intersect);
    }
    if (bottom_left_inside || bottom_right_inside) {
      if (letter_debug) fprintf(stderr, "B");
      find_point_horiz(circle_x, circle_y, circle_radius,
		       box_bottom, right_intersect, left_intersect);
    }
    if (top_left_inside || bottom_left_inside) {
      if (letter_debug) fprintf(stderr, "C");
      find_point_vert(circle_x, circle_y, circle_radius,
		      box_left, top_intersect, bottom_intersect);
    }
    if (top_right_inside || bottom_right_inside) {
      if (letter_debug) fprintf(stderr, "D");
      find_point_vert(circle_x, circle_y, circle_radius,
		      box_right, top_intersect, bottom_intersect);
    }

    if (do_inversion) {
      double temp;
      temp = left_intersect;
      left_intersect = right_intersect;
      right_intersect = temp;

      temp = bottom_intersect;
      bottom_intersect = top_intersect;
      top_intersect = temp;
    }

    if (top_left_inside) {
      if (letter_debug) fprintf(stderr, "a");
      return do_case_1(box_area, circle_radius, box_left, box_top,
		       right_intersect, bottom_intersect, do_inversion);
    } else if (top_right_inside) {
      if (letter_debug) fprintf(stderr, "b");
      return do_case_1(box_area, circle_radius, box_right, box_top,
		       left_intersect, bottom_intersect, do_inversion);
    } else if (bottom_left_inside) {
      if (letter_debug) fprintf(stderr, "c");
      return do_case_1(box_area, circle_radius, box_left, box_bottom,
		       right_intersect, top_intersect, do_inversion);
    } else if (bottom_right_inside) {
      if (letter_debug) fprintf(stderr, "d");
      return do_case_1(box_area, circle_radius, box_right, box_bottom,
		       left_intersect, top_intersect, do_inversion);
    } else {
      fprintf(stderr, "three_part_model: impossible #1\n");
      return 0;
    }

    // else two corners inside the circle: opposite sides, still four
    // cases 
  } else if (num_inside == 2) {
    // if top/bottom sides intersect, top_bottom will be TRUE. If
    // left/right sides intersect, top_bottom will be FALSE.
    const bool top_bottom = ((top_left_inside && bottom_left_inside) ||
			     (top_right_inside && bottom_right_inside));
    // if circle is to the right or to the top, high_side will be
    // TRUE. If circle is to the left or to the bottom, high_side will
    // be FALSE.
    const bool high_side = ((top_right_inside && bottom_right_inside) ||
			    (top_left_inside && top_right_inside));
    
    double intersect1, intersect2, intersect3, intersect4;
    double box_full_width;

    if (top_bottom) {
      find_point_horiz(circle_x, circle_y, circle_radius,
		       box_top, intersect1, intersect2);
      find_point_horiz(circle_x, circle_y, circle_radius,
		       box_bottom, intersect3, intersect4);
      box_full_width = box_top - box_bottom;
    } else {
      find_point_vert(circle_x, circle_y, circle_radius,
		      box_left, intersect1, intersect2);
      find_point_vert(circle_x, circle_y, circle_radius,
		      box_right, intersect3, intersect4);
      box_full_width = box_right - box_left;
    }

    if (top_bottom && high_side) {
      if (!circle_intersects_vertical(circle_x, circle_y, circle_radius,
				      box_left, box_bottom, box_top)) {
	if (letter_debug) fprintf(stderr, "E");
	return do_case_9(circle_radius, box_full_width,
			 box_right - intersect2,
			 box_right - intersect4);
      }
    } else if (top_bottom && !high_side) {
      if (!circle_intersects_vertical(circle_x, circle_y, circle_radius,
				      box_right, box_bottom, box_top)) {
	if (letter_debug) fprintf(stderr, "F");
	return do_case_9(circle_radius, box_full_width,
			 intersect1 - box_left,
			 intersect3 - box_left);
      }
    } else if ((!top_bottom) && high_side) {
      if (!circle_intersects_horizontal(circle_x, circle_y, circle_radius,
					box_bottom, box_left, box_right)) {
	if (letter_debug) fprintf(stderr, "G");
	return do_case_9(circle_radius, box_full_width,
			 box_top - intersect2,
			 box_top - intersect4);
      }
    } else if ((!top_bottom) && (!high_side)) {
      if (!circle_intersects_horizontal(circle_x, circle_y, circle_radius,
					box_top, box_left, box_right)) {
	if (letter_debug) fprintf(stderr, "H");
	return do_case_9(circle_radius, box_full_width,
			 intersect1 - box_bottom,
			 intersect3 - box_bottom);
      }
    }
    // getting here means we have the odd case where the entire box is
    // inside the circle except for two adjacent corners (case 11)
    return do_clipped_corners(box_top, box_bottom,
			      box_left, box_right,
			      circle_radius, circle_x, circle_y,
			      top_left_inside, top_right_inside,
			      bottom_left_inside, bottom_right_inside);
  } else if (num_inside == 0) {
    if (circle_y - circle_radius > box_top ||
	circle_y + circle_radius < box_bottom ||
	circle_x - circle_radius > box_right ||
	circle_x + circle_radius < box_left) return 0.0;
    
    // if none of the sides of the circle are in the box and the
    // center is not in the box, then nothing is in the box
    if (!(top_is_in || bottom_is_in || left_is_in || right_is_in ||
	  center_is_in)) {
      return 0.0;
    }

    double circle_area = M_PI * circle_radius * circle_radius;
    // now subtract off any chords that fall outside the box
    double intercept1, intercept2;

    if (center_is_in) {
      if (!top_is_in) {
	find_point_horiz(circle_x, circle_y, circle_radius,
			 box_top, intercept1, intercept2);
	assert(!isnan(intercept1));
	const double chord = fabs(intercept1 - intercept2);
	if (letter_debug) fprintf(stderr, "P");
	circle_area -= chord_area(chord, circle_radius);
      }
      if (!bottom_is_in) {
	find_point_horiz(circle_x, circle_y, circle_radius,
			 box_bottom, intercept1, intercept2);
	assert(!isnan(intercept1));
	const double chord = fabs(intercept1 - intercept2);
	if (letter_debug) fprintf(stderr, "Q");
	circle_area -= chord_area(chord, circle_radius);
      }
      if (!left_is_in) {
	find_point_vert(circle_x, circle_y, circle_radius,
			 box_left, intercept1, intercept2);
	assert(!isnan(intercept1));
	const double chord = fabs(intercept1 - intercept2);
	if (letter_debug) fprintf(stderr, "R");
	circle_area -= chord_area(chord, circle_radius);
      }
      if (!right_is_in) {
	find_point_vert(circle_x, circle_y, circle_radius,
			 box_right, intercept1, intercept2);
	if (isnan(intercept1)) {
	  fprintf(stderr, "circle_box: err:\n");
	  fprintf(stderr, "circle_x = %lf, circle_y = %lf\n",
		  circle_x, circle_y);
	  fprintf(stderr, "circle_radius = %lf, box_right = %lf\n",
		  circle_radius, box_right);
	}
	assert(!isnan(intercept1));
	const double chord = fabs(intercept1 - intercept2);
	if (letter_debug) fprintf(stderr, "S");
	circle_area -= chord_area(chord, circle_radius);
      }
      return circle_area;
    } else {
      // center is not inside

      if(top_is_in) {
	find_point_horiz(circle_x, circle_y, circle_radius,
			 box_bottom, intercept1, intercept2);
	if (!(isnan(intercept1) || isnan(intercept2))) {
	  if (intercept1 >= box_left && intercept1 <= box_right &&
	      intercept2 >= box_left && intercept2 <= box_right) {
	    const double chord = fabs(intercept1 - intercept2);
	    if (letter_debug) fprintf(stderr, "I");
	    return chord_area(chord, circle_radius);
	  }
	}
      }
    
      if (bottom_is_in) {
	find_point_horiz(circle_x, circle_y, circle_radius,
			 box_top, intercept1, intercept2);
	if (!(isnan(intercept1) || isnan(intercept2))) {
	  if (intercept1 >= box_left && intercept1 <= box_right &&
	      intercept2 >= box_left && intercept2 <= box_right) {
	    const double chord = fabs(intercept1 - intercept2);
	    if (letter_debug) fprintf(stderr, "J");
	    return chord_area(chord, circle_radius);
	  }
	}
      }
    
      if (left_is_in) {
	find_point_vert(circle_x, circle_y, circle_radius,
			box_right, intercept1, intercept2);
	if (!(isnan(intercept1) || isnan(intercept2))) {
	  if (intercept1 >= box_bottom && intercept1 <= box_top &&
	      intercept2 >= box_bottom && intercept2 <= box_top) {
	    const double chord = fabs(intercept1 - intercept2);
	    if (letter_debug) fprintf(stderr, "K");
	    return chord_area(chord, circle_radius);
	  }
	}
      }
    
      if (right_is_in) {
	find_point_vert(circle_x, circle_y, circle_radius,
			box_left, intercept1, intercept2);
	if (!(isnan(intercept1) || isnan(intercept2))) {
	  if (intercept1 >= box_bottom && intercept1 <= box_top &&
	      intercept2 >= box_bottom && intercept2 <= box_top) {
	    const double chord = fabs(intercept1 - intercept2);
	    if (letter_debug) fprintf(stderr, "L");
	    return chord_area(chord, circle_radius);
	  }
	}
      }
      fprintf(stderr, "three_part_model: impossible #7\n");
    }
  } else {
    fprintf(stderr, "three_part_model: impossible #5\n");
    return 0;
  }
  /*NOTREACHED*/
  fprintf(stderr, "three_part_model: impossible #6\n");
  return 0;
}
      
    


    
      
