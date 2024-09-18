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
/****************************************************************/

// Indices into arrays
const int HYPER_A    = 0;	// Blur at perfect focus
const int HYPER_R    = 1;	// Value of x at perfect focus

class Hyperbola {
public:
  double state_var[2];
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
  int NoSolution(void) { return !converged; }
private:
  int converged;
};

// returns -1 if would not converge
// returns 0 if converged okay
int nlls_hyperbola(Hyperbola *fs, RunData *run_data);


