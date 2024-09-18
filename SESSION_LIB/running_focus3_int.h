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
#ifndef _RUNNING_FOCUS2_INT_H
#define _RUNNING_FOCUS2_INT_H

//****************************************************************
//        class HypFocusModel
//****************************************************************
struct FocusModelState {
  double C;
  double R;
  double A;
  double AR;
  double t0;
};

struct Measurement;

class HypFocusModel {
public:
  HypFocusModel(void);
  ~HypFocusModel(void) {}

  int NumFittingParams(void);
  double AValue(double offset_t) const;
  void SetConstrained(bool is_constrained) { constrained = is_constrained; }
  FocusModelState GetStateVector(double offset_t) const;
  void SetInitialConditions(const FocusModelState &init);
  double PredictBlur(double offset_t, double ticks);
  double BestFocus(double offset_t) const;
  void PerformNLLS(void);
  double GetSumSqResiduals(void);
  HypFocusModel *DeepCopy(void);
  void SetOffsets(double offset_start, double offset_end);
  int NumPointsInSubset(void);
  
private:
  FocusModelState int_state;
  bool constrained; 		// true if C and A are continuous across segment boundaries
  std::list<Measurement *> subset; // subset of measurements applicable to this model
  void RefreshSubset(void);
  double offset_start_;
  double offset_end_;

  friend CompositeModel;
};



#endif
