/*  system_config.cc -- manage a database of telescope configuration data
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

#include <fstream>
#include <iostream>
#include <stdio.h>
#include <string.h>
#include <math.h>		// NAN

#include "system_config.h"

const char *CONFIG_FILE = "/home/ASTRO/CURRENT_DATA/system_config.txt";

std::unordered_map<std::string, std::list<std::string>> definitions
  {
   { "CAMERA", { "ST9", "QHY268M" }},
   { "TELESCOPE", { "10INCH_SCT" , "C14" }},
   { "FOCUSER", { "EV2" }},
   { "CORRECTOR", { "None", "C6.3x", "Starizona" }},
  };

struct DefConfig {
  std::string camera;
  std::string telescope;
  std::string corrector;

  double efl;
  double f_number;
  double pixel_scale;
  double focus_slope;
  SC_Optical_Configuration std_config;
};

std::list<DefConfig> DefinedConfigurations
  {{ "ST9", "10INCH_SCT", "None", 2540.0, 10.0, 1.52, 64.0, SC_ST9_Meade10 },
   { "QHY268M", "10INCH_SCT", "None", 2540.0, 10.0, 0.29, 32.0, SC_268M_Meade10 },
   { "ST9", "C14", "C6.3x", 77*25.4, 77.0/14.0, 1.97, 32.0, SC_ST9_C14_C63x },
   { "ST9", "C14", "Starizona", 88*25.4, 88/14.0, 1.81, 146.0, SC_ST9_C14_Starizona },
   { "QHY268M", "C14", "Starizona", 2316.0, 2316.0/(25.4*14.0), 1.01/3.0, 146.0, SC_268M_C14_Starizona },
  };

std::list<std::string> numeric_definitions
  { { "EFL" } };

SystemConfig::SystemConfig(void) {
  std::ifstream sc_file(CONFIG_FILE);
  if (sc_file.is_open()) {
    std::string line;
    while(std::getline(sc_file, line)) {
      if (line.size() > 62) {
	std::cerr << "SystemConfig: line too long in "
		  << CONFIG_FILE << std::endl;
      } else {
	char word1[64], word2[64];
	if (sscanf(line.c_str(), "%s %s", word1, word2) != 2) {
	  std::cerr << "SystemConfig: bad line: "
		    << line << std::endl;
	} else {
	  if (strcmp(word1, "EFL") == 0 or
	      strcmp(word1, "PIXELSCALE") == 0 or
	      strcmp(word1, "FOCUSSLOPE") == 0 or
	      strcmp(word1, "FRATIO") == 0) {
	    // good keyword
	    (void) data.insert({word1, word2});
	  } else if (definitions.find(word1) != definitions.end()) {
	    // good keyword
	    (void) data.insert({word1, word2});
	  } else {
	    std::cerr << "SystemConfig: bad keyword: "
		      << word1 << std::endl;
	  }
	}
      }
    }
    sc_file.close();
  } else {
    std::cerr << "Warning: Unable to open system config file "
	      << CONFIG_FILE << std::endl;
  }
  if (RefreshUsingDefinedConfigs()) Update();
}

void
SystemConfig::Update(void) {
  std::ofstream sc_file(CONFIG_FILE);
  if (sc_file.is_open()) {
    for (std::pair<std::string, std::string> v : data) {
      sc_file << v.first << ' ' << v.second << std::endl;
    }
    sc_file.close();
  } else {
    std::cerr << "Error: Unable to open/create system config file "
	      << CONFIG_FILE << std::endl;
  }
}

SystemConfig::~SystemConfig(void) {
  ; // nothing
}

bool
SystemConfig::IsSCT(void) const {
  std::string telescope = Telescope();
  return (telescope == "10INCH_SCT");
}

bool
SystemConfig::IsQHY268M(void) const {
  std::string camera = Camera();
  return (camera == "QHY268M");
}

bool
SystemConfig::IsST9(void) const {
  std::string camera = Camera();
  return (camera == "ST9");
}
  
std::string
SystemConfig::Telescope(void) const {
  auto telescope = data.find("TELESCOPE");
  if (telescope == data.end()) return "";
  return telescope->second;
}

std::string
SystemConfig::Camera(void) const {
  auto camera = data.find("CAMERA");
  if (camera == data.end()) return "";
  return camera->second;
}

std::string
SystemConfig::Corrector(void) const {
  auto corrector = data.find("CORRECTOR");
  if (corrector == data.end()) return "";
  return corrector->second;
}

std::string
SystemConfig::Focuser(void) const {
  auto focuser = data.find("FOCUSER");
  if (focuser == data.end()) return "";
  return focuser->second;
}

int
SystemConfig::NumFocusers(void) const {
  auto num_focusers = data.find("NUMFOCUSERS");
  if (num_focusers == data.end()) return 1;
  return std::stod(num_focusers->second);
}

std::string
SystemConfig::FineFocuserName(void) const {
  if (NumFocusers() > 1) return Focuser();
  auto fine_focuser = data.find("FINEFOCUSER");
  if (fine_focuser == data.end()) return "";
  return fine_focuser->second;
}

std::string
SystemConfig::CoarseFocuserName(void) const {
  if (NumFocusers() > 1) return Focuser();
  auto coarse_focuser = data.find("COARSEFOCUSER");
  if (coarse_focuser == data.end()) return "";
  return coarse_focuser->second;
}

double
SystemConfig::EffectiveFocalLength(void) const {
  auto efl = data.find("EFL");
  if (efl == data.end()) return NAN;
  return std::stod(efl->second);
}

double
SystemConfig::FocalRatio(void) const {
  auto fr = data.find("FRATIO");
  if (fr == data.end()) return NAN;
  return std::stod(fr->second);
}

double
SystemConfig::PixelScale(void) const {
  auto ps = data.find("PIXELSCALE");
  if (ps == data.end()) return NAN;
  return std::stod(ps->second);
}

double
SystemConfig::FocusSlope(void) const {
  auto fs = data.find("FOCUSSLOPE");
  if (fs == data.end()) return NAN;
  return std::stod(fs->second);
}

bool
SystemConfig::SetString(std::string keyword, std::string value) {
  auto x = data.find(keyword);
  if (x == data.end()) {
    data.insert({keyword, value});
  } else {
    data.at(keyword) = value;
  }
  return true;
}

bool
SystemConfig::SetEffectiveFocalLength(double efl) {
  char buffer[32];
  sprintf(buffer, "%lf", efl);
  SetString("EFL", std::string(buffer));
  if (RefreshUsingDefinedConfigs()) Update();
  return true;
}

bool
SystemConfig::SetFocusSlope(double fs) {
  char buffer[32];
  sprintf(buffer, "%lf", fs);
  SetString("FOCUSSLOPE", std::string(buffer));
  if (RefreshUsingDefinedConfigs()) Update();
  return true;
}

bool
SystemConfig::SetPixelScale(double pixel_scale) {
  char buffer[32];
  sprintf(buffer, "%lf", pixel_scale);
  SetString("PIXELSCALE", std::string(buffer));
  if (RefreshUsingDefinedConfigs()) Update();
  return true;
}

bool
SystemConfig::SetTelescope(std::string telescope) {
  SetString("TELESCOPE", telescope);
  if (RefreshUsingDefinedConfigs()) Update();
  return true;
}

bool
SystemConfig::SetCamera(std::string camera) {
  SetString("CAMERA", camera);
  if (RefreshUsingDefinedConfigs()) Update();
  return true;
}

bool
SystemConfig::SetFocuser(std::string focuser) {
  SetString("FOCUSER", focuser);
  if (RefreshUsingDefinedConfigs()) Update();
  return true;
}

bool
SystemConfig::SetFocalRatio(double fratio) {
  char buffer[32];
  sprintf(buffer, "%lf", fratio);
  SetString("FRATIO", std::string(buffer));
  if (RefreshUsingDefinedConfigs()) Update();
  return true;
}

bool
SystemConfig::SetCorrector(std::string corrector) {
  SetString("CORRECTOR", corrector);
  if (RefreshUsingDefinedConfigs()) Update();
  return true;
}

static std::list<std::string> invalid_list { "INVALID KEYWORD" };

std::list<std::string>
&SystemConfig::GetChoices(std::string keyword) {
  auto x = definitions.find(keyword);
  if (x == definitions.end()) {
    return invalid_list;
  } else {
    return x->second;
  }
}

std::list<std::string> &
SystemConfig::TelescopeChoices(void) {
  return GetChoices("TELESCOPE");
}

std::list<std::string> &
SystemConfig::CameraChoices(void) {
  return GetChoices("CAMERA");
}

std::list<std::string> &
SystemConfig::FocuserChoices(void) {
  return GetChoices("FOCUSER");
}

std::list<std::string> &
SystemConfig::CorrectorChoices(void) {
  return GetChoices("CORRECTOR");
}

// returns true if anything changed
bool
SystemConfig::RefreshUsingDefinedConfigs(void) {
#if 0
  std::string telescope = Telescope();
  std::string camera = Camera();
  std::string corrector = Corrector();
  
  for (auto x : DefinedConfigurations) {
    if (x.camera == camera and
	x.telescope == telescope and
	x.corrector == corrector) {
      char buffer[32];

      sprintf(buffer, "%lf", x.efl);
      SetString("EFL", std::string(buffer));
      sprintf(buffer, "%lf", x.focus_slope);
      SetString("FOCUSSLOPE", std::string(buffer));
      sprintf(buffer, "%lf", x.f_number);
      SetString("FRATIO", std::string(buffer));
      sprintf(buffer, "%lf", x.pixel_scale);
      SetString("PIXELSCALE", std::string(buffer));
      return true;
    }
  }
  return false;
#else
  return true;
#endif
}

SC_Optical_Configuration
SystemConfig::GetOpticalConfiguration(void) const {
  std::string telescope = Telescope();
  std::string camera = Camera();
  std::string corrector = Corrector();
  
  for (auto x : DefinedConfigurations) {
    if (x.camera == camera and
	x.telescope == telescope and
	x.corrector == corrector) {
      return x.std_config;
    }
  }
  return SC_NONSTANDARD;
}
