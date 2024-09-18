/*  gaussian_fit.h -- Gaussian-matching
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
const int MAXPOINTS = 20000;

class GRunData {
public:
  int N;			    // # datapoints
  double radius_pixel[MAXPOINTS];   // ticks
  double intensity[MAXPOINTS];	    // blur
  double err[MAXPOINTS];	    // measured blur vs modeled blur
  double t[4][MAXPOINTS];	    // partial differentials

  void reset(void) { N = 0; }
  void add(double radius, double value);
  void print(FILE *fp);
};

/****************************************************************/
/*      class Hyperbola						*/
/****************************************************************/

// Indices into arrays
const int GAUSSIAN_A    = 0;	// Scaling
const int GAUSSIAN_S    = 1;	// Sigma (shape)

class Gaussian {
public:
  double state_var[2];
  double mel;

  Gaussian(void);		// provides default init state
  void reset(void);		// resets state back to init state
  void reset(Gaussian *p);	// sets state to same as "p"
  int NoSolution(void) { return !converged; }
private:
  int converged;
};

// returns -1 if would not converge
// returns 0 if converged okay
int nlls_gaussian(Gaussian *fs, GRunData *run_data);


