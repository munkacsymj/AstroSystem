/*  aavso_photometry.cc -- parses AAVSO response to VSP request for photmetry table
 *
 *  Copyright (C) 2020 Mark J. Munkacsy

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
#ifndef _AAVSO_PHOTOMETRY_H
#define _AAVSO_PHOTOMETRY_H

#include <dec_ra.h>
#include <HGSC.h>		// MultiColorData
#include <list>

struct PhotometryRecord {
  char PR_AUID[16];
  DEC_RA PR_location;
  char PR_chart_label[16];
  double PR_V_mag;
  MultiColorData PR_colordata;
  char PR_ChartID[16];
};

typedef std::list<PhotometryRecord *> PhotometryRecordSet;

PhotometryRecordSet *ParseAAVSOResponse(const char *buffer);

#endif
