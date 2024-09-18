/* This may look like C code, but it is really -*-c++-*- */
/*  wcs.h -- Manage coordinate system transformations
 *
 *  Copyright (C) 2018 Mark J. Munkacsy

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
#ifndef _WCS_H
#define _WCS_H

#include <fitsio.h>
#include <dec_ra.h>

class ImageInfo;

enum WCS_ENUM_TYPE { WCS_SIMPLE, WCS_BILINEAR };

class WCS {
 public:
  // The following methods are for use only by the ImageInfo class' internals
  WCS(WCS_ENUM_TYPE wcs_variant);
  virtual ~WCS(void) {;}
  virtual void UpdateFITSHeader(ImageInfo *info) const = 0;

  // The following methods are for general users

  bool IsValidWCS(void) const { return wcs_is_valid; }
  // convert from pixel coordinates to Dec/RA
  virtual DEC_RA Transform(double x, double y) const = 0;
  // convert from Dec/RA to pixel coordinates
  virtual void Transform(DEC_RA *dec_ra, double *x, double *y) const = 0;
  virtual void Transform(DEC_RA &dec_ra, double *x, double *y) const { Transform(&dec_ra, x, y); }
  virtual DEC_RA Center(void) const = 0;
  virtual void PrintRotAndScale(void) const = 0;

 protected:
  WCS_ENUM_TYPE wcs_type;
  bool wcs_is_valid;
};

// This is the WCS creation factory. Use it to create a WCS
WCS *NewWCS(WCS_ENUM_TYPE wcs_variant);
WCS *NewWCS(ImageInfo *info);

class WCS_Bilinear : public WCS {
 public:
  //WCS_Bilinear(void);
  WCS_Bilinear(int height, int width);
  WCS_Bilinear(ImageInfo *info);
  ~WCS_Bilinear(void) {;}
  
  void UpdateFITSHeader(ImageInfo *info) const;

  // The following methods are for general users

  // convert from pixel coordinates to Dec/RA
  DEC_RA Transform(double x, double y) const;
  // convert from Dec/RA to pixel coordinates
  void Transform(DEC_RA *dec_ra, double *x, double *y) const;

  // only for use by star_match:
  void SetULPoint(DEC_RA point);
  void SetURPoint(DEC_RA point);
  void SetLLPoint(DEC_RA point);
  void SetLRPoint(DEC_RA point);

  DEC_RA Center(void) const;
  void PrintRotAndScale(void) const;
  
 private:
  double upperleft_dec;
  double upperright_dec;
  double lowerleft_dec;
  double lowerright_dec;

  double upperleft_ra;
  double upperright_ra;
  double lowerleft_ra;
  double lowerright_ra;

  double width_in_pixels;
  double height_in_pixels;

  int points_set; // populated by binary flags

  void normalize(void); // handle RA=00:00:00 wraparound issues

  void MakeWellBehaved(void); // undoes what normalize does
};

class WCS_Simple : public WCS {
 public:
  WCS_Simple(void);
  WCS_Simple(ImageInfo *info);
  ~WCS_Simple(void) {;}

  void SetImageSize(int width, int height);
  void Set(DEC_RA &center,
	   double scale,	// arcsec/pixel
	   double rotation);	// radians
  void UpdateFITSHeader(ImageInfo *info) const;

  // The following methods are for general users

  // convert from pixel coordinates to Dec/RA
  DEC_RA Transform(double x, double y) const;
  // convert from Dec/RA to pixel coordinates
  void Transform(DEC_RA *dec_ra, double *x, double *y) const;
  DEC_RA Center(void) const;
  void PrintRotAndScale(void) const;

 private:
  double rotation_angle;
  DEC_RA center_point;
  double scale; // arcseconds per pixel
  double cos_dec;
  double image_width; // pixels
  double image_height; // pixels
};

#endif
