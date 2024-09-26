/*  focus_simulator.cc -- Mimic a camera and focuser for testing
 *
 *  Copyright (C) 2018 Mark J. Munkacsy
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
#include <string.h>
#include "running_focus.h"
#include <Image.h>
#include <unistd.h> 		// for getopt()
#include <stdlib.h>		// for atof()
#include <gendefs.h>
#include "focus_simulator.h"
#include <time.h>		// time_t
#include <random>

#define TIME_TICK 10 // seconds

struct FocusSim {
public:
  double current_rate; // ticks/second
  double current_focus; // ticks (truth)
  double focuser_setting; // ticks (driven by algorithm)
  double current_seeing; 
  time_t reference_time;
  long   now;        // seconds since reference_time
  long elapsed_time; // seconds since reference_time (extrapolation)
} focus_context;

struct FocusPlanPoint {
public:
  double target_rate;
  double seeing;
  long delta_time_tag;
} plan[] = {
  { -0.002, 1.25, 0 }, 		// first point must be at time 0
  { -0.0015, 1.20, 20*60 },
  { -0.001, 1.19, 35*60 },
  { 0.0, 1.22, 40*60 },
  { 0.0005, 1.23, 75*60 },
  { 0.002, 1.20, 150*60 },
  { 0.0007, 1.16, 200*60 },
  { 0.0,  1.20, 60*60*60 }};		// crazy last point we'll never reach

static const int num_plan_points = sizeof(plan)/sizeof(plan[0]);

struct TruthPoint {
  double true_rate;
  double true_focus;
  long elapsed_time;
};

std::list<TruthPoint> focus_truth_data;

void FocusReSync(void) {
  double acceleration = 8.0E-6; // ticks/sec/sec

  // have to get from "elapsed_time" to "update_interval"
  while (focus_context.now > focus_context.elapsed_time) {
    focus_context.elapsed_time += TIME_TICK;
    double delta_t = TIME_TICK;
    
    if (focus_context.elapsed_time > focus_context.now) {
      delta_t = (focus_context.now - (focus_context.elapsed_time-TIME_TICK));
      focus_context.elapsed_time = focus_context.now;
    }

    // find current target rate
    double target_rate = -99.9;
    for (int i=0; i<num_plan_points; i++) {
      if (focus_context.elapsed_time >= plan[i].delta_time_tag &&
	  focus_context.elapsed_time < plan[i+1].delta_time_tag) {
	target_rate = plan[i].target_rate;
	focus_context.current_seeing = plan[i].seeing;
	break;
      }
    }

    if (target_rate < -9.9) {
      fprintf(stderr, "focus_simulator: bad plan[] array.\n");
      exit(-2);
    }

    focus_context.current_focus += delta_t * focus_context.current_rate;
    // make rate adjustment
    if (target_rate != focus_context.current_rate) {
      if (target_rate < focus_context.current_rate) {
	focus_context.current_rate += delta_t * (-acceleration);
	if (focus_context.current_rate < target_rate) {
	  focus_context.current_rate = target_rate;
	}
      } else {
	focus_context.current_rate += delta_t * acceleration;
	if (focus_context.current_rate > target_rate) {
	  focus_context.current_rate = target_rate;
	}
      }
    }
  }
  fprintf(stderr, "FocusReSync(): t=%ld, C=%.2lf, R=%lf\n",
	  focus_context.now, focus_context.current_focus, focus_context.current_rate);
}

void InitializeSimulator(const char *sim_logfile) {
  focus_context.reference_time = time(0);
  focus_context.elapsed_time = 0;
  focus_context.now = 0;
  focus_context.current_focus = 850;
  focus_context.current_rate = -0.03;
  focus_context.focuser_setting = 835;
  focus_context.current_seeing = 1.2;
}

void ChangeSimulatorFocus(long adjustment_ticks) {
  focus_context.focuser_setting += adjustment_ticks;
  fprintf(stderr, "simulator: changing focuser by %ld to %lf\n",
	  adjustment_ticks, focus_context.focuser_setting);
}

long SimulatorGetFocuser(void) {
  return (long) (focus_context.focuser_setting + 0.5);
}

double GetSimImageGaussian(void) {
  FocusReSync();

  const double a = focus_context.current_seeing;
  const double m = 0.0134;
  const double c = focus_context.current_focus;
  const double x = focus_context.focuser_setting;

  const double y = a*sqrt(1 + m*m*(x - c)*(x - c)/(a*a));

  static const double mean = 0.0;
  static const double stddev = 0.05;
  static std::default_random_engine generator;
  static std::normal_distribution<double> dist(mean, stddev);

  double err_term = dist(generator);

  return y+err_term;
}
  
void SetSimulatorTime(double time_offset_seconds) {
  focus_context.now = time_offset_seconds;
}

double GetSimulatorTime(void) {
  return focus_context.now;
}
