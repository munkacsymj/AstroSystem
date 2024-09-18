/*  hyperbola.h -- (Current) Hyperbola-matching to predict point of best focus
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
#ifndef _HYPERBOLA_H
#define _HYPERBOLA_H

#include <stdio.h>
/****************************************************************/
/*      class RunData						*/
/****************************************************************/

// Number of different measurements of focus vs focuser position
const int MAXPOINTS = 200;

class RunData {
public:
  int N;			    // # datapoints
  double focus_position[MAXPOINTS]; // ticks
  double star_size[MAXPOINTS];	    // blur
  double err[MAXPOINTS];	    // measured blur vs modeled blur
  double t[4][MAXPOINTS];	    // partial differentials

  void reset(void) { N = 0; }
  void add(double position, double size);
  void print(FILE *fp);
};

/****************************************************************/
/*      class Hyperbola						*/
/*    Hyperbola can be run in either of two modes. In one mode  */
/*    there are 3 parameters being fit (minimum, focuser ticks, */
/*    and curve slope); in the other mode curve slope is taken  */
/*    as a given. The user chooses with a call to SetC().       */
/*    If the argument to SetC is negative, then fitting to the  */
/*    curve slope will be performed (and GetC() becomes         */
/*    interesting). If the argument to SetC is positive, then   */
/*    that value is taken as fixed.                             */
/****************************************************************/

// Indices into arrays
const int HYPER_A    = 0;	// Blur at perfect focus
const int HYPER_R    = 1;	// Value of x at perfect focus
const int HYPER_C    = 2;	// Slope

class Hyperbola {
public:
  int order{2};
  double state_var[3];
  double mel;

  // the value of C is a measure of focal length/pixel size. The value
  // is the number of focus encoder ticks that equate to a change of
  // 1.0 in blur value. For the ST-9, no barlow, at 100" FL, the value
  // is 36. For the STI120 with a barlow, the value is 7.4.
  Hyperbola(void);		// provides default init state
  Hyperbola(double best_guess);	// provides best initial guess
  void SetC(double ticks_per_blur); // definition of C is above.
  void reset(void);		// resets state back to init state
  void reset(double best_guess);
  void reset(Hyperbola *p);	// sets state to same as "p"

  // The following methods are only valid after the model is run via Solve().
  double GetModel(double ticks);
  int NoSolution(void) { return !converged; }
  double GetTicks(void) { return converged_ticks; }
  double GetC(void) { return converged_slope; }
  double GetRUncertainty(RunData *rd);

// returns -1 if would not converge
// returns 0 if converged okay
  int Solve(RunData *run_data);

private:
  double C{64.0}; // this initializer is probably dangerous. User
		  // should always use SetC()
  bool converged;
  double converged_ticks;
  double converged_slope;
  int slope_search;

  void Computet1t2t3(RunData *od);
};




#endif
