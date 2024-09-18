/*  hyperbola.cc -- (Current) Hyperbola-matching to predict point of best focus
 *
 *  Copyright (C) 2015 Mark J. Munkacsy
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
#include <gsl/gsl_vector.h>
#include <gsl/gsl_matrix.h>
#include <gsl/gsl_permutation.h>
#include <gsl/gsl_linalg.h>
#include "hyperbola.h"

void my_gsl_err_handler (const char *reason,
			 const char *file,
			 int line,
			 int gsl_errno) {
  fprintf(stderr, "gsl: %s\n", reason);
}

// Our equation follows:
//
// (Y^2/a^2) - ((X-R)^2/b2) = 1
// X = encoder position (ticks)
// Y = blur
// R = encoder position of best focus (ticks)
// (b/a) = slope (blur/tick)
//

// Compute partial derivatives at each point
// (Partial derivatives came from
// http://www.derivative-calculator.net)

// (a/b) = slope (ticks/blur) == 64.0 (observed, and constrained)
//

static double C = 64.0;

void
Computet1t2t3(RunData *od, Hyperbola *fs) {

  const double &A     = fs->state_var[HYPER_A];
  const double &R     = fs->state_var[HYPER_R];

  for(int k=0; k<od->N; k++) {

    const double x = od->focus_position[k];
    const double rx = R - x;
    const double denom = sqrt(A*A + rx*rx/(C*C));

    od->t[HYPER_A][k] = A / denom;
    od->t[HYPER_R][k] = rx / (C*C * denom);

    const double modeled_value = denom;

    od->err[k] = od->star_size[k] - modeled_value;
  }
}

Hyperbola::Hyperbola(void) {
  reset();
}

void
Hyperbola::SetC(double ticks_per_blur) {
  C = ticks_per_blur;
}

Hyperbola::Hyperbola(double best_guess) {
  reset(best_guess);

  (void) gsl_set_error_handler(&my_gsl_err_handler);
}

void 
Hyperbola::reset(void) {
  state_var[HYPER_A]     = 0.5;
  state_var[HYPER_R]     = 2000;
}

void 
Hyperbola::reset(double best_guess) {
  state_var[HYPER_A]     = 0.5;
  state_var[HYPER_R]     = best_guess;
}

void 
Hyperbola::reset(Hyperbola *p) {
  state_var[HYPER_A]     = p->state_var[HYPER_A];
  state_var[HYPER_R]     = p->state_var[HYPER_R];
}

int
nlls_hyperbola(Hyperbola *fs, RunData *run_data) {
  int quit;

  int loop_count = 0;
  const int order = 2;
  double old_mel = 0.0;

  run_data->print(stderr);

  do {
    // compute t1, t2, t3 for all points, putting them into "od"
    fprintf(stderr, "A = %lf, R = %lf\n",
	    fs->state_var[HYPER_A],
	    fs->state_var[HYPER_R]);

    Computet1t2t3(run_data, fs);

    gsl_matrix *matrix = gsl_matrix_calloc(order, order);
    if(matrix == 0) return -1;

    gsl_vector *product = gsl_vector_calloc(order);
    if(!product) {
      fprintf(stderr, "pfocus: allocation of product vector failed.\n");
    }

    gsl_permutation *permutation = gsl_permutation_alloc(order);
    if(!permutation) {
      fprintf(stderr, "pfocus: permutation create failed.\n");
    }

    double err_sq = 0.0;

    for(int n = 0; n < run_data->N; n++) {
      for(int b = 0; b < order; b++) {
	(*gsl_vector_ptr(product, b)) += run_data->t[b][n] * run_data->err[n];

	for(int c = b; c < order; c++) {
	    ((*gsl_matrix_ptr(matrix, b, c)) +=
	     run_data->t[b][n] * run_data->t[c][n]);
	}
      }
      err_sq += run_data->err[n] * run_data->err[n];
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
      fprintf(stderr, "pfocus: gsl_linalg_LU_decomp() failed.\n");
      return -1;
    }

    if(gsl_linalg_LU_svx(matrix, permutation, product)) {
      fprintf(stderr, "pfocus: gls_linalg_LU_solve() failed.\n");
      return -1;
    }

    gsl_matrix_free(matrix);
    gsl_permutation_free(permutation);

    double delta_a    = 0.0;
    double delta_r    = 0.0;

    delta_a    = gsl_vector_get(product, HYPER_A);
    delta_r    = gsl_vector_get(product, HYPER_R);

    gsl_vector_free(product);

    fs->mel = sqrt(err_sq/(run_data->N-2));

    fprintf(stderr, "errsq = %f\n", err_sq);
    /* fprintf(stderr, "delta A = %f, delta B = %f, delta C = %f, mel = %f\n",
	    delta_a, delta_b, delta_c, fs->mel); */

#if 0
    if(delta_a < -fs->state_var[HYPER_A]) {
      delta_a = -0.9 * fs->state_var[HYPER_A];
      fprintf(stderr, "delta_a clamped.\n");
    }
#endif

    fs->state_var[HYPER_A]     += delta_a;
    fs->state_var[HYPER_R]     += delta_r;

    // clamp various extreme values for A and R
    if (fs->state_var[HYPER_A] < 0.05) {
      fs->state_var[HYPER_A] = 0.05;
    }
    if (fs->state_var[HYPER_A] > 10.0) {
      fs->state_var[HYPER_A] = 10.0;
    }
    if (fs->state_var[HYPER_R] < 0.0) {
      fs->state_var[HYPER_R] = 0.0;
    }
    if (fs->state_var[HYPER_R] > 3000.0) {
      fs->state_var[HYPER_R] = 3000.0;
    }

#if 0
    if(fs->state_var[HYPER_R] > 5.0) {
      fs->state_var[HYPER_R] = 5.0;
      fprintf(stderr, "C clamped.\n");
    }
    if(fs->state_var[HYPER_R] < 0.0) {
      fs->state_var[HYPER_R] = 0.0;
      fprintf(stderr, "C clamped(0).\n");
    }
#endif

    quit = 0;
    loop_count++;
    if(fabs(fs->mel - old_mel) < 0.0001) quit=1;
    if(loop_count > 30) return -1; // no convergence
    old_mel = fs->mel;
  } while (!quit);

  return fs->state_var[HYPER_R]; // okay
}

void test_hyperbola(void) {
  RunData run_data;
#if 1
  run_data.add(1182, 14.866);
  run_data.add(1232, 10.63);
  run_data.add(1282, 8.246);
  run_data.add(1332, 8.062);
  run_data.add(1132, 21.40);
  run_data.add(1072, 29.73);
  run_data.add(1372, 10.63);
  run_data.add(1431, 15.81);
#else
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
#endif
  Hyperbola h;
  h.reset();
  h.SetC(7.4);
  int x = nlls_hyperbola(&h, &run_data);
  fprintf(stderr, "nlls_hyperbola returned %d\n", x);
  fprintf(stderr, "A = %lf\n", h.state_var[HYPER_A]);
  fprintf(stderr, "B = %lf\n", C * h.state_var[HYPER_A]);
  fprintf(stderr, "R = %lf\n", h.state_var[HYPER_R]);
}

void 
RunData::print(FILE *fp) {
  for (int i=0; i<N; i++) {
    fprintf(fp, "%lf, %lf\n", focus_position[i], star_size[i]);
  }
}

			 
void RunData::add(double position, double size) {
  focus_position[N] = position;
  star_size[N++] = size;
}
