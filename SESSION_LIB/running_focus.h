/* This may look like C code, but it is really -*-c++-*- */
/*  running_focus.h -- operations to manage focus using measured bluring
 *  of star images 
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
#ifndef _RUNNING_FOCUS_H
#define _RUNNING_FOCUS_H

#include "Image.h"
#include "julian.h"
#include <list>

//****************************************************************
//        class FocusPoint
// (This class is private and not to be used externally.)
//****************************************************************

struct FocusPoint {
  double focus_position;	// ticks
  JULIAN time_tag;		// measurement time
  double time_offset;		// backwards in time from analysis_t0
  double weight;
  bool exclude;			// true => ignore this point
  double star_size;		// gaussian
  double err;			// (measured - model)
  double t[6];			// partial differentials
};

class MeasurementList : public std::list<FocusPoint> {
public:
  double min_gaussian;
};

typedef MeasurementList::iterator MeasurementIterator;

//typedef std::list<FocusPoint> MeasurementList;
//typedef std::list<FocusPoint>::iterator MeasurementIterator;

class CompositeModel;

//****************************************************************
//        class RunningFocus
//  (This is the external-facing class that others will use.)
//****************************************************************
class RunningFocus {
 public:
  RunningFocus(const char *logfilename);
  ~RunningFocus(void);

  void AddImage(Image *image);
  void AddImage(const char *image_filename);
  void AddPoint(double gaussian, double focuser, JULIAN when);

  void UpdateFocus(void); // update right now

  void PerformFocusDither(void);
  void SetInitialImagesToIgnore(int num_to_ignore);
  void Restart(void);
  FILE *Logfile(void) { return rf_log_file; }
  void BatchSolver(void);

 private:
  // state variables
  bool state_valid;
  bool first_model;
  double initial_focus;		// only valid until a ref_model exists
  
  time_t time_origin;

  // The "reference" solution follows:
  CompositeModel *ref_model;
#if 1
  //#ifdef RUNNING_FOCUS2
  std::list<CompositeModel *> fitting_models; // the experimental models
#else
  CompositeModel *fitting_model;
#endif

  int measurements_still_to_ignore;
  int dither_counter;		// -1 means done or don't do, 0 means not started
  double DoDither(void);
  void ClearFittingModels(void);

  // all measurements
  MeasurementList measurements;

  const char *rf_log_file_name;
  FILE *rf_log_file;

  std::list<double> last_5_blurs;

  int next_model_number; //DELETE

  const char *current_time_string(void);
};

#endif
