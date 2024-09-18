#include "estimate_params.h"
#include <Image.h>
#include <math.h>

void estimate_params(Image *i, FocusParams &p) {
  // accept the center_x, center_y that are provided in p.
  p.background = i->HistogramValue(0.5);

  double moment1 = 0.0;
  double moment2 = 0.0;
  p.total_flux = 0.0;

  double background = i->HistogramValue(0.5);
  double max_x = -1.0;
  double max_y = -1.0;
  double brightest = 0.0;

  for (int row = 0; row < i->height; row++) {
    for (int col = 0; col < i->width; col++) {
      if (i->pixel(col, row) > brightest) {
	brightest = i->pixel(col, row);
	max_x = col;
	max_y = row;
      }
    }
  }

  double limit = p.max_width_to_consider;
  for (int loop = 0; loop < 10; loop++) {
    double offset_x = 0.0;
    double offset_y = 0.0;
    double pix_near_sum = 0.0;
    double pix_total_sum = 0.0;

    for (int row = 0; row < i->height; row++) {
      for (int col = 0; col < i->width; col++) {
	const double del_x = (col + 0.5) - max_x;
	const double del_y = (row + 0.5) - max_y;
	const double del_r = sqrt(del_x*del_x + del_y*del_y);
	const double pix = i->pixel(col, row) - background;

	pix_total_sum += pix;
	
	if (del_r < limit) {
	  offset_x += pix*del_x;
	  offset_y += pix*del_y;
	  pix_near_sum += pix;
	}
      }
    }
    max_x = max_x + offset_x/pix_near_sum;
    max_y = max_y + offset_y/pix_near_sum;
    limit *= (pix_total_sum*0.9)/pix_near_sum;
    if (limit > i->height/2) limit = i->height/2;
    // under low SNR, this can actually drive the limit to a negative
    // number, which makes no sense at all
    if (limit < 4.0) limit = 4.0;
    
    fprintf(stderr, "trial x,y @ (%lf,%lf): offset_x = %lf, offset_y = %lf, r = %lf\n",
	    max_x, max_y, offset_x, offset_y, limit);
  }

  p.center_x = max_x;
  p.center_y = max_y;

  // if center cannot be found, return with the "success" flag cleared.
  if ((!isnormal(max_x)) || (!isnormal(max_y)) ||
      max_x < 0.0 || max_y < 0.0 ||
      max_x > i->width || max_y > i->height) {
    p.success = false;
    return;
  }

  for (int row = p.center_y-10; row <= p.center_y+10; row++) {
    for (int col = p.center_x-10; col <= p.center_x+10; col++) {
      const double del_x = (col + 0.5) - p.center_x;
      const double del_y = (row + 0.5) - p.center_y;
      const double r_sq = (del_x*del_x + del_y*del_y);
      const double r = sqrt(r_sq);
      const double this_flux = i->pixel(col, row) - p.background;

      p.total_flux += this_flux;
      moment1 += this_flux*r;
      moment2 += this_flux*r_sq;
    }
  }
  fprintf(stderr, "-----\n");

  p.moment_width = moment1;
  p.moment_2_width = moment2;
  p.success = true;
}
