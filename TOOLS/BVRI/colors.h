/* This may look like C code, but it is really -*-c++-*- */
/*  colors.h -- manage BVRI colors
 *
 *  Copyright (C) 2016 Mark J. Munkacsy
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

#ifndef _COLORS_H
#define _COLORS_H

class TransformationCoefficients; // forward declaration
class HGSC;

#define INVALID_MEASUREMENT 999.9

// These colors need to stay in this order so that the algorithms in
// colors.cc will work. Must match declarations in other bvri*.cc
// files, as well.

#define i_V 1
#define i_R 2
#define i_I 3
#define i_B 0

#define NUM_FILTERS 4 // things like B, V, R, I

#define NUM_COLORS 4 // things like (b-v, r-i, ...)

#define COLOR_B_V 0
#define COLOR_V_R 1
#define COLOR_R_I 2
#define COLOR_V_I 3

//****************************************************************
//        Class Colors
// Required sequence of operations:
//    1. AddColor() multiple times to add each color datapoint
//    2. AddComp() to provide the comp star's data
//    3. Transform()
//    4. Fetch results using GetMag(), GetColor()
// No protections are provided. Doing things out of order will create
// a nasty mess.
//****************************************************************

class Colors {
public:
  Colors(void);
  Colors(HGSC *catalog_entry);
  ~Colors(void) {;}

  void AddColor(int color_index, double magnitude);
  void AddComp(Colors *comp_colors);
  void Transform(const TransformationCoefficients *t); 
  // GetMag() is called with one of i_V, i_R, ...
  void GetMag(int color_index, double *magnitude, bool *is_transformed) const;
  // GetColor() is called with one of COLOR_B_V, COLOR_V_R, ...
  void GetColor(int color_index, double *color, bool *is_transformed) const;

#if 0 // not yet implemented
  // This correction gets added to the uncorrected mag to get the
  // final magnitude 
  double GetCorrection(int filter_being_corrected, // e.g., i_R
		       TransformationCoefficients *t);
#endif
		       
  static inline bool is_valid(double v) {
    return ( v < 99.9 && v > -99.9);
  }

  static inline bool is_invalid(double v) {
    return !is_valid(v);
  }
private:
  void ComputeRawColors(void);
  void ComputeTransformedColors(void);
  
  Colors *ref_comp;

  bool raw_colors_valid;
  
  double raw_measurements[NUM_FILTERS];
  double tr_measurements[NUM_FILTERS];
  double raw_colors[NUM_COLORS];
  double tr_colors[NUM_COLORS];
};

#endif
