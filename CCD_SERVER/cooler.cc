/*  cooler.cc -- Manages camera cooler
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

#include "ccd_server.h"
#include "ccd_message_handler.h"
#include "ambient.h"
#include "cooler.h"

#include <qhyccd.h>

#include <stdio.h>
#include <iostream>
#include <pthread.h>
#include <list>
#include <unistd.h>		// sleep()
#include <math.h>

#include <gsl/gsl_vector.h>
#include <gsl/gsl_matrix.h>
#include <gsl/gsl_permutation.h>
#include <gsl/gsl_linalg.h>
#include <gsl/gsl_multifit.h>
#include <gsl/gsl_blas.h>

extern qhyccd_handle *camhandle;

static pthread_t thread_id;
static CoolerData cooler_data;
static const char *COOLER_LOGFILE = "/home/mark/ASTRO/LOGS/cooler.log";
static FILE *cooler_log = 0;
static int cooler_cycle_time = 2; // seconds

void *RunCooler(void *args);
void RefreshCoolerStatus(void);
void DoRegulation(void);
void ControlCooler(void);
void ResetIntegrator(void);

CoolerData *GetCoolerData(void) { return &cooler_data; }

//****************************************************************
//        Linear fitter
// Three unknowns being fitted:
//        - slope_ratio (seconds)
//        - ambient_offset (degrees)
//        - power_ratio (degrees/power)
//****************************************************************

struct FitterPoint {
  int power;
  double ccd_temp;
  double ambient_temp;
  double slope;
};

struct FittingResults {
  double slope_ratio;
  double ambient_offset;
  double power_ratio;
};

std::list<FitterPoint *> measurements;

static double sum_x = 0.0;
static double sum_y = 0.0;
static double sum_xx = 0.0;
static double sum_yy = 0.0;
static double sum_xy = 0.0;
static int points = 0;

void FitterAcceptPoint(int power,
		       double ccd_temp,
		       double ambient_temp,
		       double slope) {
  const double y = ambient_temp - ccd_temp;
  const double x = power;

  sum_x += x;
  sum_y += y;
  sum_xx += (x*x);
  sum_yy += (y*y);
  sum_xy += (x*y);
  points++;
  
#if 0
  // determine if unique...
  for (auto m: measurements) {
    if (m->power == power and
	m->ccd_temp == ccd_temp and
	m->ambient_temp == ambient_temp and
	fabs(slope - m->slope) <  0.001) return;
  }
  
  FitterPoint *fp = new FitterPoint;
  fp->power = power;
  fp->ccd_temp = ccd_temp;
  fp->ambient_temp = ambient_temp;
  fp->slope = slope;
  measurements.push_back(fp);
#endif
}

void GetFittingParams(FittingResults &result) {
#if 0
  gsl_matrix *X = gsl_matrix_alloc(measurements.size(), 2);
  gsl_vector *y = gsl_vector_alloc(measurements.size());
  gsl_vector *q = gsl_vector_alloc(2);
  gsl_matrix *cov = gsl_matrix_alloc(2,2);

  int i=0;
  for (auto p: measurements) {
    gsl_matrix_set(X, i, 0, 1.0); // ambient_offset
    //gsl_matrix_set(X, i, 1, -p->slope); // slope_ratio
    gsl_matrix_set(X, i, 1, p->power);
    gsl_vector_set(y, i, (p->ambient_temp - p->ccd_temp));
  }

  double chisq;
  gsl_multifit_linear_workspace *work = gsl_multifit_linear_alloc(measurements.size(), 2);
  gsl_multifit_linear(X, y, q, cov, &chisq, work);
  gsl_multifit_linear_free(work);

  result.ambient_offset = gsl_vector_get(q, 0);
  //result.slope_ratio = gsl_vector_get(q, 1);
  result.slope_ratio = 0.0;
  result.power_ratio = gsl_vector_get(q, 1);

  gsl_vector_free(q);
  gsl_matrix_free(cov);
  gsl_vector_free(y);
  gsl_matrix_free(X);
#endif
  const double slope = (points*sum_xy - sum_x*sum_y)/(points*sum_xx - sum_x*sum_x);
  const double offset = (sum_y - slope*sum_x)/points;
  result.ambient_offset = offset;
  result.power_ratio = slope;
}
  

//****************************************************************
//        End of linear fitter
//****************************************************************

void RefreshData(void) {
  cooler_data.ambient_avail = AmbientTempAvail();
  if (cooler_data.ambient_avail) cooler_data.CoolerCurrentAmbient = AmbientCurrentDegC();
  GetCameraLock();
  cooler_data.CoolerCurrentChipTemp = GetQHYCCDParam(camhandle, CONTROL_CURTEMP);
  //cooler_data.CoolerCurrentPWM = (int) (0.5 + GetQHYCCDParam(camhandle, CONTROL_MANULPWM));
  ReleaseCameraLock();
}
  

void CoolerWriteLogEntry(void) {
  if (cooler_log) {
    bool ambient_avail = AmbientTempAvail();
    double ambient = AmbientCurrentDegC();
    time_t now = time(0);

    //RefreshCoolerData();
    fprintf(cooler_log, "%Lu,%.3lf,%d,%.3lf\n",
	    (unsigned long long) now,
	    GetCurrentChipTemp(),
	    (int) (0.5 + GetCurrentCoolerPWM()),
	    (ambient_avail ? ambient : -99.9));
    fflush(cooler_log);
  }
}

void InitCooler(void) {
  // create our thread
  thread_id = pthread_create(&thread_id,
			     nullptr, // thread attr
			     RunCooler, // start_routine
			     nullptr);	// args

}

struct TempPoint {
  time_t timestamp;
  double setpoint;
};

class Regulator {
public:
  Regulator(void) {;}
  ~Regulator(void) {;}
  void Reset(void);
  void Suspend(void) {;}
  void Cycle(void);

private:
  double current_working_setpoint;
  std::list<TempPoint> current_ramp;
  std::list<TempPoint> chip_temp_history;
  
  double power_ratio {44.7/255.0};
  const double gain_p {15.0};
  const double gain_i {1.0};
  const double gain_d {400.0};
  const double max_allowed_slope {2.0/60.0}; // 2 deg/min
  const double max_singlestep_setpoint_change {4.0}; // deg C
  const int seconds_per_ramp_step {16};
  const int slope_number_points {8};
  bool first_time = true;
  time_t last_time;
  const double ambient_offset {3.0}; // deg C

  double integrated_error {0.0};
} regulator;
  

void *RunCooler(void *args) {
  cooler_log = fopen(COOLER_LOGFILE, "w");
  cooler_data.CoolerCurrentPWM=-99;
  AmbientInitialize();
  CoolerWriteLogEntry();
  fprintf(stderr, "RunCooler: mode=%d, PWMcmd=%d, TempSetpoint=%.2lf\n",
	  cooler_data.CoolerModeDesired,
	  cooler_data.CoolerPWMCommand,
	  cooler_data.CoolerTempCommand);

  while(cooler_data.CoolerModeDesired != COOLER_TERMINATE) {
    ControlCooler();
    CoolerWriteLogEntry();
    fflush(cooler_log);
    sleep(cooler_cycle_time);
  }

  cooler_data.CoolerCurrentMode = COOLER_TERMINATED;
  fclose(cooler_log);
  // ...and this will terminate the pthread
  return nullptr;
}

void ControlCooler(void) {
  static CoolerModeRequest prior_request = COOLER_OFF;
  
  bool fetch_status = true;
  fprintf(stderr, "ControlCooler(): starting.\n");

  GetCameraLock();
  RefreshCoolerStatus();
  ReleaseCameraLock();
  
  switch(cooler_data.CoolerModeDesired) {
  case COOLER_TERMINATE:
    fetch_status = false;
    break;
    
  case COOLER_AUTO:
    if (prior_request != COOLER_AUTO) {
      regulator.Reset();
    }
    cooler_data.CoolerCurrentMode = COOLER_REGULATING;
    regulator.Cycle();
    break;
    
  case COOLER_MAN:
    {
      GetCameraLock();
      int ret = SetQHYCCDParam(camhandle, CONTROL_MANULPWM, cooler_data.CoolerPWMCommand);
      ReleaseCameraLock();
      if (ret != QHYCCD_SUCCESS) {
	fprintf(stderr, "SetQHYCCDParam(CONTROL_MANULPWM, x) failed.\n");
	cooler_data.CoolerCurrentMode = COOLER_ERROR;
      } else {
	cooler_data.CoolerCurrentMode = COOLER_MANPWM;
      }
    }
    break;
    
  case COOLER_OFF:
    if (cooler_data.CoolerCurrentMode != COOLER_POWEROFF) {
      GetCameraLock();
      int ret = SetQHYCCDParam(camhandle, CONTROL_MANULPWM, 0);
      ReleaseCameraLock();
      if (ret != QHYCCD_SUCCESS) {
	fprintf(stderr, "SetQHYCCDParam(CONTROL_MANULPWM, 0) failed.\n");
	cooler_data.CoolerCurrentMode = COOLER_ERROR;
      } else {
	cooler_data.CoolerCurrentMode = COOLER_POWEROFF;
      }
    }
    break;
    
  default:
    std::cerr << "ControlCooler(): Invalid desired mode\n";
    cooler_data.CoolerCurrentMode = COOLER_ERROR;
  }
  prior_request = cooler_data.CoolerModeDesired;
  fprintf(stderr, "ControlCooler(): exit.\n");
}

void RefreshCoolerStatus(void) {
  // Assumes already holding the camera lock

  cooler_data.ambient_avail = AmbientTempAvail();
  if (cooler_data.ambient_avail) cooler_data.CoolerCurrentAmbient = AmbientCurrentDegC();
  cooler_data.CoolerCurrentChipTemp = GetQHYCCDParam(camhandle, CONTROL_CURTEMP);
  cooler_data.CoolerCurrentPWM = (int) (0.5 + GetQHYCCDParam(camhandle, CONTROL_CURPWM));

  fprintf(stderr, "Current chip temp = %lf, current cooler PWM = %d, ambient = %.1lf\n",
	  cooler_data.CoolerCurrentChipTemp,
	  cooler_data.CoolerCurrentPWM,
	  cooler_data.CoolerCurrentAmbient);
  
  //        Humidy/Pressure
  {
    double humidity, pressure;
    int ret = GetQHYCCDPressure(camhandle, &pressure);
    if (ret == QHYCCD_SUCCESS) {
      ret = GetQHYCCDHumidity(camhandle, &humidity);
      if (ret == QHYCCD_SUCCESS) {
	fprintf(stderr, "Camera chamber pressure = %.1lf mbar, humidity = %lf\n",
		pressure, humidity);
	cooler_data.CurrentHumidity = humidity;
	cooler_data.CurrentPressure = pressure;
      } else {
	fprintf(stderr, "GetQHYCCDHumidity() failed.\n");
	cooler_data.CoolerCurrentMode = COOLER_ERROR;
      }
    } else {
      fprintf(stderr, "GetQHYCCDPressure() failed.\n");
      cooler_data.CoolerCurrentMode = COOLER_ERROR;
    }
  }
}

static double integrated_error = 0.0;

void Regulator::Reset(void) {
  integrated_error = 0.0;
  first_time = true;
  last_time = 0;
  current_ramp.clear();
  chip_temp_history.clear();
}
  
  
void Regulator::Cycle(void) {
  const time_t now = time(nullptr);
  static double prior_target_temp = 99.9;

  // This is currently disabled. Nothing is ever pushed onto measurements.
  if (measurements.size() > 20) {
    FittingResults fitter;
    GetFittingParams(fitter);
    //ambient_offset = fitter.ambient_offset;
    //power_ratio = fitter.power_ratio;
    fprintf(stderr, "Fitter: amb_offset = %0.1lf, power_ratio = %lf, slope_ratio=%lf [%ld]\n",
	    fitter.ambient_offset, fitter.power_ratio, fitter.slope_ratio, measurements.size());
  }

  if (prior_target_temp != cooler_data.CoolerTempCommand) {
    prior_target_temp = cooler_data.CoolerTempCommand;
    current_ramp.clear();
  }

  bool in_ramp = current_ramp.size() > 0;
  const double current_err = fabs(cooler_data.CoolerTempCommand -
				  cooler_data.CoolerCurrentChipTemp);
  const bool ramp_is_negative = (cooler_data.CoolerTempCommand < cooler_data.CoolerCurrentChipTemp);
  const int ramp_multiplier = (ramp_is_negative ? -1 : 1);

  // Create a ramp if the ordered temp is very different from the
  // actual current temp
  if ((not in_ramp) and current_err  > max_singlestep_setpoint_change) {
    // need to create a ramp
    double target_delta_t = current_err/max_allowed_slope; // total time for ramp
    int ramp_points = int(target_delta_t/seconds_per_ramp_step); // 16-seconds per ramp point
    double temp_increment = current_err/(ramp_points+1);

    fprintf(stderr, "New cooler ramp with %d points:\n", ramp_points);
    for (int i=0; i<ramp_points; i++) {
      TempPoint p;
      p.timestamp = now + (i*seconds_per_ramp_step);
      p.setpoint = cooler_data.CoolerCurrentChipTemp + (i+1)*ramp_multiplier*temp_increment;
      fprintf(stderr, "    time = %Lu, temp = %.2lf\n",
	      (unsigned long long) p.timestamp, p.setpoint);

      current_ramp.push_back(p);
    }
    in_ramp = true;
  }

  if (in_ramp) {
    TempPoint p = current_ramp.front();
    if (now >= p.timestamp) {
      current_working_setpoint = p.setpoint;
      current_ramp.pop_front();
    }
  } else {
    current_working_setpoint = cooler_data.CoolerTempCommand;
  }

  fprintf(stderr, "Current chip temp = %.2lf, current target = %.2lf\n",
	  cooler_data.CoolerCurrentChipTemp,
	  current_working_setpoint);

  static double last_chip_temp = 0.0; // possibly bad initial value!
  int target_power = (cooler_data.CoolerCurrentAmbient -ambient_offset - current_working_setpoint)/power_ratio + 0.5;
  fprintf(stderr, "target_power = %d\n", target_power);

  // calculate slope
  chip_temp_history.push_back(TempPoint{now,cooler_data.CoolerCurrentChipTemp});
  if (chip_temp_history.size() > slope_number_points) {
    chip_temp_history.pop_front();
  }
  const int num_slope_points = chip_temp_history.size();
  // slope will be zero if don't have enough points to measure slope
  double slope = 0.0;
  if (num_slope_points > 1) {
    TempPoint p1 = chip_temp_history.front();
    TempPoint p2 = chip_temp_history.back();
    long delta_t = p2.timestamp - p1.timestamp;
    if (delta_t > 0) {
      // positive means chip is warming
      slope = (p2.setpoint - p1.setpoint)/delta_t;
    }
  }

  double temp_err = current_working_setpoint - cooler_data.CoolerCurrentChipTemp;
  double trial_integrated_error = integrated_error;
  
  // calculated integrated error
  if (first_time) {
    first_time = false;
    trial_integrated_error = 0.0;
  } else {
    int delta_time = now - last_time;
    trial_integrated_error += (delta_time*temp_err);
  }
  last_time = now;

  // calculate new power level
#if 1
  int command = target_power + int(-(temp_err*gain_p +
				     trial_integrated_error*gain_i +
				     (-slope)*gain_d));
#else
  int command = int(-(temp_err*gain_p +
		      trial_integrated_error*gain_i +
		      (-slope)*gain_d));
#endif
  if (command > 255) {
    command = 255;
    //Reset();
  } else if (command < 0) {
    command = 0;
    //Reset();
  } else {
    integrated_error = trial_integrated_error;
  }

  fprintf(stderr, "%Lu, %.2lf, %.2lf, %.2lf, %d, %lf, %d, (%lf, %lf, %lf)\n",
	  (unsigned long long) now, cooler_data.CoolerCurrentChipTemp,
	  cooler_data.CoolerCurrentAmbient,
	  temp_err,
	  command,
	  slope,
	  target_power, temp_err*gain_p, integrated_error*gain_i, -slope*gain_d);
	  
  fprintf(stderr, "new command = %d\n", command);

#if 0
  static int cycle_count = 0;
  if (++cycle_count > 3) exit(0);
#endif

  // Now add this point to the fitter
  if (not in_ramp) {
    FitterAcceptPoint(cooler_data.CoolerCurrentPWM,      // power
		      cooler_data.CoolerCurrentChipTemp, // ccd temp
		      cooler_data.CoolerCurrentAmbient,  // ambient
		      slope);
  }
  
  //  if (command != cooler_data.CoolerCurrentPWM) {
  {
    cooler_data.CoolerCurrentPWM = command;
    GetCameraLock();
    int ret = SetQHYCCDParam(camhandle, CONTROL_MANULPWM, command);
    if (ret != QHYCCD_SUCCESS) {
      fprintf(stderr, "SetQHYCCDParam(CONTRO_MANULPWM, regulator) failed.\n");
    }
    ReleaseCameraLock();
  }

}
