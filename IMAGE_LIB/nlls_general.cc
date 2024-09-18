/*  nlls_general.cc -- Non-linear least-squares modeling of star point-
 *  spread-function (PSF)
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
#include <math.h>		// for fabs()
#include "Image.h"
#include <gsl/gsl_vector.h>
#include <gsl/gsl_matrix.h>
#include <gsl/gsl_permutation.h>
#include <gsl/gsl_linalg.h>
#include "nlls_general.h"

const int FS_X0    = 0;		// x0 (star center, X)
const int FS_Y0    = 1;		// y0 (star center, Y)
const int FS_C     = 2;		// C = total flux
const int FS_B     = 3;		// B = background
const int FS_R     = 4;		// R = blur (FWHM)
const int FS_Beta  = 5;		// Beta = gaussian tail

class focus_state {
public:
  double state_var[8];
  double mel;

  focus_state(void);		// provides default init state

  double &blur(void)             { return state_var[FS_R]; }
  double &R(void)                { return state_var[FS_R]; }
  double &Beta(void)             { return state_var[FS_Beta]; }
};

struct obs_data {
  int N;
  double *xi;			// coordinates of point
  double *yi;
  double *y;			// flux measured
  double **t;			// 8 pointers
  double *err;
};

void
Computet1t2t3(struct obs_data *od, focus_state *fs) {
  const double &C     = fs->state_var[FS_C];
  const double &B     = fs->state_var[FS_B];
  const double &Beta  = fs->Beta();
  const double &R     = fs->R();
  const double &x0    = fs->state_var[FS_X0];
  const double &y0    = fs->state_var[FS_Y0];

  for(int k=0; k<od->N; k++) {

    const double del_x1 = od->xi[k] - x0;
    const double del_y1 = od->yi[k] - y0;
    const double r1_sq  = del_x1*del_x1 + del_y1*del_y1;

    const double fact1 = 1.0 + r1_sq/(R*R);
    const double compl1 = pow(fact1, Beta);
    const double aug1 = compl1 * fact1;	// these are the "beta + 1" terms
    
    od->err[k] = od->y[k] - (B + (C/compl1));

    // partial derivative wrt x0
    od->t[FS_X0][k] = (2.0 * Beta * C / (R*R)) * (del_x1/aug1);

    // partial derivative wrt y0
    od->t[FS_Y0][k] = (2.0 * Beta * C / (R*R)) * (del_y1/aug1);

    // partial derivative wrt C
    od->t[FS_C][k] = 1.0/compl1;

    od->t[FS_B][k] = 1.0;		// easy one

    // partial derivative wrt R
    od->t[FS_R][k] = (2.0 * Beta * C / (R*R*R)) * (r1_sq/aug1);

    // partial derivative wrt Beta
    od->t[FS_Beta][k] = -C * log(fact1)/compl1;
  }
}

focus_state::focus_state(void) {
  state_var[FS_X0]    = 0.0;
  state_var[FS_Y0]    = 0.0;
  state_var[FS_R]     = 0.5;
  state_var[FS_Beta]  = 1.2;
  state_var[FS_C]     = 6000.0;
  state_var[FS_B]     = 100.0;
}

int
nlls(Image *primary_image, int star_id, IStarList *sl) {
  focus_state fs;
  int quit;
  const int BoxWidth = 10;

  obs_data *od = new obs_data;

  od->N        = BoxWidth * BoxWidth;
  od->xi       = new double[od->N];
  od->yi       = new double[od->N];
  od->y        = new double[od->N];
  od->t        = new double * [6];
  od->err      = new double[od->N];
  for(int j=0; j<6; j++) {
    od->t[j] = new double[od->N];
  }

  const double center_x = sl->StarCenterX(star_id);
  const double center_y = sl->StarCenterY(star_id);

  const int left_edge   = (int) (center_x - BoxWidth/2 + 0.5);
  const int right_edge  = left_edge + BoxWidth;
  const int top_edge    = (int) (center_y - BoxWidth/2 + 0.5);
  const int bottom_edge = top_edge + BoxWidth;

  {
    int pixel_no = 0;

    for(int x = left_edge; x< right_edge; x++) {
      for(int y = top_edge; y < bottom_edge; y++) {
	od->xi[pixel_no] = x - center_x;
	od->yi[pixel_no] = (y - center_y)*(19.7/17.0);
	od->y[pixel_no] = primary_image->pixel(x, y);
	pixel_no++;
      }
    }
  }

  fs.state_var[FS_B] = primary_image->pixel(left_edge, top_edge);
  fs.state_var[FS_C] = 2.0 * (primary_image->statistics()->BrightestPixel -
			       primary_image->statistics()->DarkestPixel);

  int loop_count = 0;
  const int order = 6;
  do {
    // compute t1, t2, t3, and t4 for all points, putting them into "od"
    /* fprintf(stderr, "C = %f, B = %f, R = %f, Beta = %f\n",
       fs.state_var[FS_C], fs.state_var[FS_B],
       fs.state_var[FS_R], fs.state_var[FS_Beta]);
    
    fprintf(stderr, "x0 = %f, y0 = %f\n",
	    fs.state_var[FS_X0], fs.state_var[FS_Y0]);
	    */

    Computet1t2t3(od, &fs);

    gsl_matrix *matrix = gsl_matrix_calloc(order, order);
    if(matrix == 0) return -1;

    gsl_vector *product = gsl_vector_calloc(order);
    if(!product) {
      fprintf(stderr, "nlls: allocation of product vector failed.\n");
    }

    gsl_permutation *permutation = gsl_permutation_alloc(order);
    if(!permutation) {
      fprintf(stderr, "nlls: permutation create failed.\n");
    }

    double err_sq = 0.0;

    for(int n = 0; n < od->N; n++) {
      for(int b = 0; b < order; b++) {
	(*gsl_vector_ptr(product, b)) += od->t[b][n] * od->err[n];

	for(int c = b; c < order; c++) {
	    ((*gsl_matrix_ptr(matrix, b, c)) += od->t[b][n] * od->t[c][n]);
	}
      }
      err_sq += od->err[n] * od->err[n];
    }
    for(int b=0; b < order; b++) {
      for(int c = b+1; c < order; c++) {
	(*gsl_matrix_ptr(matrix, c, b)) = (*gsl_matrix_ptr(matrix, b, c));
      }
    }

    int sig_num;

    // fprintf(stdout, "----------------\n");
    // gsl_matrix_fprintf(stdout, matrix, "%f");
    // fprintf(stdout, "----------------\n");
    // gsl_vector_fprintf(stdout, product, "%f");

    if(gsl_linalg_LU_decomp(matrix, permutation, &sig_num)) {
      fprintf(stderr, "nlls: gsl_linalg_LU_decomp() failed.\n");
      return -1;
    }

    if(gsl_linalg_LU_svx(matrix, permutation, product)) {
      fprintf(stderr, "nlls: gls_linalg_LU_solve() failed.\n");
      return -1;
    }

    gsl_matrix_free(matrix);
    gsl_permutation_free(permutation);

    // a1, a2, and a3 are now in "product", with a1 = delta(C), a2 =
    // delta(B), and a3 = delta(R)
      
    double delta_c    = 0.0;
    double delta_b    = 0.0;
    double delta_beta = 0.0;
    double delta_r    = 0.0;
    double delta_x0;
    double delta_y0;

    delta_beta = gsl_vector_get(product, FS_Beta);
    delta_r    = gsl_vector_get(product, FS_R);
    delta_c    = gsl_vector_get(product, FS_C);
    delta_b    = gsl_vector_get(product, FS_B);

    delta_x0   = gsl_vector_get(product, FS_X0);
    delta_y0   = gsl_vector_get(product, FS_Y0);

    gsl_vector_free(product);

    if(fabs(delta_c) > 0.25*fs.state_var[FS_C]) {
      if(delta_c < 0.0) delta_c = -0.25*fs.state_var[FS_C];
      else delta_c = 0.25*fs.state_var[FS_C];
      // fprintf(stderr, "clamped Delta(C)\n");
    }

    if(fabs(delta_r) > 0.25*fs.state_var[FS_R]) {
      // fprintf(stderr, "clamped Delta(R) from %f\n", delta_r);
      if(delta_r < 0.0) delta_r = -0.25*fs.state_var[FS_R];
      else delta_r = 0.25*fs.state_var[FS_R];
    }
    
    if(fabs(delta_b) > 100.0) {
      if(delta_b < 0.0) delta_b = -100.0;
      else delta_b = 100.0;
      // fprintf(stderr, "clamped Delta(B)\n");
    }
    
    if(fabs(delta_beta) > 0.25*fs.state_var[FS_Beta]) {
      if(delta_beta < 0.0) delta_beta = -0.25*fs.state_var[FS_Beta];
      else delta_beta = 0.25*fs.state_var[FS_Beta];
      // fprintf(stderr, "clamped beta\n");
    }

    fs.mel = sqrt(err_sq/(od->N-2));

    /* fprintf(stderr, "Delta_C = %f, delta_B = %f, delta_R = %f, delta_Beta = %f, m.e.l.=%f\n",
	    delta_c, delta_b, delta_r, delta_beta, fs->mel);
    fprintf(stderr, "  delta_x0 = %f, delta_y0 = %f\n", delta_x0, delta_y0);
    */

    fs.state_var[FS_R]     += delta_r;
    fs.state_var[FS_B]     += delta_b;
    fs.state_var[FS_C]     += delta_c;
    fs.state_var[FS_Beta]  += delta_beta;
    fs.state_var[FS_X0]    += delta_x0;
    fs.state_var[FS_Y0]    += delta_y0;

    if(fabs(fs.state_var[FS_X0]) > 2.0) {
      // fprintf(stderr, "Clamping x0\n");
      fs.state_var[FS_X0] = 0.0;
    }

    if(fabs(fs.state_var[FS_Y0]) > 2.0) {
      // fprintf(stderr, "Clamping y0\n");
      fs.state_var[FS_Y0] = 0.0;
    }

    quit = 0;
    if(fabs(delta_c) < 0.0001*fs.state_var[FS_C]) quit=1;
    loop_count++;
    if(loop_count < 8) quit = 0;
    if(loop_count > 50) return -1; // no convergence
  } while (!quit);

  for(int j=0; j<6; j++) {
    delete [] od->t[j];
  }
  delete [] od->xi;
  delete [] od->yi;
  delete [] od->y;
  delete [] od->t;
  delete [] od->err;
  delete od;

  IStarList::IStarOneStar *my_star = sl->FindByIndex(star_id);
  my_star->validity_flags |= (NLLS_FOR_XY | BKGD_VALID | COUNTS_VALID);
  my_star->nlls_x          = fs.state_var[FS_X0] + center_x;
  my_star->nlls_y          = fs.state_var[FS_Y0]*(17.0/19.7) + center_y;
  my_star->nlls_background = fs.state_var[FS_B];
  my_star->nlls_counts     = fs.state_var[FS_C];

  return 0;			// okay
}
