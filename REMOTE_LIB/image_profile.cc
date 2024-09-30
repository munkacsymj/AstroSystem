/*  image_profile.cc -- Implements JSON-based image profiles for use
 *  with exposure_flags 
 *
 *  Copyright (C) 2022 Mark J. Munkacsy

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
#include <json.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <list>
#include "image_profile.h"
#include <system_config.h>

int
ImageProfile::GetInt(const char *keyword) {
  ValueKeywordPair *pair = FindByKeyword(keyword);
  if (pair and not pair->value_is_string) return pair->int_val;
  fprintf(stderr, "ImageProfile::GetInt(%s): type mismatch.\n",
	  keyword);
  return -1; // Not a very good error return...
}

const char *
ImageProfile::GetChar(const char *keyword) {
  ValueKeywordPair *pair = FindByKeyword(keyword);
  if (pair and pair->value_is_string) return pair->string_val;
  fprintf(stderr, "ImageProfile::GetChar(%s): type mismatch.\n",
	  keyword);
  return nullptr; 
}

bool
ImageProfile::IsDefined(const char *keyword) {
  ValueKeywordPair *pair = FindByKeyword(keyword);
  return pair != nullptr;
}

ValueKeywordPair *
ImageProfile::FindByKeyword(const char *keyword) {
  for (auto x : keywords) {
    if (strcmp(x->keyword, keyword) == 0) return x;
  }
  return nullptr;
}

JSON_Expression *ParseImageProfiles(void) {
  std::string profile_filename(system_config.ImageProfileFilename());
  std::string profile_path("/home/ASTRO/CURRENT_DATA/" + profile_filename);
  int fd = open(profile_path.c_str(), O_RDONLY);
  if (fd < 0) {
    fprintf(stderr, "Unable to open %s\n", profile_path.c_str());
    exit(-1);
  }

  struct stat statbuf;
  if (stat(profile_path.c_str(), &statbuf)) {
    fprintf(stderr, "Unable to stat() image_profiles from %s\n",
	    profile_path.c_str());
    return nullptr;
  } else {
    char *profile_contents = (char *) malloc(statbuf.st_size+1);
    if (read(fd, profile_contents, statbuf.st_size) != statbuf.st_size) {
      fprintf(stderr, "Error reading image_profiles from %s\n",
	      profile_filename.c_str());
      exit(-1);
    }
    close(fd);

    profile_contents[statbuf.st_size]=0;
    const JSON_Expression *profiles = new JSON_Expression(profile_contents);

    JSON_Expression *tree = profiles->GetValue("profiles");
    if (not tree->IsList()) {
      fprintf(stderr, "image_profiles.json: profiles are not in form of a list.\n");
      exit(-1);
    }
    free(profile_contents);
    return tree;
  }
  /*NOTREACHED*/
}

ImageProfile::ImageProfile(const char *profile_name, JSON_Expression *tree) {
  if (tree == nullptr) {
    tree = ParseImageProfiles();
  }

  // At this point, "tree" is an expression list (the assignment value
  // for "profiles").
  const JSON_Expression *match = nullptr;
  // Each "p" is a sequence.
  for (auto p : tree->Value_list()) {
    const JSON_Expression *name_expr = p->Value("name");
    if (name_expr->IsString() and
	strcmp(name_expr->Value_char(), profile_name) == 0) {
      match = p;
      break;
    }
  }
  if (match == nullptr) {
    fprintf(stderr, "ImageProfile: No profile found with name == %s\n",
	    profile_name);
    exit(-1);
  }
  // Check for an "include" using keyword "base".
  // "match" is a sequence with two or three assignments ("name",
  // "content", and "base")
  const JSON_Expression *base_expr = match->Value("base");
  if (base_expr) {
    ImageProfile base_profile(base_expr->Value_string().c_str(), tree);
    keywords = base_profile.keywords;
  }
  const JSON_Expression *content = match->Value("content");
  if (content and content->IsSeq()) {
    const std::list<std::string> flag_keywords { "offset",
					    "gain",
					    "mode",
					    "binning",
					    "compress",
					    "usb_traffic",
					    "format",
					    "box_bottom",
					    "box_top",
					    "box_left",
					    "box_right" };
    for (auto keyword : flag_keywords) {
      const JSON_Expression *this_value = content->Value(keyword.c_str());
      if (this_value) {
	ValueKeywordPair *this_pair = FindByKeyword(keyword.c_str());
	if (this_pair == nullptr) {
	  this_pair = new ValueKeywordPair;
	  keywords.push_back(this_pair);
	  this_pair->keyword = strdup(keyword.c_str());
	}
	this_pair->value_is_string = this_value->IsString();
	if (this_pair->value_is_string) {
	  this_pair->string_val = strdup(this_value->Value_string().c_str());
	} else {
	  this_pair->int_val = this_value->Value_int();
	}
      }
    }
  } else {
    fprintf(stderr, "Invalid or missing content in profile %s\n",
	    profile_name);
    exit(-1);
  }
  profile_valid = true;
}
  
const std::list<std::string> &GetImageProfileNames(void) {
  static std::list<std::string> all_names;

  all_names.clear();
  JSON_Expression *tree = ParseImageProfiles();
  // tree points to a JSON list
  for (auto p : tree->Value_list()) {
    const JSON_Expression *name_expr = p->Value("name");
    if (name_expr->IsString()) {
      all_names.push_back(name_expr->Value_string());
    } else {
      fprintf(stderr, "image_profile:: profile without a name??\n");
    }
  }
  return all_names;
}

struct param_data {
  const char *param_name;
  bool integer_found {false};
  bool string_found {false};
};
  
static std::list<param_data *> param_dictionary;

static param_data *FindByName(const char *param_name) {
  for (auto x : param_dictionary) {
    if (strcmp(param_name, x->param_name) == 0) return x;
  }
  return nullptr;
}

void PrintImageProfiles(FILE *fp) {
  const std::list<std::string> &profile_names = GetImageProfileNames();

  // First pass through profiles to create param_dictionary
  param_dictionary.clear();
  for (auto profile : profile_names) {
    ImageProfile p(profile.c_str());

    //fprintf(stderr, "Profile %s has %ld keywords.\n",
    //	    profile.c_str(), p.keywords.size());
    for (auto k : p.keywords) {
      param_data *param = FindByName(k->keyword);
      if (param == nullptr) {
	param = new param_data;
	param->param_name = k->keyword;
	param_dictionary.push_back(param);
      }
      if (k->value_is_string) {
	param->string_found = true;
      } else {
	param->integer_found = true;
      }
    }
    //fprintf(stderr, "Dictionary now has %ld entries.\n",
    //	    param_dictionary.size());
  }

  // Now print
  fprintf(fp, "%12s ", "");
  for (auto param : param_dictionary) {
    fprintf(fp, "%7s ", param->param_name);
  }
  fprintf(fp, "\n");

  for (auto profile : profile_names) {
    ImageProfile p(profile.c_str());
    fprintf(fp, "%12s ", profile.c_str());
    for (auto param : param_dictionary) {
      ValueKeywordPair *value = p.FindByKeyword(param->param_name);
      if (not value) {
	fprintf(fp, "%7s ", "");
      } else if(value->value_is_string) {
	fprintf(fp, "%7s ", value->string_val);
      } else {
	fprintf(fp, "%7d ", value->int_val);
      }
    }
    fprintf(fp, "\n");
  }
}

  
    
  
