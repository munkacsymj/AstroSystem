// This may look like C code, but it is really -*- C++ -*-
/*  plan_exposure.h -- manages the selection of exposure times
 *
 *  Copyright (C) 2017 Mark J. Munkacsy

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

#ifndef _PLAN_EXPOSURE_H
#define _PLAN_EXPOSURE_H

#include <Filter.h>
#include <HGSC.h>		// PhotometryColor
#include <list>
#include <unordered_map>

class Image;

struct FilterExposurePlan {
public:
  double eTime;			// subexposure time
  unsigned int eQuantity;	// number of exposures
  int eCameraGain;
  int eCameraMode;
  int eCameraOffset;
};

struct ExposurePlanList {
  bool exposure_plan_valid;
  std::unordered_map<PhotometryColor, FilterExposurePlan> exposure_plan_list;

  void clear(void) { exposure_plan_valid = false; exposure_plan_list.clear(); }
};

typedef std::list<double> MagnitudeList;
typedef std::unordered_map<PhotometryColor, MagnitudeList> ColorMagnitudeList;

// Must be called exactly once at the beginning. Darks must
// exist. Probably needs to have "optical configuration" added as an
// argument. 
void InitializeExposurePlanner(const char *homedir);

// Any image of the sky is useful. Will be used for skyglow, HWFM PSF,
// and flux/magnitude ratio.
void AddImageToExposurePlanner(Image &image, const char *image_filename);

// You provide a list of star magnitudes of interest. This will return
// a set of observing plans.
const ExposurePlanList &GetExposurePlan(const ColorMagnitudeList &ml);

void ExposurePlannerPrintMeasurements(void);

#endif
