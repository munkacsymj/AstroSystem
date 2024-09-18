/*  drifter.cc -- Implements image drift management
 *
 *  Copyright (C) 2018 Mark J. Munkacsy

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

#include "drifter.h"
#include <time.h>		// time(), time_t
#include <unistd.h>		// sleep()
#include "scope_api.h"		// guide()
#include <gsl/gsl_vector.h>
#include <gsl/gsl_matrix.h>
#include <gsl/gsl_permutation.h>
#include <gsl/gsl_linalg.h>

static const int UPDATE_TIME = 10; /*seconds*/

static void my_gsl_err_handler (const char *reason,
				const char *file,
				int line,
				int gsl_errno) {
  fprintf(stderr, "gsl: %s\n", reason);
}

//****************************************************************
//        Class AxisDrifter
//****************************************************************

AxisDrifter::AxisDrifter(FILE *logfile, const char *name_of_axis) {
  log = logfile;
  axis_name = name_of_axis;
  cum_guidance_arcsec = 0.0;
  dscale = -2.0; // completely invalid; to be set later
  initialized = false;
  drift_rate = 0.0;
  drift_accel = 0.0;
}

AxisDrifter::~AxisDrifter(void) {
  std::list<AxisMeasurement *>::iterator it;
  for (it=measurements.begin(); it != measurements.end(); it++) {
    delete (*it);
  }
  fprintf(log, "Shutting down AxisDrifter(%s)\n", axis_name);
}

void
AxisDrifter::AcceptCenter(double measurement, JULIAN when) { // extracted from image
  if (!initialized) {
    if (dscale < 0.0) {
      fprintf(stderr, "drifter: scale never initialized.\n");
      return;
    }
    orig_position = measurement;
    orig_time = when;
  }
  initialized = true;
  AxisMeasurement *m = new AxisMeasurement;
  m->when = when;
  m->delta_t = 24.0*3600.0*(m->when - orig_time); // in seconds
  m->measured_posit = (180.0*3600.0/M_PI)* (measurement - orig_position); //arcsec
  m->cum_measured_posit = m->measured_posit + cum_guidance_arcsec;
  measurements.push_back(m);

  fprintf(log, "%s Measurements follow:\n", axis_name);
  std::list<AxisMeasurement *>::iterator it;
  for (it=measurements.begin(); it != measurements.end(); it++) {
    AxisMeasurement *m = (*it);
    fprintf(log, "%lf, meas=%lf, cum=%lf, weight=%lf\n",
	    m->delta_t, m->measured_posit, m->cum_measured_posit, m->weight);
  }

  // recalculate drift rate
  RecalculateDriftRate();
}

void
AxisDrifter::RecalculateDriftRate(void) {
  double weight = 1.0;
  std::list<AxisMeasurement *>::iterator it;

  if (measurements.size() < 2) {
    drift_rate = 0.0;
    drift_intercept = 0.0;
    drift_accel = 0.0;
    return;
  }

  reference_time = measurements.back()->when;
  gsl_matrix *sum_xx = gsl_matrix_calloc(3, 3);
  gsl_matrix *w = gsl_matrix_alloc(3, 1);
  gsl_matrix *sum_xy = gsl_matrix_calloc(3, 1);

  for (it=measurements.begin(); it != measurements.end(); it++) {
    AxisMeasurement *m = (*it);
    m->weight = weight;
    weight *= 1.05;
    m->delta_t = (m->when - reference_time)*24.0*3600.0; // seconds

    gsl_matrix_set(w, 0, 0, 1.0*weight);
    gsl_matrix_set(w, 1, 0, m->delta_t*weight);
    gsl_matrix_set(w, 2, 0, m->delta_t * m->delta_t*weight);

    gsl_blas_dgemm(CblasNoTrans, CblasTrans, 1.0, w, w, 1.0, sum_xx);

    gsl_matrix_scale(w, m->cum_measured_posit*weight);
    gsl_matrix_add(sum_xy, w);
  }

  gsl_permutation *p = gsl_permutation_alloc(3);
  gsl_matrix *inverse = gsl_matrix_alloc(3, 3);
  int s;
  gsl_linalg_LU_decomp(sum_xx, p, &s);
  gsl_linalg_LU_invert(sum_xx, p, inverse);
  gsl_matrix *result = gsl_matrix_calloc(3, 1);
  gsl_blas_dgemm(CblasNoTrans, CblasNoTrans, 1.0, inverse, sum_xy, 0.0, result);

  // extract the fitting parameters
  drift_intercept = gsl_matrix_get(result, 0, 0);
  drift_rate = gsl_matrix_get(result, 1, 0);
  drift_accel = gsl_matrix_get(result, 2, 0);

  gsl_matrix_free(result);
  gsl_matrix_free(inverse);
  gsl_matrix_free(sum_xy);
  gsl_matrix_free(sum_xx);
  gsl_matrix_free(w);
}

void
AxisDrifter::print(FILE *fp) {
  fprintf(fp, " rate = %lf\n", drift_rate);
}

void
AxisDrifter::ExposureStart(double duration, double update_period) { // duration in seconds
  // calculate pointing position for moment that is 1/2 update_period
  // beyond now and issue guiding command
  time_t time_target = time(0) + (int) (update_period/2.0);
  long time_offset = (time_target - reference_time.to_unix());
  double target_position = drift_intercept + drift_rate*time_offset + drift_accel*time_offset*time_offset/2.0; //arcsec
  double guide_amount = target_position - cum_guidance_arcsec;

  const int guidance_sign = (axis_is_dec && north_up) ? -1 : +1;

  // guide_amount is in arcsec. Convert to guiding time
#define GUIDE_RATE 3.75 // arcseconds per second of guiding time
  double guide_sec = (guide_amount/GUIDE_RATE);

  fprintf(log,
	  "%s: time_offset = %ld, drift_intercept = %lf, drift_rate = %lf, drift_accel = %lg, guide_amount = %lf, ",
	  axis_name, time_offset, drift_intercept, drift_rate, drift_accel, guide_amount);
  
  fprintf(log, "guide_sec = %lf\n", guide_sec);
  
  if (guide_sec < 8.000 && guide_sec > -8.000) {
    if (axis_is_dec) {
      guide(guidance_sign * guide_sec, 0.0);
    } else { // RA
      guide(0.0, -guide_sec/dscale); // assumes the GM2000 "Speed Correction" option is OFF
    }
    cum_guidance_arcsec += guide_amount;
  } else {
    fprintf(log, "unreasonable guide inhibited.\n");
  }
  fflush(log);
}

void
AxisDrifter::ExposureUpdate(double time_to_next_update) {
  ExposureStart(0.0, time_to_next_update);
}

Drifter::Drifter(FILE *logfile) {
  log = logfile;
  dec_drifter = new AxisDrifter(log, "DEC");
  dec_drifter->SetAxis(true); /*IsDec*/
  ra_drifter = new AxisDrifter(log, "RA");
  ra_drifter->SetAxis(false); /*IsNotDec*/
  (void) gsl_set_error_handler(&my_gsl_err_handler);
}

Drifter::~Drifter(void) {
  delete dec_drifter;
  delete ra_drifter;
  fprintf(log, "Shutting down drifter.\n");
  fclose(log);
}

void
Drifter::SetNorthUp(bool NorthUp) {
  dec_drifter->SetNorthUp(NorthUp);
  ra_drifter->SetNorthUp(NorthUp);
}

void
Drifter::AcceptCenter(DEC_RA center, JULIAN when) {
  ra_drifter->SetScale(cos(center.dec()));
  dec_drifter->SetScale(1.0);
  
  dec_drifter->AcceptCenter(center.dec(), when);
  ra_drifter->AcceptCenter(center.ra_radians(), when);
}

void
Drifter::ExposureStart(double duration) { // blocks for entire duration of exposure

  // Each of the following two may trigger mount corrections, so they
  // may take a while to complete and return control back to here. 
  dec_drifter->ExposureStart(duration, UPDATE_TIME);
  ra_drifter->ExposureStart(duration, UPDATE_TIME);

  exposure_start_time = time(0);
  exposure_duration = duration;
}

void
Drifter::ExposureGuide(void) {  // blocks for duration of exposure
  time_t now;
  time_t end_time = exposure_start_time + exposure_duration;
  
  do {
    // how long do we sleep for?
    now = time(0);
    time_t remaining = (end_time - now);
    long sleep_duration = (long) UPDATE_TIME;
    if (remaining < sleep_duration) sleep_duration = (long) remaining;
    sleep(sleep_duration);

    // finished sleeping, issue update commands if exposure isn't done
    now = time(0);
    if (now < end_time) {
      dec_drifter->ExposureUpdate(UPDATE_TIME);
      ra_drifter->ExposureUpdate(UPDATE_TIME);
    }
  } while (time(0) < end_time);
}
  
void
Drifter::print(FILE *fp) {
  fprintf(fp, "Dec drift: ");
  dec_drifter->print(fp);

  fprintf(fp, "RA drift: ");
  ra_drifter->print(fp);

  fflush(fp);
}
