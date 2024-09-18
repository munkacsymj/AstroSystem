/*  background.cc -- calculate statistics/model of image background,
 *  including gradients
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
#include <stdio.h>
#include <unistd.h> 		// for getopt()
#include <stdlib.h>		// for atof()
#include <math.h>		// for fabs()
#include "Image.h"
#include <gsl/gsl_vector.h>
#include <gsl/gsl_matrix.h>
#include <gsl/gsl_permutation.h>
#include <gsl/gsl_linalg.h>
#include "nlls_general.h"
#include "background.h"

//
// Background model:
// Measure (x,y) as pixel offsets from image center
// I = K + A*r + B*x + C*y
//
double
Background::Value(int x, int y) {
  const int x_off  = x-x0;
  const int y_off  = y-y0;
  const double r = sqrt(x_off*x_off + y_off*y_off);

  return K + A*r + B*x_off + C*y_off;
}

Background::Background(Image *i) {
  // find the max and min pixel values for the image's background. Any
  // pixel values outside these limits will not be used in the
  // calculation of the background.
  double bgd_max, bgd_min;

  bgd_max = i->HistogramValue(0.75);
  bgd_min = i->HistogramValue(0.10);
  fprintf(stderr, "Analyzing background points between %.1f and %.1f\n",
	  bgd_min, bgd_max);

  x0 = i->width/2;
  y0 = i->height/2;
  double sum_z = 0.0;
  double sum_zr = 0.0;
  double sum_zx = 0.0;
  double sum_zy = 0.0;
  double sum_r = 0.0;
  double sum_x = 0.0;
  double sum_y = 0.0;
  double sum_r2 = 0.0;
  double sum_xr = 0.0;
  double sum_yr = 0.0;
  double sum_x2 = 0.0;
  double sum_y2 = 0.0;
  double sum_xy = 0.0;
  int N = 0;

  for(int x=0 ; x<i->width; x++) {
    for(int y=0; y<i->height; y++) {
      const double z = i->pixel(x, y);
      if(z < bgd_min || z > bgd_max) continue;

      N++;
      const int x_val = x-x0;
      const int y_val = y-y0;
      double r = sqrt(x_val*x_val + y_val*y_val);

      sum_z += z;
      sum_zr += (z*r);
      sum_zx += (z*x_val);
      sum_zy += (z*y_val);

      sum_r += r;
      sum_x += x_val;
      sum_y += y_val;
      sum_r2 += (r*r);
      sum_xr += (r*x_val);
      sum_yr += (r*y_val);
      sum_x2 += (x_val*x_val);
      sum_y2 += (y_val*y_val);
      sum_xy += (x_val*y_val);
    }
  }

  fprintf(stderr, "with %d points ...\n", N);

  gsl_matrix *matrix = gsl_matrix_calloc(4, 4);
  if(matrix == 0) return;

  gsl_vector *product = gsl_vector_calloc(4);
  if(!product) {
    fprintf(stderr, "nlls: allocation of product vector failed.\n");
  }

  (*gsl_vector_ptr(product, 0)) = sum_z;
  (*gsl_vector_ptr(product, 1)) = sum_zr;
  (*gsl_vector_ptr(product, 2)) = sum_zx;
  (*gsl_vector_ptr(product, 3)) = sum_zy;

  (*gsl_matrix_ptr(matrix, 0, 0)) = (double) N;
  (*gsl_matrix_ptr(matrix, 0, 1)) = sum_r;
  (*gsl_matrix_ptr(matrix, 0, 2)) = sum_x;
  (*gsl_matrix_ptr(matrix, 0, 3)) = sum_y;
  (*gsl_matrix_ptr(matrix, 1, 0)) = sum_r;
  (*gsl_matrix_ptr(matrix, 1, 1)) = sum_r2;
  (*gsl_matrix_ptr(matrix, 1, 2)) = sum_xr;
  (*gsl_matrix_ptr(matrix, 1, 3)) = sum_yr;
  (*gsl_matrix_ptr(matrix, 2, 0)) = sum_x;
  (*gsl_matrix_ptr(matrix, 2, 1)) = sum_xr;
  (*gsl_matrix_ptr(matrix, 2, 2)) = sum_x2;
  (*gsl_matrix_ptr(matrix, 2, 3)) = sum_xy;
  (*gsl_matrix_ptr(matrix, 3, 0)) = sum_y;
  (*gsl_matrix_ptr(matrix, 3, 1)) = sum_yr;
  (*gsl_matrix_ptr(matrix, 3, 2)) = sum_xy;
  (*gsl_matrix_ptr(matrix, 3, 3)) = sum_y2;
  
  gsl_permutation *permutation = gsl_permutation_alloc(4);
  if(!permutation) {
    fprintf(stderr, "nlls: permutation create failed.\n");
  }

  int sig_num;
  if(gsl_linalg_LU_decomp(matrix, permutation, &sig_num)) {
    fprintf(stderr, "nlls: gsl_linalg_LU_decomp() failed.\n");
    return;
  }

  if(gsl_linalg_LU_svx(matrix, permutation, product)) {
    fprintf(stderr, "nlls: gls_linalg_LU_solve() failed.\n");
    return;
  }

  gsl_matrix_free(matrix);
  gsl_permutation_free(permutation);

  // get results
  K = gsl_vector_get(product, 0);
  A = gsl_vector_get(product, 1);
  B = gsl_vector_get(product, 2);
  C = gsl_vector_get(product, 3);

  fprintf(stderr, "K = %f, A= %f, B= %f, C= %f\n", K, A, B, C);

  // Now calculate the standard deviation of the background
  N = 0;
  double sum_err_sq = 0.0;
  for(int x=0 ; x<i->width; x++) {
    for(int y=0; y<i->height; y++) {
      const double z = i->pixel(x, y);
      if(z < bgd_min || z > bgd_max) continue;

      N++;
      const double ref = Value(x, y);
      const double err = z - ref;
      sum_err_sq += (err*err);
    }
  } // end loop over all pixels
  stddev = sqrt(sum_err_sq/N);

  gsl_vector_free(product);
}

double distance_from(double star_x, double star_y,
		     double corner_x, double corner_y) {
  double del_x = corner_x - star_x;
  double del_y = corner_y - star_y;
  return sqrt(del_x*del_x + del_y*del_y);
}

double r_fact(double rad1, double rad2, double aperture_size) {
  if(rad1 < rad2) return r_fact(rad2, rad1, aperture_size);

  // guarantee that rad1 > rad2
  if(rad2 > aperture_size) return 0.0;
  if(rad1 < aperture_size) return 1.0;

  // somewhere in between
  return (aperture_size - rad2)/(rad1 - rad2);
}

