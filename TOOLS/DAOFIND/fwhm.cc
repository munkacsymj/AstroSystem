/*  fwhm.cc -- Measure actual stars' FWHMx and FWHMy
 *
 *  Copyright (C) 2021 Mark J. Munkacsy

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

#include "fwhm.h"
#include <pthread.h>
#include <list>
#include <gsl/gsl_rng.h>
#include <gsl/gsl_matrix.h>
#include <gsl/gsl_vector.h>
#include <gsl/gsl_blas.h>
#include <gsl/gsl_multifit_nlinear.h>
#include <Image.h>
#include "egauss.h"
#include <iostream>

#define FWHM2SIGMA 0.42467 

struct FWHMData {
  double FWHMx;
  double FWHMy;
  double amplitude;
};

struct datapoint {
  size_t n;
  double *x;
  double *y;
  double *v;
};

// Parameters:
// A: 0
// B: 1
// 2*sig_x^2: 2
// 2*sig_y^2: 3
#define NUM_PARAM 4
#define PARAM_A 0
#define PARAM_B 1
#define PARAM_2SIGXX 2
#define PARAM_2SIGYY 3


int expb_f(const gsl_vector *x, void *data, gsl_vector *f) {
  const datapoint *dp = (const datapoint *) data;
  const double A = gsl_vector_get(x, PARAM_A);
  const double B = gsl_vector_get(x, PARAM_B);
  const double SIGXX2 = gsl_vector_get(x, PARAM_2SIGXX);
  const double SIGYY2 = gsl_vector_get(x, PARAM_2SIGYY);

  for (size_t i=0; i<dp->n; i++) {
    const double zx = dp->x[i];
    const double zy = dp->y[i];
    const double p = B+A*exp(-(zx*zx/SIGXX2 + zy*zy/SIGYY2));
    gsl_vector_set(f, i, p-dp->v[i]);
  }
  return GSL_SUCCESS;
}

int  expb_df(const gsl_vector *x, void *data, gsl_matrix *J) {
  const datapoint *dp = (const datapoint *) data;
  const double A = gsl_vector_get(x, PARAM_A);
  //const double B = gsl_vector_get(x, PARAM_B);
  const double SIGXX2 = gsl_vector_get(x, PARAM_2SIGXX);
  const double SIGYY2 = gsl_vector_get(x, PARAM_2SIGYY);

  for (size_t i=0; i<dp->n; i++) {
    const double zx = dp->x[i];
    const double zy = dp->y[i];
    const double u = exp(-(zx*zx/SIGXX2 + zy*zy/SIGYY2));
    gsl_matrix_set(J, i, PARAM_A, u);
    gsl_matrix_set(J, i, PARAM_B, 1.0);
    gsl_matrix_set(J, i, PARAM_2SIGXX, A*zx*zx*u/(SIGXX2*SIGXX2));
    gsl_matrix_set(J, i, PARAM_2SIGYY, A*zy*zy*u/(SIGYY2*SIGYY2));
  }
  return GSL_SUCCESS;
}

void callback(const size_t iter, void *params, const gsl_multifit_nlinear_workspace *w) {
#if 0
  gsl_vector *f = gsl_multifit_nlinear_residual(w);
  gsl_vector *x = gsl_multifit_nlinear_position(w);
  double rcond;

  gsl_multifit_nlinear_rcond(&rcond, w);

  fprintf(stderr, "iter %2zu: A = %.1f, B = %.1f, SIGXX2 = %.3f, SIGYY2 = %.3f, cond(J) = %.4f, |f(x)| = %.4f\n",
	  iter,
	  gsl_vector_get(x, PARAM_A),
	  gsl_vector_get(x, PARAM_B),
	  gsl_vector_get(x, PARAM_2SIGXX),
	  gsl_vector_get(x, PARAM_2SIGYY),
	  1.0/rcond,
	  gsl_blas_dnrm2(f));
#else
  ;
#endif
}


typedef std::list<FWHMData> ResultList;

struct ThreadData {
  unsigned int thread_id;
  unsigned int thread_count;
  ResultList results;
  DAOStarlist *stars;
  Image *image;
  FWHMParam *params;
};

static constexpr int NUM_THREADS = 6;

void *thread_measure_fwhm(void *raw_data) {
  ThreadData *data = (ThreadData *) raw_data;
  ImageInfo *iinfo = data->image->GetImageInfo();
  const int y_edge = ((iinfo && iinfo->FrameXYValid()) &&
		      iinfo->GetFrameY() < 10 ? 10 - iinfo->GetFrameY() : 0);
  const int x_edge = 0;
  const int nx = data->params->rp->gauss->nx;
  const int ny = data->params->rp->gauss->ny;
  const int nx2 = (nx-1)/2;
  const int ny2 = (ny-1)/2;

  for (unsigned int i=data->thread_id; i<data->stars->size(); i += data->thread_count) {
    auto star = (*data->stars)[i];
    // make sure entire star is in valid region of the image
    const int start_x = (int)(star->x + 0.5) - nx2;
    const int start_y = (int)(star->y + 0.5) - ny2;
    const int end_x = start_x + nx;
    const int end_y = start_y + ny;

    if(start_x < x_edge or
       end_x >= data->image->width or
       start_y < y_edge or
       end_y >= data->image->height or
       !star->valid) continue;

    // We are good to go
    gsl_multifit_nlinear_fdf fdf;
    gsl_multifit_nlinear_parameters fdf_params = gsl_multifit_nlinear_default_parameters();
    const size_t n = ny*nx;
    gsl_matrix *covar = gsl_matrix_alloc(NUM_PARAM, NUM_PARAM);
    double px[n], py[n], pv[n];
    struct datapoint d { n, px, py, pv };
    const double background = data->image->pixel(start_x, start_y); // a corner
    const double peak = data->image->pixel((int)(star->x + 0.5),
					   (int)(star->y + 0.5));
    //fprintf(stderr, "fwhm(), background = %.1lf, peak = %.1lf, @(%.1lf,%.1lf)\n",
    //	    background, peak, star->x, star->y);
    double x_init[NUM_PARAM] {
		   peak-background, // A
		     background, // B
		     2.0*data->params->FWHMx*data->params->FWHMx*FWHM2SIGMA*FWHM2SIGMA,
		     2.0*data->params->FWHMy*data->params->FWHMy*FWHM2SIGMA*FWHM2SIGMA
		     };
    gsl_vector_view x = gsl_vector_view_array(x_init, NUM_PARAM);

    gsl_rng_env_setup();
    gsl_rng *r = gsl_rng_alloc(gsl_rng_default);
    fdf.f = expb_f;
    fdf.df = expb_df;
    fdf.fvv = nullptr;
    fdf.n = n;
    fdf.p = NUM_PARAM;
    fdf.params = &d;
	
    int counter = 0;
    for (int y=start_y; y < end_y; y++) {
      const double del_y = (y-star->y);
      for (int x=start_x; x < end_x; x++) {
	const double del_x = (x-star->x);
	const double v = data->image->pixel(x,y);
	px[counter] = del_x;
	py[counter] = del_y;
	pv[counter] = v;
	counter++;
      }
    }

    const gsl_multifit_nlinear_type *T = gsl_multifit_nlinear_trust;
    gsl_multifit_nlinear_workspace *w = gsl_multifit_nlinear_alloc(T, &fdf_params, n, NUM_PARAM);
    gsl_multifit_nlinear_init(&x.vector, &fdf, w);
    gsl_vector *f = gsl_multifit_nlinear_residual(w);
    double chisq0;
    gsl_blas_ddot(f, f, &chisq0);
    const double xtol = 0.001;
    const double gtol = 0.001;
    const double ftol = 0.0;
    int info;
    int status = gsl_multifit_nlinear_driver(100, xtol, gtol, ftol, nullptr, nullptr, &info, w);
    double chisq;
    gsl_blas_ddot(f, f, &chisq);

#define FIT(i) gsl_vector_get(w->x, i)
#define ERR(i) sqrt(gsl_matrix_get(covar, i, i))      

#if 0
    fprintf(stderr, "summary from method '%s/%s'\n",
	    gsl_multifit_nlinear_name(w),
	    gsl_multifit_nlinear_trs_name(w));
    fprintf(stderr, "number of iterations: %zu\n",
	    gsl_multifit_nlinear_niter(w));
    fprintf(stderr, "function evaluations: %zu\n", fdf.nevalf);
    fprintf(stderr, "Jacobian evaluations: %zu\n", fdf.nevaldf);
    fprintf(stderr, "reason for stopping: %s\n",
	    (info == 1) ? "small step size" : "small gradient");
    fprintf(stderr, "initial |f(x)| = %f\n", sqrt(chisq0));
    fprintf(stderr, "final   |f(x)| = %f\n", sqrt(chisq));
    {
      double dof = n - NUM_PARAM;
      double c = GSL_MAX_DBL(1, sqrt(chisq / dof));

      fprintf(stderr, "chisq/dof = %g\n", chisq / dof);

      const double f2x = FIT(PARAM_2SIGXX);
      const double f2y = FIT(PARAM_2SIGYY);

      fprintf (stderr, "A      = %.5f +/- %.5f\n", FIT(PARAM_A), c*ERR(PARAM_A));
      fprintf (stderr, "B      = %.5f +/- %.5f\n", FIT(PARAM_B), c*ERR(PARAM_B));
      fprintf (stderr, "FWHM_X = %.5f\n", sqrt(f2x/2.0)/FWHM2SIGMA);
      fprintf (stderr, "FWHM_Y = %.5f\n", sqrt(f2y/2.0)/FWHM2SIGMA);
    }

    fprintf (stderr, "status = %s\n", gsl_strerror (status));
#endif

    star->valid = (status == 0 and
		   FIT(PARAM_A) > 0.0 and
		   FIT(PARAM_2SIGXX) > 0.5 and
		   FIT(PARAM_2SIGYY) > 0.5 and
		   FIT(PARAM_2SIGXX) < 50.0 and
		   FIT(PARAM_2SIGYY) < 50.0);
    //fprintf(stderr, "star->valid = %s\n", star->valid ? "true" : "false");

    if (star->valid) {
      const double fwhmx = sqrt(FIT(PARAM_2SIGXX)/2.0)/FWHM2SIGMA;
      const double fwhmy = sqrt(FIT(PARAM_2SIGYY)/2.0)/FWHM2SIGMA;
      FWHMData d;
      d.FWHMx = fwhmx;
      d.FWHMy = fwhmy;
      d.amplitude = FIT(PARAM_A);
      
      data->results.push_back(d);
    }

    gsl_multifit_nlinear_free (w);
    gsl_matrix_free (covar);
    gsl_rng_free (r);

  }
  return nullptr;
}

void measure_fwhm(DAOStarlist &stars, Image &image, FWHMParam &params) {
  pthread_t thread_ids[NUM_THREADS];
  ThreadData thread_data[NUM_THREADS];
  for (int i=0; i<NUM_THREADS; i++) {
    ThreadData *data = &thread_data[i];
    data->thread_id = i;
    data->thread_count = NUM_THREADS;
    data->stars = &stars;
    data->image = &image;
    data->params = &params;
    int err = pthread_create(&thread_ids[i],
			     nullptr, // attributes for the thread
			     &thread_measure_fwhm,
			     &thread_data[i]);
    if (err) {
      std::cerr << "Error creating thread in fwhm.cc: %d\n" << err << std::endl;
    }
  }

  double sum_fwhmx = 0.0;
  double sum_fwhmy = 0.0;
  int star_count = 0;

  for (int i=0; i<NUM_THREADS; i++) {
    int s = pthread_join(thread_ids[i], nullptr);
    if (s != 0) {
      perror("pthread_join");
    } else {
      for (auto r : thread_data[i].results) {
	sum_fwhmx += r.FWHMx;
	sum_fwhmy += r.FWHMy;
	star_count++;
      }
    }
  }

  if (star_count) {
    params.FWHMx = sum_fwhmx/star_count;
    params.FWHMy = sum_fwhmy/star_count;
    fprintf(stderr, "Final aggregate FWHMx = %.2lf pixels, FWHMy = %.2lf pixels\n",
	    params.FWHMx, params.FWHMy);
    params.valid = true;
  } else {
    params.valid = false;
  }
}
