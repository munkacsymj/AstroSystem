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
  void Update(void);		// The *only* way to update the config file

  // Quick tests
  bool IsSCT(void) const;
  bool IsQHY268M(void) const;
  bool IsST9(void) const;
  
  // Full Queries
  double EffectiveFocalLength(void) const;
  double FocalRatio(void) const;
  double PixelScale(void) const;	// arcseconds/pixel (unbinned)
  double FocusSlope(void) const;	// focuser ticks/unit-change-in-focus
  SC_Optical_Configuration GetOpticalConfiguration(void) const;
  std::string Telescope(void) const;
  std::string Camera(void) const;
  std::string Corrector(void) const;
  std::string Focuser(void) const;
  int NumFocusers(void) const;
  std::string FineFocuserName(void) const;
  std::string CoarseFocuserName(void) const;

  // Allowable entries
  std::list<std::string> &TelescopeChoices(void);
  std::list<std::string> &CameraChoices(void);
  std::list<std::string> &FocuserChoices(void);
  std::list<std::string> &CorrectorChoices(void);

  // Update values (return true if success)
  bool SetEffectiveFocalLength(double efl);
  bool SetTelescope(std::string telescope);
  bool SetCamera(std::string camera);
  bool SetFocuser(std::string focuser);
  bool SetCorrector(std::string corrector);
  bool SetFocusSlope(double slope);
  bool SetPixelScale(double pixel_scale);
  bool SetFocalRatio(double fratio);

private:
  std::unordered_map<std::string, std::string> data;
  bool SetString(std::string keyword, std::string value);
  std::list<std::string> &GetChoices(std::string keyword);

  bool RefreshUsingDefinedConfigs(void);
};
  
#endif
