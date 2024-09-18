/*  TCS.cc -- Coordinate system conversions
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
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include "TCS.h"
#include <gendefs.h>

const char *TransformFilename = TRANSFORM_DIR "/transform_state.dat";

/****************************************************************/
/*        Save persistent transformation state			*/
/****************************************************************/
void
TCSXform::SavePersistentState(void) {
  FILE *fp = fopen(TransformFilename, "w");
  if(!fp) {
    perror("Cannot create persistent transform file");
    return;
  }

  fprintf(fp, "%f %f %f\n", x_scale, x_rotation, x_confidence);
  fclose(fp);
}

/****************************************************************/
/*        Fetch persistent transformation state			*/
/****************************************************************/
TCSXform *
GetDefaultPersistentState(void) {
  TCSXform *new_transform =  new TCSXform(0.0, 1.0);
  new_transform->SetConfidence(25.0); // default confidence
  return new_transform;
}

TCSXform *
GetPersistentState(void) {
  struct stat FileData;
  struct timespec RightNow;
  struct timeval RightNow_timeval;

  gettimeofday(&RightNow_timeval, 0);
  TIMEVAL_TO_TIMESPEC(&RightNow_timeval, &RightNow);

  // We want to know if the file holding the persistent transform was
  // updated within the last 6 hours.  If so, we believe it. If not,
  // we ignore the file and set the state to a default value.
  if(stat(TransformFilename, &FileData) < 0) {
    return GetDefaultPersistentState();
  }

#ifdef linux
  if(FileData.st_mtime + 6*3600 < RightNow.tv_sec) // 6 hrs
#else
  if(FileData.st_mtimespec.tv_sec + 6*3600 < RightNow.tv_sec) // 6 hrs
#endif
    return GetDefaultPersistentState();

  // File seems to be current.  Open it and read it.
  FILE *fp = fopen(TransformFilename, "r");
  if(!fp) {
    perror("Puzzled: persistent transform file stat() okay but cannot open");
    fclose(fp);
    return GetDefaultPersistentState();
  }

  double scale_fp, rotation_fp, confidence_fp;
  if(fscanf(fp, "%lf %lf %lf",
	    &scale_fp,
	    &rotation_fp,
	    &confidence_fp) != 2) {
    fprintf(stderr, "Persistent transform file format bad.\n");
    fclose(fp);
    return GetDefaultPersistentState();
  }

  fclose(fp);
  TCSXform *new_xform = new TCSXform(rotation_fp, scale_fp);
  new_xform->SetConfidence(confidence_fp);
  return new_xform;
}

/****************************************************************/
/*        Constructors						*/
/****************************************************************/
TCSXform::TCSXform(double rotation,
		   double scale) {
  SetScaleRotation(rotation, scale);
}

void
TCSXform::SetScaleRotation(double rotation, double scale) {
  x_rotation = rotation;
  x_scale    = scale;

  f1 = cos(rotation) * scale;
  f2 = sin(rotation) * scale;
  g1 = -f2;
  g2 = f1;
}

const char *
TCSXform::ToString(void) {
  static char buffer[80];
  sprintf(buffer, "scale = %.3lf, rotation = %.2lf deg",
	  x_scale, x_rotation*180.0/M_PI);
  return buffer;
}

TCS
TCSXform::toTCS(const TCS &loc) const {
  TCS answer;

  answer.x = f1*loc.x + f2*loc.y;
  answer.y = g1*loc.x + g2*loc.y;
  return answer;
}

// This converts from catalog to image
TCS
TCSXform::toTCSInverse(const TCS &loc) const {
  TCS answer;

  double denom = f1*g2 - f2*g1;
  answer.x = (g2*loc.x - f2*loc.y)/denom;
  answer.y = (f1*loc.y - g1*loc.x)/denom;
  return answer;
}


TCSXform::TCSXform(const TCS &cat_ref,
		   const TCS &cat_alt,
		   const TCS &image_alt) {
  const TCS &image_ref = cat_ref;

  const double del_cat_x = cat_alt.x - cat_ref.x;
  const double del_cat_y = cat_alt.y - cat_ref.y;
  const double del_image_x = image_alt.x - image_ref.x;
  const double del_image_y = image_alt.y - image_ref.y;

  const double r_cat = sqrt(del_cat_x * del_cat_x +
			    del_cat_y * del_cat_y);
  const double r_image = sqrt(del_image_x * del_image_x +
			      del_image_y * del_image_y);
  const double scale = r_cat/r_image;

  const double alpha = atan2(del_image_y, del_image_x) -
    atan2(del_cat_y, del_cat_x);

  SetScaleRotation(alpha, scale);
}
  
DEC_RA
TCStoDecRA::toDecRA(TCS &loc) {
  double dec = XFORM_center.dec();
  const double cos_dec = cos(dec);
  double ra  = XFORM_center.ra_radians();

  dec += loc.y;			// +y = North
  ra  += (loc.x/cos_dec);	// +x = East

  return DEC_RA(dec, ra);
}

TCS    
TCStoDecRA::toTCS(DEC_RA &loc) {
  const double cos_dec = cos(XFORM_center.dec());

  double delta_dec = loc.dec() - XFORM_center.dec();
  double delta_ra  = loc.ra_radians() - XFORM_center.ra_radians();

  TCS answer;
  answer.y = delta_dec;
  answer.x = delta_ra * cos_dec;

  return answer;
}

TCStoImage::TCStoImage(double image_scale,
		       PCS center,
		       double rotation) {	// clockwise to make North up

  // XFORM_scale is in units of radians per pixel
  constexpr double RADIANSperARCSEC = 2.0*M_PI/(360.0*3600.0);

  XFORM_rotation = rotation;
  sin_rotation = sin(rotation);
  cos_rotation = cos(rotation);

  XFORM_horiz_factor = 1.0; // means pixels are square, not rectangular
  XFORM_scale = image_scale * RADIANSperARCSEC;

  pixel_scale = image_scale;
  image_center = center;
}

// angle is rotation angle in radians
static TCS rotate(TCS xy, double cos_angle, double sin_angle) {
  TCS answer;

  answer.x = xy.x * cos_angle - xy.y * sin_angle;
  answer.y = xy.x * sin_angle + xy.y * cos_angle;

  return answer;
}

TCS
TCStoImage::toTCS(PCS &loc) {
  // PCS +y moves in the North direction
  // PCS +x moves in the East direction
  TCS answer;

  answer.y = XFORM_scale*(loc.y - image_center.y);
  answer.x = XFORM_horiz_factor * XFORM_scale * (loc.x - image_center.x);

  return rotate(answer, cos_rotation, sin_rotation);
}

void
TCStoImage::print(FILE *fp) {
  fprintf(fp, "XFORM_scale = %f, XFORM_horiz_factor = %f\n",
	  XFORM_scale, XFORM_horiz_factor);
}

PCS
TCStoImage::toPCS(TCS &loc) {
  // PCS +y moves in the North direction
  // PCS +x moves in the East direction
  PCS answer;
  // rotate through a negative angle, and use identities for sin,cos
  TCS rotated = rotate(loc, cos_rotation, -sin_rotation);

  answer.y = (rotated.y/XFORM_scale) + image_center.y;
  answer.x = rotated.x/(XFORM_horiz_factor * XFORM_scale) + image_center.x;

  return answer;
}

void
TCSXform::SetConfidence(double confidence_factor) {
  x_confidence = confidence_factor;
}

double
TCSXform::Confidence(void) {
  return x_confidence;
}

