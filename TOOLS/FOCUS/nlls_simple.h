/*  nlls_simple.h -- Non-linear least squares estimator of star image
 *  point spread function to calculate FWHM
 *
 *  Copyright (C) 2007 Mark J. Munkacsy
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
const int FS_X0    = 0;		// x0 (star center, X)
const int FS_Y0    = 1;		// y0 (star center, Y)
const int FS_C     = 2;		// C = total flux
const int FS_B     = 3;		// B = background
const int FS_R     = 4;		// R = blur (FWHM)
const int FS_Beta  = 5;		// Beta = gaussian tail

class focus_state {
public:
  double state_var[8];
  double mel;

  focus_state(void);		// provides default init state

  double &blur(void)             { return state_var[FS_R]; }
  double &R(void)                { return state_var[FS_R]; }
  double &Beta(void)             { return state_var[FS_Beta]; }
};

// returns -1 if would not converge
// returns 0 if converged okay
int nlls(Image *primary_image, focus_state *fs);
int nlls1(Image *primary_image, focus_state *fs);

