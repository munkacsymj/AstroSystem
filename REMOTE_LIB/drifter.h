/* This may look like C code, but it is really -*-c++-*- */
/*  drifter.h -- Implements image drift management
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
#ifndef _DRIFTER_H
#define _DRIFTER_H

#include <Image.h>
#include <list>
#include <stdio.h>
#include "julian.h"
#include "dec_ra.h"

struct AxisMeasurement {
  JULIAN when;
  double delta_t; // offset from latest observation, usually negative
  double measured_posit; // radians, offset from orig_position
  double cum_measured_posit; // radians, offset from orig_time
  double weight;
};

class AxisDrifter {
 public:
  AxisDrifter(FILE *logfile, const char *name_of_axis);
  ~AxisDrifter(void);

  inline void SetScale(double scale) { dscale = scale; }
  inline void SetAxis(bool AxisIsDec) { axis_is_dec = AxisIsDec; }
  inline void SetNorthUp(bool NorthUp) { north_up = NorthUp; }
  void AcceptCenter(double measurement, JULIAN when); // extracted from image
  void ExposureStart(double duration, double update_period); // duration in seconds
  void ExposureUpdate(double time_to_next_update);

  void print(FILE *fp);

 private:
  void RecalculateDriftRate(void);
  
  double orig_position; // radians in this axis
  JULIAN orig_time;

  JULIAN reference_time; // reference for the next three params
  double drift_rate; // current estimate, arcsec/second
  double drift_intercept; // location back when delta_t == 0
  double drift_accel;
  
  bool initialized;
  double cum_guidance_arcsec; // cum sum of all guidance commands

  double dscale; // cos(dec) for the RA AxisDrifter
  bool north_up; // same as Image.NorthIsUp()
  bool axis_is_dec;

  std::list<AxisMeasurement *>measurements;

  const char *axis_name;
  FILE *log;
};

class Drifter {
 public:
  Drifter(FILE *logfile);
  ~Drifter(void);

  void SetNorthUp(bool NorthUp);
  void AcceptCenter(DEC_RA center, JULIAN when);
  void AcceptImage(Image *i);
  void AcceptImage(const char *image_filename);

  void ExposureStart(double duration); // blocks for short time (issues guide)
  void ExposureGuide(void);  // blocks for duration of exposure

  void print(FILE *fp);

 private:
  AxisDrifter *dec_drifter;
  AxisDrifter *ra_drifter;
  time_t exposure_start_time;
  double exposure_duration;

  FILE *log;
};

#endif
