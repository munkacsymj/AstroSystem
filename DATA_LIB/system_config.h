// This may look like C code, but it is really -*- C++ -*-
/*  system_config.h -- manage a database of what's installed in the observatory
 *
 *  Copyright (C) 2021 Mark J. Munkacsy

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

#ifndef _SYSTEM_CONFIG_H
#define _SYSTEM_CONFIG_H

#include <string>
#include <list>
#include <unordered_map>

#include "json.h"
#include "scope_api.h"

extern const char *CONFIG_FILE;

enum SC_Optical_Configuration {SC_ST9_Meade10,
			       SC_268M_Meade10,
			       SC_ST9_C14_C63x,
			       SC_ST9_C14_Starizona,
			       SC_268M_C14_Starizona,
			       SC_Cookbook_Meade10,
			       SC_NONSTANDARD,
};
			       

class SystemConfig {
 public:
  SystemConfig(void);
  ~SystemConfig(void);

  // Quick tests
  bool IsSCT(void) const;
  bool IsQHY268M(void) const;
  bool IsST9(void) const;
  bool IsST10(void) const;
  bool IsAP1200(void) const;
  
  // Full Queries
  double EffectiveFocalLength(void) const;
  double FocalRatio(void) const;
  double PixelScale(void) const;	// arcseconds/pixel (unbinned)
  double FocusSlope(FocuserName which_focuser) const;	// focuser ticks/unit-change-in-focus
  std::string Telescope(void) const;
  std::string Camera(void) const;
  std::string Corrector(void) const;
  std::string Focuser(FocuserName which_focuser) const;
  bool IsMovingReducer(void) const; // true if reducer moves with the focuser
  int NumFocusers(void) const;
  std::string FineFocuserName(void) const;
  std::string CoarseFocuserName(void) const;

  double Latitude(void) const;	    // degrees
  double Longitude(void) const;     // degrees, negative if west of prime meridian
  double AverageSeeing(void) const; // arcseconds FWHM
  double MirrorShift(void) const;   // arcminutes, typical
  std::string ImageProfileFilename(void) const; // NOT full path; just filename
  int CFWPositions(void) const;			// zero means no CFW available
  std::string FixedFilter(void) const;		// valid only if CFWPositions() is zero
  double PixelSize(void) const;			// microns
  std::list<std::string> CFWFilters(void) const;
  double FocuserMin(FocuserName which_focuser) const;
  double FocuserMax(FocuserName which_focuser) const;
  double FocuserTickMicrons(FocuserName which_focuser) const;

private:
  void BuildSynonymList(void);
  std::unordered_map<std::string, std::string> synonyms;
  std::unordered_map<std::string, JSON_Expression *> data;
  JSON_Expression *al_exp {nullptr};
  JSON_Expression *FindByName(std::string config_name);
  bool RecursiveLoad(std::unordered_map<std::string, JSON_Expression *> *xref, std::string &config_name);
};

extern SystemConfig system_config;
  
#endif
