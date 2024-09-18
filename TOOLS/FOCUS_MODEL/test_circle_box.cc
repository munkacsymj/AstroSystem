#include "circle_box.h"
#include <stdio.h>
#include <math.h>

void do_test(const char *message,
	     double circle_x, double circle_y, double circle_radius,
	     double box_top, double box_bottom,
	     double box_left, double box_right) {
  fprintf(stderr, "--------------------------------\n%s\n", message);
  fprintf(stderr, "  circle center -> (%lf, %lf), radius = %lf\n",
	  circle_x, circle_y, circle_radius);
  fprintf(stderr, "  box left = %lf, right = %lf\n",
	  box_left, box_right);
  fprintf(stderr, "  box top = %lf, bottom = %lf\n",
	  box_top, box_bottom);
  
  double a = area_in_circle(circle_x, circle_y, circle_radius,
			    box_top, box_bottom, box_left, box_right);
  fprintf(stderr, "  circle area = %lf, box area = %lf\n",
	  M_PI * circle_radius * circle_radius,
	  (box_right - box_left) * (box_top - box_bottom));
  fprintf(stderr, "  overlap area = %lf\n", a);
}

int main(int argc, char **argv) {
  double c_x = 2.0;
  double c_y = 2.0;
  double c_r = 2.0;

  do_test("tiny box inside circle", c_x, c_y, c_r, 2.01, 1.99, 1.99, 2.01);
  do_test("huge box contains circle", c_x, c_y, c_r, 1000.0, -1000.0, -1000.0, 1000.0);
  do_test("box with 1/4 circle", c_x, c_y, c_r, 2.0, -50.0, -50.0, 2.0);
  do_test("box splits circle", c_x, c_y, c_r, 4.1, 0.0, -1.0, 2.0);
  do_test("again, bigger box", c_x, c_y, c_r, 4.1, -0.1, -1.0, 2.0);
  do_test("1/2 box in circle", c_x, c_y, c_r, 2.001, 1.999, -2.0, 2.0);
  do_test("all but sliver of circle", c_x, c_y, c_r, 3.999, 0.0, -1.0, 5.0);
  do_test("close to 1/4 circle", c_x, c_y, c_r, 1.99, 0.001, 0.001, 1.99); 
}
