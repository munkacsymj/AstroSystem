/* This may look like C code, but it is really -*-c++-*- */
/*  model.h -- Definition of data structure "Model"
 *
 *  Copyright (C) 2007, 2017, 2020 Mark J. Munkacsy
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
#ifndef _MODEL_H
#define _MODEL_H

struct Model {
  double center_x, center_y;
  double defocus_width;
  double obstruction_fraction;
  double gaussian_sigma;
  double collimation_x;		// width-wise collimation error
  double collimation_y;		// height-wise collimation error
  double background;
};

#endif
