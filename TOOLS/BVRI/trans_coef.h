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

#ifndef _TRANS_COEF_H
#define _TRANS_COEF_H

#include <unordered_map>

#define TC_Tr_vi 0
#define TC_Ti_vi 1
#define TC_Tv_vi 2
#define TC_Tvr   3
#define TC_Tr_ri 4
#define TC_Tv_vr 5
#define TC_Ti_ri 6
#define TC_Tbv   7
#define TC_Tb_bv 8
#define TC_Tr_vr 9
#define TC_Tv_bv 10
#define TC_Tri   11
#define TC_Tvi   12
#define TC_Tbr   13
#define TC_Tbi   14
#define TC_Tb_br 15
#define TC_Tb_bi 16

class TransformationCoefficients {
 public:
  TransformationCoefficients(const char *coef_filename = 0);
  ~TransformationCoefficients(void) {;}

  // Typical strings that name Coefficients: "Tbv", "Tv_bv", ...
  double Coefficient(const char *name) const;
  double Coefficient(int name) const;

 private:
  std::unordered_map<int, double> coefficients;
};

#endif
