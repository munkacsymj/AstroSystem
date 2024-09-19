/* This may look like C code, but it is really -*-c++-*- */
/*  correlate.h -- Correlate stars in an image with a catalog
 *
 *  Copyright (C) 2007 Mark J. Munkacsy
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
#include <IStarList.h>
#include <dec_ra.h>
#include <Image.h>
#include "TCS.h"
#include "wcs.h"

struct Verbosity {
  bool residuals;
  bool fixups;
  bool starlists;
  bool catalog;
  bool unmatched;
};

extern Verbosity verbosity;

WCS_Bilinear *
correlate(Image &primary_image,
	  IStarList *list,
	  const char *HGSCfilename,
	  DEC_RA *ref_location,
	  const char *param_filename,
	  int no_shortcuts,
	  int use_high_precision,
	  const char *residual_filename);
