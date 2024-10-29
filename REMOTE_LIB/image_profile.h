/* This may look like C code, but it is really -*-c++-*- */
/*  image_profile.h -- Initializes exposure_flags from profiles kept
 *                     in /home/ASTRO/CURRENT_DATA/image_profiles.json
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
#ifndef _IMAGE_PROFILE_H
#define _IMAGE_PROFILE_H

#include <stdio.h>
#include <json.h>
#include <list>
#include <string>

struct ValueKeywordPair {
  const char *keyword;
  bool value_is_string;
  bool value_is_double;
  const char *string_val;
  int int_val;
  double double_val;
};

class ImageProfile {
public:
  ImageProfile(const char *profile_name, JSON_Expression *tree = nullptr);

  bool IsDefined(const char *keyword);
  int GetInt(const char *keyword);
  double GetDouble(const char *keyword);
  const char *GetChar(const char *keyword);

private:
  std::list<ValueKeywordPair *> keywords;
  bool profile_valid {false};
  ValueKeywordPair *FindByKeyword(const char *keyword);

  friend
    void PrintImageProfiles(FILE *fp);
};

const std::list<std::string> &GetImageProfileNames(void);
JSON_Expression *ParseImageProfiles(void);
void PrintImageProfiles(FILE *fp);

#endif

