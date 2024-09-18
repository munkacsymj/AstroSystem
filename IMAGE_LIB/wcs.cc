/* This may look like C code, but it is really -*-c++-*- */
/*  wcs.cc -- Manage coordinate system transformations
 *
 *  Copyright (C) 2018, 2020 Mark J. Munkacsy

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

#include "wcs.h"
#include <string.h>
#include <Image.h>		// ImageInfo
#include <list>
#include <string>

#define Str(x) #x
#define Xstr(x) Str(x)
#define LINENO Xstr(__LINE__)

WCS::WCS(WCS_ENUM_TYPE wcs_variant) {
  wcs_is_valid = false;
  wcs_type = wcs_variant;
}
  
WCS *NewWCS(ImageInfo *info) {
  if (!info->KeywordPresent("WCSTYPE")) return 0;
  const string &w_type = info->GetValueString("WCSTYPE");

  if (w_type == "BILINEAR") {
    return new WCS_Bilinear(info);
  }
  if (w_type == "SIMPLE") {
    return new WCS_Simple(info);
  }

  fprintf(stderr, "wcs.cc: illegal WCSTYPE keyword encountered: %s\n", w_type.c_str());
  return new WCS_Simple(0);
}

WCS *NewWCS(WCS_ENUM_TYPE wcs_variant) {
  switch(wcs_variant) {
  case WCS_SIMPLE:
    return new WCS_Simple(0);
  case WCS_BILINEAR:
    return new WCS_Bilinear(0);
  }
  
  fprintf(stderr, "wcs.cc: illegal wcs_variant enum encountered: %d\n", (int) wcs_variant);
  return new WCS_Simple(0);
}

#if 0
WCS_Bilinear::WCS_Bilinear(void) : WCS(WCS_BILINEAR)  {
  points_set = 0;

  width_in_pixels = 512.0;
  height_in_pixels = 512.0;

  upperleft_dec = 0.0;
  upperright_dec = 0.0;
  lowerleft_dec = 0.0;
  lowerright_dec = 0.0;

  upperleft_ra = 0.0;
  upperright_ra = 0.0;
  lowerleft_ra = 0.0;
  lowerright_ra = 0.0;
}
#endif

void
WCS_Bilinear::PrintRotAndScale(void) const {
  double avg_delta_dec = ((upperleft_dec-lowerleft_dec)+(upperright_dec-lowerright_dec))/2.0;
  double scale = (avg_delta_dec/height_in_pixels)*180.0*3600.0/M_PI; // arcsec/pixel
  double top_delta_dec = (upperleft_dec-upperright_dec);
  double bottom_delta_dec = (lowerleft_dec-lowerright_dec);
  double left_right_delta_dec = (top_delta_dec+bottom_delta_dec)/2.0;
  double delta_y = left_right_delta_dec*3600.0*180.0/M_PI; // arcsec NS
  double delta_x = width_in_pixels*scale; // arcsec EW
  double rotation_angle = atan2(delta_y, delta_x);
  
  fprintf(stderr, "Rotation angle = %.1lf deg, Scale = %.2lf arcsec/pixel\n",
	  180.0*rotation_angle/M_PI, scale);
}

void WCS_Bilinear::MakeWellBehaved(void) {
  if (fabs(upperleft_ra - upperright_ra) > M_PI ||
      fabs(lowerleft_ra - lowerright_ra) > M_PI) {
    if (upperleft_ra < 0.0) upperleft_ra += (2*M_PI);
    if (upperright_ra < 0.0) upperright_ra += (2*M_PI);
    if (lowerleft_ra < 0.0) lowerleft_ra += (2*M_PI);
    if (lowerright_ra < 0.0) lowerright_ra += (2*M_PI);
  }
}
    
WCS_Bilinear::WCS_Bilinear(int height, int width) : WCS(WCS_BILINEAR) {
  width_in_pixels = width;
  height_in_pixels = height;

  points_set = 0;

  upperleft_dec = 0.0;
  upperright_dec = 0.0;
  lowerleft_dec = 0.0;
  lowerright_dec = 0.0;

  upperleft_ra = 0.0;
  upperright_ra = 0.0;
  lowerleft_ra = 0.0;
  lowerright_ra = 0.0;
}

WCS_Bilinear::WCS_Bilinear(ImageInfo *info) : WCS(WCS_BILINEAR) {
  bool any_err = false;

  if(info == 0) return; // this creates an "empty" WCS

  width_in_pixels = info->width;
  height_in_pixels = info->height;
  
  if (!info->KeywordPresent("WCSULDEC")) {
    fprintf(stderr, "WCSULDEC keyword missing.\n");
    any_err = true;
  }

  if (!info->KeywordPresent("WCSURDEC")) {
    fprintf(stderr, "WCSURDEC keyword missing.\n");
    any_err = true;
  }

    if (!info->KeywordPresent("WCSLLDEC")) {
    fprintf(stderr, "WCSLLDEC keyword missing.\n");
    any_err = true;
  }

  if (!info->KeywordPresent("WCSLRDEC")) {
    fprintf(stderr, "WCSLRDEC keyword missing.\n");
    any_err = true;
  }

  if (!info->KeywordPresent("WCSULRA")) {
    fprintf(stderr, "WCSULRA keyword missing.\n");
    any_err = true;
  }

  if (!info->KeywordPresent("WCSURRA")) {
    fprintf(stderr, "WCSURRA keyword missing.\n");
    any_err = true;
  }

    if (!info->KeywordPresent("WCSLLRA")) {
    fprintf(stderr, "WCSLLRA keyword missing.\n");
    any_err = true;
  }

  if (!info->KeywordPresent("WCSLRRA")) {
    fprintf(stderr, "WCSLRRA keyword missing.\n");
    any_err = true;
  }

  if (any_err == false) {
    upperleft_dec = info->GetValueDouble("WCSULDEC");
    upperright_dec = info->GetValueDouble("WCSURDEC");
    lowerleft_dec = info->GetValueDouble("WCSLLDEC");
    lowerright_dec = info->GetValueDouble("WCSLRDEC");
    upperleft_ra = info->GetValueDouble("WCSULRA");
    upperright_ra = info->GetValueDouble("WCSURRA");
    lowerleft_ra = info->GetValueDouble("WCSLLRA");
    lowerright_ra = info->GetValueDouble("WCSLRRA");
    
    MakeWellBehaved();
  }
  wcs_is_valid = !any_err;
}

void SetValuePrecise(ImageInfo *info, const char *keyword, double value) {
  char buffer[80];
  sprintf(buffer, "%.15lf", value);
  info->SetValue(string(keyword), string(buffer));
}

void
WCS_Bilinear::UpdateFITSHeader(ImageInfo *info) const {
  info->SetValueString(string("WCSTYPE"), string("BILINEAR"));
  SetValuePrecise(info, "WCSULDEC", upperleft_dec);
  SetValuePrecise(info, "WCSURDEC", upperright_dec);
  SetValuePrecise(info, "WCSLLDEC", lowerleft_dec);
  SetValuePrecise(info, "WCSLRDEC", lowerright_dec);
  SetValuePrecise(info, "WCSULRA", upperleft_ra);
  SetValuePrecise(info, "WCSURRA", upperright_ra);
  SetValuePrecise(info, "WCSLLRA", lowerleft_ra);
  SetValuePrecise(info, "WCSLRRA", lowerright_ra);
}

// convert from pixel coordinates to Dec/RA
// (remember, "left RA" > "right RA")
DEC_RA
WCS_Bilinear::Transform(double x, double y) const {
  const double del_dec_top = (upperright_dec - upperleft_dec);
  const double del_dec_bottom = (lowerright_dec - lowerleft_dec);
  const double del_ra_top = (upperright_ra - upperleft_ra);
  const double del_ra_bottom = (lowerright_ra - lowerleft_ra);

  const double fraction_x = (x/width_in_pixels);

  const double interp_top_ra = upperleft_ra + fraction_x*del_ra_top;
  const double interp_bottom_ra = lowerleft_ra + fraction_x*del_ra_bottom;
  const double interp_top_dec = upperleft_dec + fraction_x*del_dec_top;
  const double interp_bottom_dec = lowerleft_dec + fraction_x*del_dec_bottom;

  const double fraction_y = (y/height_in_pixels);
  const double del_dec = interp_top_dec - interp_bottom_dec;
  const double del_ra = interp_top_ra - interp_bottom_ra;

  const double final_dec = interp_bottom_dec + fraction_y*del_dec;
  double final_ra = interp_bottom_ra + fraction_y*del_ra;

  if (final_ra < 0.0) final_ra += (2.0*M_PI);
  return DEC_RA(final_dec, final_ra);
}
  
// convert from Dec/RA to pixel coordinates
#define GOOD_ENOUGH (.001*(M_PI/(180*3600))) // .001 arcsec in radians

double distance_between(DEC_RA p1, DEC_RA p2) {
  double del_dec = p1.dec() - p2.dec();
  double del_ra = p1.ra_radians() - p2.ra_radians();
  del_ra *= (1.0/cos(p1.dec()));

  return sqrt(del_dec*del_dec + del_ra*del_ra);
}

struct OnePoint {
  DEC_RA dec_ra;
  double x;
  double y;
};

void
WCS_Bilinear::Transform(DEC_RA *dec_ra, double *x, double *y) const {
  OnePoint ur, ul, lr, ll, interp;

  ur.x = width_in_pixels;
  ul.x = 0.0;
  lr.x = width_in_pixels;
  ll.x = 0.0;

  ur.y = height_in_pixels;
  ul.y = height_in_pixels;
  lr.y = 0.0;
  ll.y = 0.0;

  ur.dec_ra = Transform(ur.x, ur.y);
  ul.dec_ra = Transform(ul.x, ul.y);
  lr.dec_ra = Transform(lr.x, lr.y);
  ll.dec_ra = Transform(ll.x, ll.y);

  unsigned int loop_count = 0;

  do {
    // interpolate, bilinearly...
    double width = ul.x - ur.x;
    double height = ur.y - lr.y;
    
    const double ra_span_top = ul.dec_ra.ra_radians() - ur.dec_ra.ra_radians();
    const double ra_span_bottom = ll.dec_ra.ra_radians() - lr.dec_ra.ra_radians();
    const double fraction_top = (dec_ra->ra_radians() - ur.dec_ra.ra_radians())/ra_span_top;
    const double fraction_bottom = (dec_ra->ra_radians() - lr.dec_ra.ra_radians())/ra_span_bottom;

    OnePoint top_mid, bottom_mid;
    top_mid.x = ur.x + fraction_top*width;
    top_mid.y = ur.y;
    bottom_mid.x = lr.x + fraction_bottom*width;
    bottom_mid.y = lr.y;
    top_mid.dec_ra = Transform(top_mid.x, top_mid.y);
    bottom_mid.dec_ra = Transform(bottom_mid.x, bottom_mid.y);

    const double dec_span = top_mid.dec_ra.dec() - bottom_mid.dec_ra.dec();
    const double fraction_vertical = (dec_ra->dec() - bottom_mid.dec_ra.dec())/dec_span;

    interp.x = fraction_vertical*top_mid.x + (1.0-fraction_vertical)*bottom_mid.x;
    interp.y = fraction_vertical*height + lr.y;
    interp.dec_ra = Transform(interp.x, interp.y);

    if (distance_between(interp.dec_ra, *dec_ra) <= GOOD_ENOUGH) {
      *x = interp.x;
      *y = interp.y;
      return;
    }

    bool is_upper = false;
    bool is_left = false;
    
    // Now pick one of four quadrants
    if((dec_ra->dec() - ll.dec_ra.dec())/(interp.dec_ra.dec() - ll.dec_ra.dec()) > 1.0) {
      // target point is in the upper quadrants
      is_upper = true;
    } else {
      is_upper = false;
    }

    if ((dec_ra->ra_radians() - ll.dec_ra.ra_radians())/(interp.dec_ra.ra_radians() - ll.dec_ra.ra_radians()) > 1.0) {
      is_left = false;
    } else {
      is_left = true;
    }

    if (is_upper && !is_left) {
      ll = interp;
      ul.x = ll.x;
      ul.y = ur.y;
      lr.x = ur.x;
      lr.y = ll.y;
      lr.dec_ra = Transform(lr.x, lr.y);
      ul.dec_ra = Transform(ul.x, ul.y);
    } else if (is_upper && is_left) {
      lr = interp;
      ur.x = lr.x;
      ur.y = ul.y;
      ll.x = ul.x;
      ll.y = lr.y;
      ll.dec_ra = Transform(ll.x, ll.y);
      ur.dec_ra = Transform(ur.x, ur.y);
    } else if (!is_left) { // lower right
      ul = interp;
      ll.x = ul.x;
      ll.y = lr.y;
      ur.x = lr.x;
      ur.y = ul.y;
      ur.dec_ra = Transform(ur.x, ur.y);
      ll.dec_ra = Transform(ll.x, ll.y);
    } else { // lower left
      ur = interp;
      lr.x = ur.x;
      lr.y = ll.y;
      ul.x = ll.x;
      ul.y = ur.y;
      ul.dec_ra = Transform(ul.x, ul.y);
      lr.dec_ra = Transform(lr.x, lr.y);
    }
  } while (loop_count++ < 12);
  if (interp.x >= 0 && interp.x <= width_in_pixels &&
      interp.y >= 0 && interp.y <= height_in_pixels) {
    fprintf(stderr, "wcs.cc: Transform(): did not converge.\n");
  } else {
    *x = interp.x;
    *y = interp.y;
  }
}


#define POINT_UL 0x1
#define POINT_UR 0x2
#define POINT_LL 0x4
#define POINT_LR 0x8
#define ALL_POINTS 0x0f

void
WCS_Bilinear::SetULPoint(DEC_RA point) {
  upperleft_dec = point.dec();
  upperleft_ra = point.ra_radians();
  points_set |= POINT_UL;
  if (points_set == ALL_POINTS) normalize();
}
void
WCS_Bilinear::SetURPoint(DEC_RA point) {
  upperright_dec = point.dec();
  upperright_ra = point.ra_radians();
  points_set |= POINT_UR;
  if (points_set == ALL_POINTS) normalize();
}
void
WCS_Bilinear::SetLLPoint(DEC_RA point) {
  lowerleft_dec = point.dec();
  lowerleft_ra = point.ra_radians();
  points_set |= POINT_LL;
  if (points_set == ALL_POINTS) normalize();
}
void
WCS_Bilinear::SetLRPoint(DEC_RA point) {
  lowerright_dec = point.dec();
  lowerright_ra = point.ra_radians();
  points_set |= POINT_LR;
  if (points_set == ALL_POINTS) normalize();
}

void
WCS_Bilinear::normalize(void) {
  wcs_is_valid = true;
  //if (upperleft_ra < upperright_ra) {
  //upperright_ra -= (2.0*M_PI);
  //lowerright_ra -= (2.0*M_PI);
  //}
}

DEC_RA
WCS_Bilinear::Center(void) const {
  return Transform(width_in_pixels/2, height_in_pixels/2);
}


//****************************************************************
//        WCS_Simple
//****************************************************************
WCS_Simple::WCS_Simple(void) : WCS(WCS_SIMPLE) {
  cos_dec = 1.0;
}

std::list<std::string> simple_keywords
  { "WCSROT",
    "WCSDECCTR",
    "WCSRACTR",
    "WCSSCALE" };
    
DEC_RA
WCS_Simple::Center(void) const {
  return Transform(image_width/2, image_height/2);
}

WCS_Simple::WCS_Simple(ImageInfo *info) : WCS(WCS_SIMPLE) {
  bool any_err = false;
  double declination = 0.0;
  double right_ascension = 0.0;

  if (info == 0) {
    wcs_is_valid = false;
    return;
  }

  for (auto keyword : simple_keywords) {
    if (!info->KeywordPresent(keyword.c_str())) {
      fprintf(stderr, "%s keyword missing.\n",
	      keyword.c_str());
      any_err = true;
    }
  }

  if (!any_err) {
    rotation_angle = info->GetValueDouble("WCSROT");
    scale = info->GetValueDouble("WCSSCALE");
    
    declination = info->GetValueDouble("WCSDECCTR");
    right_ascension = info->GetValueDouble("WCSRACTR");

    center_point = DEC_RA(declination, right_ascension);
  }

  wcs_is_valid = !any_err;
}
  
void
WCS_Simple::UpdateFITSHeader(ImageInfo *info) const {
  if (info == 0 || !wcs_is_valid) return;

  const double declination = center_point.dec();
  const double right_ascension = center_point.ra_radians();

  info->SetValueString("WCSTYPE", "SIMPLE");
  SetValuePrecise(info, "WCSROT", rotation_angle);
  SetValuePrecise(info, "WCSSCALE", scale);
  SetValuePrecise(info, "WCSDECCTR", declination);
  SetValuePrecise(info, "WCSRACTR", right_ascension);
}

  // The following methods are for general users
void
WCS_Simple::Set(DEC_RA &center,
		double img_scale,	// arcsec/pixel
		double rotation) { // radians
  center_point = center;
  scale = img_scale;
  rotation_angle = rotation;
  cos_dec = cos(center.dec());
}


void WCS_Simple::SetImageSize(int width, int height) {
  image_width = width;
  image_height = height;
}

// convert from pixel coordinates to Dec/RA
// incoming (x,y) is based on image pixel origin (in corner)
DEC_RA
WCS_Simple::Transform(double x, double y) const {
  const double center_x = image_width/2.0;
  const double center_y = image_height/2.0;
  const double cos_rot = cos(rotation_angle);
  const double sin_rot = sin(rotation_angle);

  const double offset_x = x-center_x;
  const double offset_y = y-center_y;

  const double offset_ew = offset_x*cos_rot + offset_y*sin_rot;
  const double offset_ns = offset_y*cos_rot - offset_x*sin_rot;

  const double del_dec = offset_ns*scale*M_PI/(3600.0*180.0);
  const double del_ra = offset_ew*(scale/cos_dec)*M_PI/(3600.0*180.0);

  return DEC_RA(center_point.dec()+del_dec,
		center_point.ra_radians()+del_ra);
}

  // convert from Dec/RA to pixel coordinates
void
WCS_Simple::Transform(DEC_RA *dec_ra, double *x, double *y) const {
  const double cos_rot = cos(rotation_angle);
  const double sin_rot = sin(rotation_angle);
  const double delta_dec = dec_ra->dec()-center_point.dec();
  const double delta_ra = (dec_ra->ra_radians()-center_point.ra_radians())*cos_dec;

  const double delta_ew = delta_ra*(3600.0*180.0/M_PI)/scale;
  const double delta_ns = delta_dec*(3600.0*180.0/M_PI)/scale;

  const double delta_x = delta_ew*cos_rot - delta_ns*sin_rot;
  const double delta_y = delta_ew*sin_rot + delta_ns*cos_rot;

  *x = image_width/2.0 + delta_x;
  *y = image_height/2.0 + delta_y;
}

void
WCS_Simple::PrintRotAndScale(void) const {
  fprintf(stderr, "Rotation angle = %.1lf deg, Scale = %.2lf arcsec/pixel\n",
	  180.0*rotation_angle/M_PI, scale);
}
