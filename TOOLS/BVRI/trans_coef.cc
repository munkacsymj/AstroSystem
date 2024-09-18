/* This may look like C code, but it is really -*-c++-*- */
/*  trans_coef.cc -- Read transformation coefficient file
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

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "trans_coef.h"

#define NUM_PREDEFINED 17
int TC_predef_int[NUM_PREDEFINED] = {
  TC_Tr_vi,
  TC_Ti_vi,
  TC_Tv_vi,
  TC_Tvr,
  TC_Tr_ri,
  TC_Tv_vr,
  TC_Ti_ri,
  TC_Tbv,
  TC_Tbr,
  TC_Tbi,
  TC_Tb_br,
  TC_Tb_bi,
  TC_Tb_bv,
  TC_Tr_vr,
  TC_Tv_bv,
  TC_Tri,
  TC_Tvi};
const char *TC_predef_char[NUM_PREDEFINED] = {
  "Tr_vi",
  "Ti_vi",
  "Tv_vi",
  "Tvr",
  "Tr_ri",
  "Tv_vr",
  "Ti_ri",
  "Tbv",
  "Tbr",
  "Tbi",
  "Tb_br",
  "Tb_bi",
  "Tb_bv",
  "Tr_vr",
  "Tv_bv",
  "Tri",
  "Tvi"};

int lookup_param_name(const char *name) {
  for (int i=0; i<NUM_PREDEFINED; i++) {
    if (strcmp(name, TC_predef_char[i]) == 0) {
      return TC_predef_int[i];
    }
  }
  return -1;
}

TransformationCoefficients::TransformationCoefficients(const char *coef_filename) {
  if (coef_filename == 0) {
    coef_filename = "/home/ASTRO/CURRENT_DATA/transforms.ini";
  }
  FILE *fp = fopen(coef_filename, "r");
  if (!fp) {
    fprintf(stderr, "transformation_coefficients: cannot read file %s\n",
	    coef_filename);
    exit(-2);
  }

  bool in_coefficients = false;
  char buffer[512];
  while(fgets(buffer, sizeof(buffer), fp)) {
    if (strcmp(buffer, "[Coefficients]\n") == 0) {
      in_coefficients = true;
    } else if (buffer[0] == '[') {
      in_coefficients = false;
    } else if (in_coefficients) {
      // look for an "="
      char *s = buffer;
      while(*s && *s != '=') s++;
      if (*s == '=') {
	*s = 0;
	const char *second = s+1;
	double value = strtod(second, 0);
	int param = lookup_param_name(buffer);
	if (param < 0) {
	  fprintf(stderr, "trans_coef: unrecognized coefficient: %s\n",
		  buffer);
	} else {
	  coefficients.emplace(param, value);
	}
      } else {
	fprintf(stderr, "trans_coef: unable to parse line: %s\n", buffer);
      }
    }
  }
  fprintf(stderr, "%d transformation coefficients read from %s\n",
	  (int) coefficients.size(), coef_filename);
  fclose(fp);
}

// Typical strings that name Coefficients: "Tbv", "Tv_bv", ...
double
TransformationCoefficients::Coefficient(const char *name) const {
  int param = lookup_param_name(name);
  if (param < 0) return NAN;

  return coefficients.at(param);
}

// Typical strings that name Coefficients: "Tbv", "Tv_bv", ...
double
TransformationCoefficients::Coefficient(int name) const {
  return coefficients.at(name);
}

