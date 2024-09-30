/*  system_config.cc -- manage a database of telescope configuration data
 *
 *  Copyright (C) 2021, 2024 Mark J. Munkacsy

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

const char *CONFIG_FILE = "/home/ASTRO/CURRENT_DATA/system_config.json";
// extern for other people to use
SystemConfig system_config;

void
SystemConfig::BuildSynonymList(void) {
  JSON_Expression *syn_exp = this->al_exp->Value("synonyms");
  this->synonyms.clear();
  if (syn_exp) {
    for (JSON_Expression *exp : syn_exp->Value_seq()) {
      std::string root_name(exp->Assignment_variable());
      JSON_Expression *target_list = exp->GetAssignmentPtr();
      if (not target_list->IsList()) {
	std::cerr << "ERROR: Bad synonym syntax for " << root_name << std::endl;
	throw std::runtime_error("Invalid system_config.json file");
      }
      for (JSON_Expression *syn_word : target_list->Value_list()) {
	if (not syn_word->IsString()) {
	  std::cerr << "ERROR: Bad synonym for " << root_name << std::endl;
	  throw std::runtime_error("Invalid system_config.json file");
	}
	this->synonyms[syn_word->Value_string()] = root_name;
      }
    }
  }
  //std::cerr << "Synonyms:\n";
  //for (auto x : this->synonyms) {
  //std::cerr << "    " << x.first << " => " << x.second << std::endl;
  //}
}
	  
bool
SystemConfig::RecursiveLoad(std::unordered_map<std::string, JSON_Expression *> *xref, std::string &config_param) {
  //std::cerr << "RecursiveLoad called for " << config_name << std::endl;

  std::string &config_name (config_param);
  if (this->synonyms.find(config_param) != this->synonyms.end()) {
    config_name = this->synonyms[config_param];
  }
  //std::cerr << "RecursiveLoad() searching for " << config_name << '\n';
  if (xref->find(config_name) == xref->end()) {
    std::cerr << "ERROR: 'current' configuration not found in system_config.json\n";
    throw std::runtime_error("Invalid system_config.json file");
  }

  JSON_Expression *subconfig = (*xref)[config_name];
  // Look for (and handle) next level of includes...
  JSON_Expression *includes = subconfig->FindAssignment("include");
  if (includes) {
    // Yes, there is another level. Is this a list or a single name
    includes = includes->GetAssignmentPtr(); // get the value of the assignment (the RHS)
    if (includes->IsList()) {
      // It's a list of includes
      for (JSON_Expression *sub : includes->Value_list()) {
	std::string include_name(sub->Value_string());
	bool loaded_okay = this->RecursiveLoad(xref, include_name);
	if (not loaded_okay) return false;
      }
    } else {
      // It's a single include
      std::string include_name(includes->Value_string());
      bool loaded_okay = this->RecursiveLoad(xref, include_name);
      if (not loaded_okay) return false;
    }
  }
  
  // Now load all the values in this entry and return...
  for (JSON_Expression *item : subconfig->Value_seq()) {
    const char *assign_var = item->Assignment_variable();
    if (strcmp(assign_var, "config_name") == 0) continue;
    if (strcmp(assign_var, "include") == 0) continue;
    this->data[std::string(assign_var)] = item->GetAssignmentPtr();
  }
  return true; // success
}
    
SystemConfig::SystemConfig(void) {
  this->al_exp = new JSON_Expression();
  this->al_exp->SyncWithFile(CONFIG_FILE, JSON_READONLY);

  // at the top level, look for an assignment named "current"
  JSON_Expression *current = this->al_exp->Value("current");
  if (!current) {
    std::cerr << "ERROR: Cannot find entry named 'current' in system_config.json\n";
    throw std::runtime_error("Invalid system_config.json file");
  }

  std::string current_configname(current->Value_string());

  // There must be a configuration present with that name
  JSON_Expression *configs = this->al_exp->Value("configurations");
  if (not configs->IsList()) {
    std::cerr << "ERROR: Configurations entry isn't a list in system_config.json\n";
    throw std::runtime_error("Invalid system_config.json file");
  }
  std::unordered_map<std::string, JSON_Expression *> xref;
  // each item in the list of configs turns into an item in the xref lookup
  for (JSON_Expression *x : configs->Value_list()) {
    // find the name of this sub-configuration
    JSON_Expression *subname_exp = x->GetValue("config_name");
    if (!subname_exp) {
      std::cerr << "ERROR: A configuration entry has no 'config_name' in system_config.json\n";
      throw std::runtime_error("Invalid system_config.json file");
    }
    //std::cerr << "Setting xref[" << subname_exp->Value_string() << "]\n";
    xref[subname_exp->Value_string()] = x;
  }

  this->BuildSynonymList();
  (void) this->RecursiveLoad(&xref, current_configname);
}

SystemConfig::~SystemConfig(void) {
  synonyms.clear();
  data.clear();
}

double
SystemConfig::Latitude(void) const {	    // degrees
  auto latitude = data.find("latitude");
  if (latitude == data.end()) {
    std::cerr << "ERROR: no latitude in system_config.json\n";
    throw std::runtime_error("Invalid system_config.json file");
  }
  return latitude->second->Value_double();
}

double
SystemConfig::Longitude(void) const {       // degrees, negative if west of prime meridian
  auto longitude = data.find("longitude");
  if (longitude == data.end()) {
    std::cerr << "ERROR: no longitude in system_config.json\n";
    throw std::runtime_error("Invalid system_config.json file");
  }
  return longitude->second->Value_double();
}

double
SystemConfig::AverageSeeing(void) const {   // arcseconds FWHM
  auto seeing = data.find("seeing");
  if (seeing == data.end()) {
    std::cerr << "ERROR: no seeing in system_config.json\n";
    throw std::runtime_error("Invalid system_config.json file");
  }
  return seeing->second->Value_double();
}

double
SystemConfig::MirrorShift(void) const {     // arcminutes, typical
  auto mirror_shift = data.find("mirror_shift");
  if (mirror_shift == data.end()) {
    std::cerr << "ERROR: no mirror_shift in system_config.json\n";
    throw std::runtime_error("Invalid system_config.json file");
  }
  return mirror_shift->second->Value_double();
}

std::string
SystemConfig::ImageProfileFilename(void) const { // NOT full path; just filename
  auto image_prof = data.find("imageprofile");
  if (image_prof == data.end()) {
    std::cerr << "ERROR: no imageprofile in system_config.json\n";
    throw std::runtime_error("Invalid system_config.json file");
  }
  return image_prof->second->Value_string();
}

int
SystemConfig::CFWPositions(void) const {    // zero means no CFW available
  auto filter_count = data.find("cfw_positions");
  if (filter_count == data.end()) {
    std::cerr << "ERROR: no cfw_positions in system_config.json\n";
    throw std::runtime_error("Invalid system_config.json file");
  }
  return filter_count->second->Value_double();
}

std::string
SystemConfig::FixedFilter(void) const {	    // valid only if CFWPositions() is zero
  auto fixed_filter = data.find("fixed_filter");
  if (fixed_filter == data.end()) {
    std::cerr << "ERROR: no fixed_filter in system_config.json\n";
    throw std::runtime_error("Invalid system_config.json file");
  }
  return fixed_filter->second->Value_string();
}

int
SystemConfig::NumFocusers(void) const {
  auto num_focusers = data.find("numfocusers");
  if (num_focusers == data.end()) return 1;
  return num_focusers->second->Value_int();
}

std::string
SystemConfig::FineFocuserName(void) const {
  if (NumFocusers() > 1) return Focuser(FOCUSER_FINE);
  auto fine_focuser = data.find("finefocuser");
  if (fine_focuser == data.end()) return "";
  return fine_focuser->second->Value_string();
}

std::string
SystemConfig::CoarseFocuserName(void) const {
  if (NumFocusers() > 1) return Focuser(FOCUSER_COARSE);
  auto coarse_focuser = data.find("coarsefocuser");
  if (coarse_focuser == data.end()) return "";
  return coarse_focuser->second->Value_string();
}

double
SystemConfig::PixelSize(void) const {	    // microns
  auto pixel_size = data.find("pixel_size");
  if (pixel_size == data.end()) {
    std::cerr << "ERROR: no pixel_size in system_config.json\n";
    throw std::runtime_error("Invalid system_config.json file");
  }
  return pixel_size->second->Value_double();
}

std::list<std::string>
SystemConfig::CFWFilters(void) const {
  auto full_list = data.find("filters");
  std::list<std::string> ret_value;
  if (full_list != data.end()) {
    for (auto x : full_list->second->Value_list()) {
      ret_value.push_back(x->Value_string());
    }
  }
  return ret_value;
}

double
SystemConfig::FocuserMin(FocuserName which_focuser) const {
  auto min_travel = data.find("min_travel_fine");
  if (which_focuser == FOCUSER_FINE or
      which_focuser == FOCUSER_DEFAULT) {
    if (min_travel == data.end()) {
      min_travel = data.find("min_travel");
    }
    if (min_travel == data.end()) {
      std::cerr << "ERROR: no min_travel_fine in system_config.json\n";
      throw std::runtime_error("Invalid system_config.json file");
    }
  } else {
    min_travel = data.find("min_travel_coarse");
    if (min_travel == data.end()) {
      std::cerr << "ERROR: no min_travel_coarse in system_config.json\n";
      throw std::runtime_error("Invalid system_config.json file");
    }
  }
  return min_travel->second->Value_double();
}

double
SystemConfig::FocuserMax(FocuserName which_focuser) const {
  auto max_travel = data.find("max_travel_fine");
  if (which_focuser == FOCUSER_FINE or
      which_focuser == FOCUSER_DEFAULT) {
    if (max_travel == data.end()) {
      max_travel = data.find("max_travel");
    }
    if (max_travel == data.end()) {
      std::cerr << "ERROR: no max_travel_fine in system_config.json\n";
      throw std::runtime_error("Invalid system_config.json file");
    }
  } else {
    max_travel = data.find("max_travel_coarse");
    if (max_travel == data.end()) {
      std::cerr << "ERROR: no max_travel_coarse in system_config.json\n";
      throw std::runtime_error("Invalid system_config.json file");
    }
  }
  return max_travel->second->Value_double();
}

double
SystemConfig::FocuserTickMicrons(FocuserName which_focuser) const {
  auto focuser_tick = data.find("focuser_tick_fine");
  if (which_focuser == FOCUSER_FINE or
      which_focuser == FOCUSER_DEFAULT) {
    if (focuser_tick == data.end()) {
      focuser_tick = data.find("focuser_tick");
    }
    if (focuser_tick == data.end()) {
      std::cerr << "ERROR: no focuser_tick_fine in system_config.json\n";
      throw std::runtime_error("Invalid system_config.json file");
    }
  } else {
    focuser_tick = data.find("focuser_tick_coarse");
    if (focuser_tick == data.end()) {
      std::cerr << "ERROR: no focuser_tick_coarse in system_config.json\n";
      throw std::runtime_error("Invalid system_config.json file");
    }
  }
  return focuser_tick->second->Value_double();
}

bool
SystemConfig::IsSCT(void) const {
  std::string telescope = Telescope();
  return (telescope == "10INCH_SCT");
}

bool
SystemConfig::IsAP1200(void) const {
  auto mount = data.find("mount");
  if (mount == data.end()) return false;
  return mount->second->Value_string() == "AP1200";
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

bool
SystemConfig::IsST10(void) const {
  std::string camera = Camera();
  return (camera == "ST10" or
	  camera == "ST-10XME");
}
  
std::string 
SystemConfig::Telescope(void) const {
  auto telescope = data.find("telescope");
  if (telescope == data.end()) return "";
  return telescope->second->Value_string();
}

std::string 
SystemConfig::Camera(void) const {
  auto camera = data.find("camera");
  if (camera == data.end()) return "";
  return camera->second->Value_string();
}

std::string 
SystemConfig::Corrector(void) const {
  auto corrector = data.find("corrector");
  if (corrector == data.end()) return "";
  return corrector->second->Value_string();
}

std::string 
SystemConfig::Focuser(FocuserName which_focuser) const {
  auto focuser = data.find("focuser");
  if (which_focuser == FOCUSER_FINE or
      which_focuser == FOCUSER_DEFAULT) {
    if (focuser == data.end()) {
      focuser = data.find("focuser_fine");
    }
  } else {
    focuser = data.find("focuser_coarse");
  }
  if (focuser == data.end()) return "";
  return focuser->second->Value_string();
}

double
SystemConfig::EffectiveFocalLength(void) const {
  auto efl = data.find("efl");
  if (efl == data.end()) return NAN;
  return efl->second->Value_double();
}

double
SystemConfig::FocalRatio(void) const {
  auto fr = data.find("fratio");
  if (fr == data.end()) return NAN;
  return fr->second->Value_double();
}

double
SystemConfig::PixelScale(void) const {
  auto ps = data.find("pixelscale");
  if (ps == data.end()) return NAN;
  return ps->second->Value_double();
}

double
SystemConfig::FocusSlope(FocuserName which_focuser) const {
  auto fs = data.find("focusslope");
  if (which_focuser == FOCUSER_FINE or
      which_focuser == FOCUSER_DEFAULT) {
    if (fs == data.end()) {
      fs = data.find("focusslope_fine");
    }
  } else {
    fs = data.find("focusslope_coarse");
  }
  if (fs == data.end()) return NAN;
  return fs->second->Value_double();
}

