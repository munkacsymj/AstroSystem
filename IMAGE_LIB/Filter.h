/* This may look like C code, but it is really -*-c++-*- */
/*  Filter.h -- Manage photometric filters
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
#ifndef _FILTER_H
#define _FILTER_H

#include <vector>

class Filter {
public:
  Filter(void) 
  : filter_ID(6) {
    ; }
  Filter(const char *filter_name);
  ~Filter(void) {;}

  bool operator==(Filter f) const { return f.filter_ID == filter_ID; }
  bool operator!=(Filter f) const { return f.filter_ID != filter_ID; }

  // These are the allowable values of "app" in AppName()
#define FILTER_APP_Canonical 0
#define FILTER_APP_Filename  1
#define FILTER_APP_1char     2
#define FILTER_APP_2char     3
#define FILTER_APP_original  4
#define FILTER_APP_AAVSO     5
  
  const char *AppName(int app) const;

  const char *NameOf(void) const;
  const char *CanonicalNameOf(void) const { return AppName(FILTER_APP_Canonical); }

  // This returns the position of this filter on the filter wheel. The
  // first position is position '0'. If the filter isn't loaded, then
  // -1 will be returned.
  int PositionOf(void);
  
  int FlagWordValue(void) const;

  //friend inline int operator==(const Filter f1, const Filter f2);

  // Although the following two functions are public, please don't use
  // them except in REMOTE_LIB when sending messages between computers
  // that talk about filters.
  inline int FilterIDIndex(void) const { return filter_ID; }
  inline void SetFilterIDIndex(int ID) { filter_ID = ID; }

  // The AAVSO_FilterName is the one-character abbreviation that AAVSO
  // uses on its web site (B, V, R, I)
  const char *AAVSO_FilterName(void) const;

private:
  int filter_ID;
};

// The filter specified by "f" will become the default filter to be
// used in future expose() commands.
void SetDefaultFilter(const Filter *f);

// Returns "1" on success, returns "0" if there is no default
// filter. If a default filter exists, will be stored into "f". 
int GetDefaultFilter(Filter &f);

// Return list of filter names, separated by \n.
const char *AllDefinedFilterNames(void);

// Sets size of the filter wheel
void SetCFWSize(int n); // number of filter slots in the wheel
// Position-counting starts with '0'
void SetCFWFilter(int n, Filter &filter);
// Fetch number of filters in the CFW
int FilterWheelSlots(void);

std::vector<Filter> &InstalledFilters(void);

#endif
