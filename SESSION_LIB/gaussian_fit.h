// This may look like C code, but it is really -*- C++ -*-
/*  gaussian_fit.h -- Gaussian-matching
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
#include <list>
/****************************************************************/
/*      class RunData						*/
/****************************************************************/

// Number of different measurements of focus vs focuser position

struct GRunPoint {
  double pixel_x;		// distance from center of star
  double pixel_y;
  double intensity;		// pixel brightness
  double err;			// measured gaussian vs modeled
  double t[6];			// partial differentials
};

class GRunData {
public:
  std::list<GRunPoint *> all_points;
  int N;			    // # datapoints

  void reset(void) { N = 0; all_points.clear(); }
  void add(double x, double y, double value) {
    GRunPoint *p = new GRunPoint;
    p->pixel_x = x;
    p->pixel_y = y;
    p->intensity = value;
    all_points.push_back(p);
    N++;
  }

  void print(FILE *fp);
};

/****************************************************************/
/*      class Hyperbola						*/
/****************************************************************/

// Indices into arrays
const int GAUSSIAN_A    = 0;	// Scaling
const int GAUSSIAN_S    = 1;	// Sigma (shape)
const int GAUSSIAN_B    = 2;	// zero offset
//const int GAUSSIAN_R0   = 3;    // flat-top width
const int GAUSSIAN_X0   = 3;    // center x
const int GAUSSIAN_Y0   = 4;    // center y

class Gaussian {
public:
  double state_var[6];
  double mel;

  Gaussian(void);		// provides default init state
  void reset(void);		// resets state back to init state
  void reset(Gaussian *p);	// sets state to same as "p"
  int NoSolution(void) { return !converged; }
private:
  int converged;

  friend int nlls_gaussian(Gaussian *fs, GRunData *run_data);
};

// returns -1 if would not converge
// returns 0 if converged okay
int nlls_gaussian(Gaussian *fs, GRunData *run_data);


