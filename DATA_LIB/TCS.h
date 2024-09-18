/* This may look like C code, but it is really -*-c++-*- */
/*  TCS.h -- Coordinate system conversions
 *
 *  Copyright (C) 2007 Mark J. Munkacsy

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
#ifndef _TCS_H
#define _TCS_H

#include <stdio.h>
#include <dec_ra.h>

// Tangent Coordinate System (North is always up)
class TCS {
public:
  double x, y;			// 0,0 in center
};

// Pixel Coordinate System (orientation unknown)
class PCS {
public:
  double x, y;			// 0,0 in upper left corner
};

// Transform between TCS and Image/pixel coordinate systems
class TCStoImage {
public:
  // Constructor and destructor

  TCStoImage(double image_scale,     // arcsec/pixel
	     PCS center,	     // coordinates of center pixel
	     double rotation = 0.0);	// clockwise to make North up

  ~TCStoImage(void) {;}

  TCS toTCS(PCS &loc);
  PCS toPCS(TCS &loc);
  void print(FILE *fp);

private:
  double pixel_scale;		// arcsec/pixel
  PCS image_center;
  double XFORM_rotation;
  double sin_rotation;
  double cos_rotation;
  double XFORM_scale;
  double XFORM_horiz_factor;
};

class TCSXform {
public:
  TCSXform(double rotation,
	   double scale);
  TCSXform(const TCS &cat_ref,
	   const TCS &cat_alt,
	   const TCS &image_alt);

  void SetScaleRotation(double rotation, double scale);

  // This converts from image to catalog
  TCS toTCS(const TCS &loc) const;
  // This converts from catalog to image
  TCS toTCSInverse(const TCS &loc) const;

  // Here are a set of methods to get and set a persistent state
  void SavePersistentState(void);

  void SetConfidence(double confidence_factor);
  double Confidence(void);
  const char *ToString(void);

private:
  double x_rotation;
  double x_scale;
  double x_confidence;

  double f1, f2;
  double g1, g2;
};

TCSXform *
GetPersistentState(void);

// The class TCStoDecRA is used to convert back and forth between
// DecRA coordinates and the TCS coordinate system centered on the
// specified "center" (the tangent point).
class TCStoDecRA {
public:
  // Constructor and destructor

  TCStoDecRA(DEC_RA &center) : XFORM_center(center) {; }
  ~TCStoDecRA(void) {;}

  DEC_RA toDecRA(TCS &loc);
  TCS    toTCS(DEC_RA &loc);

private:
  DEC_RA XFORM_center;
};

#endif
