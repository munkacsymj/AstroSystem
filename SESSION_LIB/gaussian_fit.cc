/*  gaussian_fit.cc -- Gaussian-matching
 *
 *  Copyright (C) 2015, 2018, 2021 Mark J. Munkacsy
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
#include <math.h>		// for fabs()
#include <list>
#include <gsl/gsl_vector.h>
#include <gsl/gsl_matrix.h>
#include <gsl/gsl_permutation.h>
#include <gsl/gsl_linalg.h>
#include "gaussian_fit.h"

double GaussianR0 = 0.0;

void my_gsl_err_handler (const char *reason,
			 const char *file,
			 int line,
			 int gsl_errno) {
  fprintf(stderr, "gsl: %s\n", reason);
}

// Our equation follows:
//
// f(x) = A * exp(-x^2/S^2)
// A = intensity scale factor
// S = sigma (shape)
//

// Compute partial derivatives at each point
// (Partial derivatives came from
// http://www.derivative-calculator.net)

void
Computet1t2t3(GRunData *od, Gaussian *fs) {

  const double &A     = fs->state_var[GAUSSIAN_A];
  const double &S     = fs->state_var[GAUSSIAN_S];
  const double &R     = GaussianR0; //fs->state_var[GAUSSIAN_R0];
  //constexpr double R  = 0.0;
  const double &B     = fs->state_var[GAUSSIAN_B];
  const double &X0    = fs->state_var[GAUSSIAN_X0];
  const double &Y0    = fs->state_var[GAUSSIAN_Y0];
  const double R_SQ   = R*R;

  std::list<GRunPoint *>::iterator it;
  for(it = od->all_points.begin(); it != od->all_points.end(); it++) {
    GRunPoint *rp = (*it);

    const double x = rp->pixel_x - X0;
    const double y = rp->pixel_y - Y0;
    const double x_sq = x*x;
    const double y_sq = y*y;
    const bool in_disc = x_sq+y_sq < R_SQ;
    const double q = (in_disc ? 0.0 : x_sq+y_sq-R_SQ);
    
    const double f1 = exp(-q/(S*S));

    rp->t[GAUSSIAN_A] = f1;
    rp->t[GAUSSIAN_B] = 1.0;
    rp->t[GAUSSIAN_S] = 2 * A * q * f1 / (S*S*S);
    //rp->t[GAUSSIAN_R0] = (in_disc ? 0.0 : 2*A*R*f1/(S*S));
    rp->t[GAUSSIAN_X0] =  2*A*x*f1/(S*S);
    rp->t[GAUSSIAN_Y0] =  2*A*y*f1/(S*S);

    const double modeled_value = A * f1 + B;

    rp->err = rp->intensity - modeled_value;
    //fprintf(stderr, "r = %.1lf, meas = %.1lf, model = %.1lf, diff[A] = %.1lf, diff[S] = %.1lf\n",
    //	    x, rp->intensity, modeled_value,
    //	    rp->t[GAUSSIAN_A], rp->t[GAUSSIAN_S]);
  }
}

Gaussian::Gaussian(void) {
  reset();
}

void 
Gaussian::reset(void) {
  state_var[GAUSSIAN_A]     = 1000.0;
  state_var[GAUSSIAN_S]     = 10.0;
  //state_var[GAUSSIAN_R0]    = 1.0;
  state_var[GAUSSIAN_B]     = 0.0;
  state_var[GAUSSIAN_X0]    = 1.0;
  state_var[GAUSSIAN_Y0]    = 1.0;
}

void 
Gaussian::reset(Gaussian *p) {
  state_var[GAUSSIAN_A]     = p->state_var[GAUSSIAN_A];
  state_var[GAUSSIAN_S]     = p->state_var[GAUSSIAN_S];
  //state_var[GAUSSIAN_R0]    = p->state_var[GAUSSIAN_R0];
  state_var[GAUSSIAN_B]     = p->state_var[GAUSSIAN_B];
  state_var[GAUSSIAN_X0]    = p->state_var[GAUSSIAN_X0];
  state_var[GAUSSIAN_Y0]    = p->state_var[GAUSSIAN_Y0];
}

int
nlls_gaussian(Gaussian *fs, GRunData *run_data) {
  int quit;

  fs->converged = 0;
  int loop_count = 0;
  constexpr int order = 5;
  double old_mel = 0.0;

  double sum_x = 0.0;
  double sum_y = 0.0;
  double max_v = -99.9e99;
  double min_v = 99.9e99;
  //double max_x = -99.9e99;
  //double min_x = 99.9e99;
  for (auto rp : run_data->all_points) {
    sum_x += rp->pixel_x;
    sum_y += rp->pixel_y;
    if (rp->intensity > max_v) max_v = rp->intensity;
    if (rp->intensity < min_v) min_v = rp->intensity;
    //if (rp.pixel_x > max_x) max_x = rp.pixel_x;
    //if (rp.pixel_x < min_x) min_x = rp.pixel_x;
  }

  //const double spanx = max_x - min_x;

  // some reasonable initial values...
  fs->state_var[GAUSSIAN_A] = max_v;
  fs->state_var[GAUSSIAN_B] = min_v;
  fs->state_var[GAUSSIAN_X0] = sum_x/run_data->all_points.size();
  fs->state_var[GAUSSIAN_Y0] = sum_y/run_data->all_points.size();

  fprintf(stderr, "Initial guesses:\n");
  fprintf(stderr, "A : %lf, B : %lf, X0 : %lf, Y0 : %lf\n",
	  fs->state_var[GAUSSIAN_A],
	  fs->state_var[GAUSSIAN_B],
	  fs->state_var[GAUSSIAN_X0],
	  fs->state_var[GAUSSIAN_Y0]);
    
  //run_data->print(stderr);

  do {
    // compute t1, t2, t3 for all points, putting them into "od"
    // fprintf(stderr, "A = %lf, R = %lf\n",
    //	    fs->state_var[GAUSSIAN_A],
    //	    fs->state_var[GAUSSIAN_S]);

    Computet1t2t3(run_data, fs);

    gsl_matrix *matrix = gsl_matrix_calloc(order, order);
    if(matrix == 0) return -1;

    gsl_vector *product = gsl_vector_calloc(order);
    if(!product) {
      fprintf(stderr, "gaussian_fit: allocation of product vector failed.\n");
    }

    gsl_permutation *permutation = gsl_permutation_alloc(order);
    if(!permutation) {
      fprintf(stderr, "gaussian_fit: permutation create failed.\n");
    }

    double err_sq = 0.0;

    std::list<GRunPoint *>::iterator it;
    for(it = run_data->all_points.begin(); it != run_data->all_points.end(); it++) {
      for(int b = 0; b < order; b++) {
	(*gsl_vector_ptr(product, b)) += (*it)->t[b] * (*it)->err;

	for(int c = b; c < order; c++) {
	    (*gsl_matrix_ptr(matrix, b, c)) +=
	      ((*it)->t[b] * (*it)->t[c]);
	}
      }
	err_sq += (*it)->err * (*it)->err;
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
      fprintf(stderr, "gaussian_fit: gsl_linalg_LU_decomp() failed.\n");
      return -1;
    }

    if(gsl_linalg_LU_svx(matrix, permutation, product)) {
      fprintf(stderr, "gaussian_fit: gls_linalg_LU_solve() failed.\n");
      return -1;
    }

    gsl_matrix_free(matrix);
    gsl_permutation_free(permutation);

    double delta_a    = 0.0;
    double delta_s    = 0.0;
    double delta_b    = 0.0;
    double delta_r    = 0.0;
    double delta_x0   = 0.0;
    double delta_y0   = 0.0;

    delta_a    = gsl_vector_get(product, GAUSSIAN_A);
    delta_s    = gsl_vector_get(product, GAUSSIAN_S);
    delta_b    = gsl_vector_get(product, GAUSSIAN_B);
    //delta_r    = gsl_vector_get(product, GAUSSIAN_R0);
    delta_x0   = gsl_vector_get(product, GAUSSIAN_X0);
    delta_y0   = gsl_vector_get(product, GAUSSIAN_Y0);

    gsl_vector_free(product);

    fs->mel = sqrt(err_sq/(run_data->N-2));

    fprintf(stderr, "errsq = %lf\n", err_sq);
    fprintf(stderr, "Deltas: A = %lf, S = %lf, B = %lf, R0 = %lf, X0 = %lf, Y0 = %lf, mel = %f\n",
    	    delta_a, delta_s, delta_b, delta_r, delta_x0, delta_y0, fs->mel);

    fs->state_var[GAUSSIAN_A]     += delta_a;
    fs->state_var[GAUSSIAN_B]     += delta_b;
    fs->state_var[GAUSSIAN_X0]    += delta_x0;
    fs->state_var[GAUSSIAN_Y0]    += delta_y0;
    if (fabs(delta_s) > fs->state_var[GAUSSIAN_S]/2.0) {
      if (delta_s < 0.0) {
	fs->state_var[GAUSSIAN_S] /= 2.0;
      } else {
	fs->state_var[GAUSSIAN_S] *= 1.5;
      }
    } else {
      fs->state_var[GAUSSIAN_S]     += delta_s;
    }

    if (fs->state_var[GAUSSIAN_A] < 0.0) {
      fs->state_var[GAUSSIAN_A] = 1.0;
    }
    if (fs->state_var[GAUSSIAN_S] < 0.001) {
      fs->state_var[GAUSSIAN_S] -= delta_s;
      fs->state_var[GAUSSIAN_S] /= 2.0;
    }

#if 0
    fs->state_var[GAUSSIAN_R0] += delta_r;
    //fs->state_var[GAUSSIAN_R0] = 0.1;
    if (fs->state_var[GAUSSIAN_R0] < 0.1) {
      fs->state_var[GAUSSIAN_R0] = 0.1;
    }
    if (fs->state_var[GAUSSIAN_R0] > spanx/2.0) {
      fs->state_var[GAUSSIAN_R0] = spanx/2.0;
    }
#endif

    fprintf(stderr, "new: A = %.2lf, S = %lf, B = %.2lf, R = %.2lf, X0 = %.3lf, Y0 = %.3lf\n",
    	    fs->state_var[GAUSSIAN_A], 
    	    fs->state_var[GAUSSIAN_S],
    	    fs->state_var[GAUSSIAN_B],
	    GaussianR0,
    	    //fs->state_var[GAUSSIAN_R0],
    	    fs->state_var[GAUSSIAN_X0],
    	    fs->state_var[GAUSSIAN_Y0]);
	    
    quit = 0;
    loop_count++;
    if(fabs(fs->mel - old_mel) < 0.0001) quit=1;
    if(loop_count > 30) return -1; // no convergence
    old_mel = fs->mel;
  } while (!quit);

  fs->converged = 1;
  return 0;
  // return fs->state_var[GAUSSIAN_S]; // okay
}

#if 0
void test_gaussian(void) {
  GRunData run_data;
  //#if 1
  run_data.add(1182, 14.866);
  run_data.add(1232, 10.63);
  run_data.add(1282, 8.246);
  run_data.add(1332, 8.062);
  run_data.add(1132, 21.40);
  run_data.add(1072, 29.73);
  run_data.add(1372, 10.63);
  run_data.add(1431, 15.81);
  //#else
  // 7-5-2015 data
  run_data.add(1888, 2.904);
  run_data.add(1908, 2.439);
  run_data.add(1928, 2.391);

  // old data
  run_data.add(1505, 2.901);
  run_data.add(1505, 2.901);
  run_data.add(1605, 4.277);
  run_data.add(1705, 6.001);
  run_data.add(1805, 7.512);
  run_data.add(1772, 6.949);
  run_data.add(1772, 6.985);
  run_data.add(1739, 6.380);
  run_data.add(1739, 6.457);
  run_data.add(1706, 6.022);
  run_data.add(1706, 5.933);
  run_data.add(1674, 5.393);
  run_data.add(1674, 5.479);
  run_data.add(1674, 5.304);
  run_data.add(1642, 5.073);
  run_data.add(1642, 4.957);
  run_data.add(1611, 4.507);
  run_data.add(1611, 4.684);
  run_data.add(1580, 3.832);
  run_data.add(1580, 4.087);
  run_data.add(1549, 3.403);
  run_data.add(1549, 3.408);
  run_data.add(1519, 2.927);
  run_data.add(1519, 3.149);
  run_data.add(1489, 2.548);
  run_data.add(1489, 2.636);
  run_data.add(1459, 2.249);
  run_data.add(1459, 2.043);
  run_data.add(1429, 1.841);
  run_data.add(1429, 1.919);
  run_data.add(1399, 1.433);
  run_data.add(1399, 1.327);
  run_data.add(1369, 0.559);
  run_data.add(1369, 0.556);
  run_data.add(1339, 1.198);
  run_data.add(1339, 0.909);
  run_data.add(1309, 0.864);
  run_data.add(1279, 1.548);
  run_data.add(1279, 1.253);
  run_data.add(1249, 1.611);
  run_data.add(1249, 1.733);
  run_data.add(1219, 1.788);
  run_data.add(1219, 1.925);
  run_data.add(1189, 2.408);
  run_data.add(1189, 2.461);
  run_data.add(1159, 2.847);
  run_data.add(1159, 2.925);
  run_data.add(1159, 2.740);
  run_data.add(1129, 3.351);
  run_data.add(1129, 3.121);
  run_data.add(1039, 4.772);
  run_data.add(949, 6.135);
  run_data.add(949, 6.177);
  //#endif
  Gaussian h;
  h.reset();

  int x = nlls_gaussian(&h, &run_data);
  fprintf(stderr, "nlls_gaussian returned %d\n", x);
  fprintf(stderr, "A = %lf\n", h.state_var[GAUSSIAN_A]);
  //fprintf(stderr, "B = %lf\n", C * h.state_var[GAUSSIAN_A]);
  //fprintf(stderr, "R = %lf\n", h.state_var[GAUSSIAN_S]);
}
#endif

void 
GRunData::print(FILE *fp) {
  std::list<GRunPoint *>::iterator it;
  for(it = all_points.begin(); it != all_points.end(); it++) {
    fprintf(fp, "(%lf, %lf) %lf\n", (*it)->pixel_x, (*it)->pixel_y, (*it)->intensity);
  }
}

			 
