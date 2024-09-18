/* This may look like C code, but it is really -*-c++-*- */
/*  running_focus.h -- operations to manage focus using measured bluring
 *  of star images 
 *
 *  Copyright (C) 2020 Mark J. Munkacsy

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
#ifndef _RUNNING_FOCUS_INT_H
#define _RUNNING_FOCUS_INT_H

//****************************************************************
//        class FocusModel (superclass)
//****************************************************************
class FocusModel {
public:
  FocusModel(FILE *log, JULIAN meas_start_time);
  virtual ~FocusModel(void);

  // return true means something good happened
  virtual bool Recalculate(MeasurementList *ml) = 0;
  virtual double ValueAtTime(JULIAN when) = 0;
  virtual bool ModelIsValid(void) = 0;
  virtual int NumberFittingParameters(void) = 0;
  virtual double PredictBlur(double delta_ticks) = 0;
  virtual const char *OneLineSummary(void) = 0;
  virtual void Promote(void);
  int SeqNo(void) { return seq_no; }
protected:
  int seq_no;
  FILE *logfile;

public:
  JULIAN ref_time; // this is the "t0" time. Nothing before this accepted
  JULIAN end_time; // nothing beyond this will be processed
};

//****************************************************************
//        class ConstantFocusModel
//   Subclass of FocusModel
//****************************************************************

class ConstantFocusModel : public FocusModel {
 public:
  ConstantFocusModel(FILE *log, JULIAN meas_start_time);
  ~ConstantFocusModel(void);
  double focus_center;

  bool Recalculate(MeasurementList *ml) { return true;}
  double ValueAtTime(JULIAN when) { return focus_center; }
  double PredictBlur(double delta_ticks);
  bool ModelIsValid(void) { return true; }
  const char *OneLineSummary(void);
  int NumberFittingParameters(void) { return 1; }
};
  
//****************************************************************
//        class HypFocusModel
//   Subclass of FocusModel
//****************************************************************
class HypFocusModel : public FocusModel {
public:
  HypFocusModel(FILE *log, JULIAN meas_start_time);
  ~HypFocusModel(void) {;}

  // returns false if would not converge
  // returns true if converged okay
  bool Recalculate(MeasurementList *od);
  double ValueAtTime(JULIAN when);
  double PredictBlur(double delta_ticks);
  bool ModelIsValid(void) { return converged; }
  const char *OneLineSummary(void);
  int NumberFittingParameters(void) { return 3; }

private:
  JULIAN t0;
  double focus_center;		// ticks at time t0
  double focus_rate;
  double min_gaussian;
  int    order;
  bool   converged;
  double mel;			// rms value of residuals

  void Computet1t2t3(MeasurementList *run_data);
  bool PerformNLLS(MeasurementList *run_data);
};

#endif
