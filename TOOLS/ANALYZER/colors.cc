/* This may look like C code, but it is really -*-c++-*- */
/*  trans_coef.h -- Read transformation coefficient file
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

#include "colors.h"
#include "trans_coef.h"
#include "assert.h"
#include <HGSC.h>

inline bool is_valid(double v) {
  return ( v < 99.9 );
}

inline bool is_invalid(double v) {
  return !is_valid(v);
}

//****************************************************************
//        Colors() constructor
//****************************************************************
Colors::Colors(void) {
  ref_comp = 0;
  raw_colors_valid = false;
  for (int c = 0; c < NUM_FILTERS; c++) {
    raw_measurements[c] = INVALID_MEASUREMENT;
    tr_measurements[c] = INVALID_MEASUREMENT;
  }
  for (int c = 0; c < NUM_COLORS; c++) {
    raw_colors[c] = INVALID_MEASUREMENT;
    tr_colors[c] = INVALID_MEASUREMENT;
  }
}

Colors::Colors(HGSC *catalog_entry) : Colors() {
  MultiColorData *m = &(catalog_entry->multicolor_data);
  if (m->IsAvailable(PHOT_B)) AddColor(i_B, m->Get(PHOT_B));
  if (m->IsAvailable(PHOT_V)) AddColor(i_V, m->Get(PHOT_V));
  if (m->IsAvailable(PHOT_R)) AddColor(i_R, m->Get(PHOT_R));
  if (m->IsAvailable(PHOT_I)) AddColor(i_I, m->Get(PHOT_I));
  for (int c = 0; c < NUM_FILTERS; c++) {
    tr_measurements[c] = raw_measurements[c];
  }
  ComputeRawColors();
  ComputeTransformedColors();
}

//****************************************************************
//        AddColor()
//****************************************************************
void Colors::AddColor(int color_index, double magnitude) {
  raw_measurements[color_index] = magnitude;
}

//****************************************************************
//        AddComp()
//****************************************************************
void Colors::AddComp(Colors *comp_colors) {
  assert(ref_comp == 0);
  ref_comp = comp_colors;
}

struct color_info {
  int color1;
  int color2;
  int color_measure;
  int coef_name;
} a_color_info[] = {
  { i_B, i_V, COLOR_B_V, TC_Tbv },
  { i_V, i_R, COLOR_V_R, TC_Tvr },
  { i_R, i_I, COLOR_R_I, TC_Tri },
  { i_V, i_I, COLOR_V_I, TC_Tvi },
};

struct transform_info {
  int t_color; // the color being transformed (e.g., V)
  int x_color; // the color used for transforming (e.g., B-V)
  int coef_name; // the coefficient for transformation
} a_transform_info[] = {
  // The order of these matters. The first one found that can be used
  // is the one that actually will be used, so put preferred items
  // first. 
  { i_B, COLOR_B_V, TC_Tb_bv },
  { i_V, COLOR_V_R, TC_Tv_vr },
  { i_R, COLOR_V_R, TC_Tr_vr },
  { i_R, COLOR_R_I, TC_Tr_ri },
  { i_I, COLOR_R_I, TC_Ti_ri },
  { i_V, COLOR_V_I, TC_Ti_vi },
  { i_R, COLOR_V_I, TC_Tr_vi },
  { i_I, COLOR_V_I, TC_Ti_vi },
  { i_V, COLOR_B_V, TC_Tv_bv },
};


#if 0 // not yet implemented
//****************************************************************
//        GetCorrection()
// This correction gets added to the uncorrected mag to get the
// final magnitude
//****************************************************************
double
Colors::GetCorrection(int filter_being_corrected, // e.g., i_R
		      TransformationCoefficients *t) {
  const int num_color_xform = (sizeof(a_color_info)/sizeof(a_color_info[0]));
  const int num_xform_info = (sizeof(a_transform_info)/sizeof(a_transform_info[0]));
  assert(num_color_xform == NUM_COLORS);

  // now search for a transform valid for this filter
  for (int c = 0; c < num_xform_info; c++) {
    transform_info *xform = &(a_transform_info[c]);
    const double coefficient = t->Coefficient(xform->coef_name);
    if (xform->t_color != filter_being_corrected ||
	is_invalid(coefficient)) continue;
    if (is_valid(tr_colors[xform->x_color])) {
      return coefficient * 

	// We have good numbers and we're ready to proceed
	tr_measurements[i] = coefficient * transformed_delta_colors[xform->x_color] +
	  raw_measurements[i];
	break;
      }
    }
  }
  
}
#endif

//****************************************************************
//        Transform()
//****************************************************************
void Colors::Transform(const TransformationCoefficients *t) {
  assert(ref_comp);
  double transformed_delta_colors[NUM_COLORS];
  
  // Make sure both this star and the comp star have raw colors
  // computed. 
  ComputeRawColors();
  ref_comp->ComputeRawColors();

  const int num_color_xform = (sizeof(a_color_info)/sizeof(a_color_info[0]));
  const int num_xform_info = (sizeof(a_transform_info)/sizeof(a_transform_info[0]));
  assert(num_color_xform == NUM_COLORS);
  for (int i = 0; i < NUM_COLORS; i++) {
    struct color_info *x = &(a_color_info[i]);
    // If both the comp star and this star have a valid measurement
    // for this color, then we can compute an instrumental delta
    // color. 
    if (is_valid(raw_colors[x->color_measure]) &&
	is_valid(ref_comp->raw_colors[x->color_measure])) {
      const double delta_color = raw_colors[x->color_measure] -
	ref_comp->raw_colors[x->color_measure];
      // now transform the colors
      double transform = t->Coefficient(x->coef_name);
      transformed_delta_colors[x->color_measure] = transform * delta_color;
    } else {
      transformed_delta_colors[x->color_measure] = INVALID_MEASUREMENT;
    }
  }

  for (int i=0; i<NUM_FILTERS; i++) {
    tr_measurements[i] = INVALID_MEASUREMENT; // override later on
    if (is_valid(raw_measurements[i]) &&
	is_valid(ref_comp->raw_measurements[i])) {
      // now search for a transform valid for this measurement
      for (int c = 0; c < num_xform_info; c++) {
	transform_info *xform = &(a_transform_info[c]);
	const double coefficient = t->Coefficient(xform->coef_name);
	if (xform->t_color != i ||
	    is_invalid(transformed_delta_colors[xform->x_color]) ||
	    is_invalid(coefficient)) continue;
	// We have good numbers and we're ready to proceed
	tr_measurements[i] = coefficient * transformed_delta_colors[xform->x_color] +
	  raw_measurements[i];
	break;
      }
    }
  }
  // ComputeTransformedColors();
}

void
Colors::GetMag(int color_index, double *magnitude, bool *is_transformed) const {
  if (is_valid(tr_measurements[color_index])) {
    *is_transformed = true;
    *magnitude = tr_measurements[color_index];
  } else {
    *is_transformed = false;
    *magnitude = raw_measurements[color_index];
  }
}
	   
void
Colors::GetColor(int color_index, double *magnitude, bool *is_transformed) const {
  if (is_valid(tr_colors[color_index])) {
    *is_transformed = true;
    *magnitude = tr_colors[color_index];
  } else {
    *is_transformed = false;
    *magnitude = raw_colors[color_index];
  }
}
	   
void
Colors::ComputeRawColors(void) {
  if (!raw_colors_valid) {
    raw_colors_valid = true;
    
    for (int i = 0; i < NUM_COLORS; i++) {
      struct color_info *x = &(a_color_info[i]);
      // first calculate raw (instrumental) colors
      if (is_valid(raw_measurements[x->color1]) &&
	  is_valid(raw_measurements[x->color2])) {
	raw_colors[x->color_measure] = raw_measurements[x->color1] - raw_measurements[x->color2];
      } else {
	raw_colors[x->color_measure] = INVALID_MEASUREMENT;
      }
    }
  }
}

void
Colors::ComputeTransformedColors(void) {
  for (int i=0; i < NUM_COLORS; i++) {
    struct color_info *x = &(a_color_info[i]);
    double mag1, mag2;
    bool is_transformed; // will be ignored (and overwritten)

    GetColor(x->color1, &mag1, &is_transformed);
    GetColor(x->color2, &mag2, &is_transformed);

    tr_colors[x->color_measure] = mag1 - mag2;
  }
}
