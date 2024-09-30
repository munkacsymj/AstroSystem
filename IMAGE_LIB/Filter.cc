/*  Filter.cc -- Manage photometric filters
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
#include <string.h>		// strlen(), strcmp()
#include <stdio.h>
#include <ctype.h>		// toupper()
#include <gendefs.h>		// FILTER_DEFAULT_FILE
#include <list>
#include <vector>
#include <string>
#include "Filter.h"

// Warning! Filter information is stored by the camera server in a
// file that uses the integer values in the following definitions. If
// these definitions are changed,
// ******** the camera server filter data file must be deleted ********
// and rebuilt.  As new filters are received and put into use, add new
// entries to this list, but DO NOT delete old filters that are no
// longer in use. It does no harm to have these numbers run up into
// the hundreds with many gaps. (It just means that the filters[]
// array will have "unused" entries for each of the retired filters.) 
#define FILTER_Invalid 0
#define FILTER_Rc      1
#define FILTER_Bc      2
#define FILTER_B       2
#define FILTER_Uc      3
#define FILTER_U       3
#define FILTER_Ic      4
#define FILTER_Vc      5
#define FILTER_V       5
#define FILTER_Clear   6
#define FILTER_None    7
#define FILTER_Dark    8
#define FILTER_CBB     9 // clear, blue-blocking, exoplanet
#define FILTER_SA200  10 

#define NUM_FILTERS ((int)(sizeof(filters)/sizeof(filters[0])))

struct filter_info {
  const char *filter_name;
  const char *uc_filter_name;
  int filter_position;	
  int filter_flag;		// values for flag_word in camera_api.h
};

// The first letters of these filter names must be unique, since only
// the first letter is sent to jellybean in the expose message.
static filter_info filters[] = {
  { "Invalid", "INVALID", -1, 0x00 }, // id_index = 0
  { "Rc", "RC", -1, 0x01 },	     // id_index = 1
  { "Bc", "BC", -1, 0x02 },	     // id_index = 2
  { "Uc", "UC", -1, 0x03 },	     // id_index = 3
  { "Ic", "IC", -1, 0x04 },	     // id_index = 4
  { "Vc", "VC", -1, 0x05 },	     // id_index = 5
  { "Clear", "CLEAR", -1, 0x06 },     // id_index = 6
  { "None", "NONE", -1, 0x07 },	     // id_index = 7
  { "Dark", "DARK", -1, 0x08 },       // id_index = 8
  { "X", "X", -1, 0x09 },	     // id_index = 9
  { "SA200", "SA200", -1, 0x0a },    // id_index = 10
};

struct FilterNameXref {
  const char *synonym;
  int filter_number;
};
std::list<FilterNameXref> synonyms
  { { "Invalid", FILTER_Invalid },
    { "Rc", FILTER_Rc },
    { "R", FILTER_Rc },
    { "Bc", FILTER_Bc },
    { "B", FILTER_Bc },
    { "Ic", FILTER_Ic },
    { "I", FILTER_Ic },
    { "Vc", FILTER_Vc },
    { "V", FILTER_Vc },
    { "Dark", FILTER_Dark },
    { "D", FILTER_Dark },
    { "Clear", FILTER_Clear },
    { "X", FILTER_CBB },
    { "CBB", FILTER_CBB },
    { "SA200", FILTER_SA200 },
    { "N", FILTER_None },
    { "None", FILTER_None },
  };

std::vector<std::vector<std::string>> filter_app_names {
  // FILTER_APP_Canonical
  { "Invalid", "R", "B", "U", "I", "V", "Clear", "None", "Dark", "CBB", "SA200" },
  // FILTER_APP_Filename
  { "Invalid", "R", "B", "U", "I", "V", "Clear", "None", "N/A", "CBB", "SA200" },
  // FILTER_APP_1char
  { "0", "R", "B", "U", "I", "V", "C", "-", "D", "X", "S" },
  // FILTER_APP_2char
  { "00", "Rc", "Bc", "Uc", "Ic", "Vc", "CL", "--", "DD", "CB", "SA" },
  // FILTER_APP_original
  { "Invalid", "Rc", "Bc", "U", "Ic", "Vc", "Clear", "None", "N/A", "CBB", "N/A" },
  // FILTER_APP_AAVSO
  { "Invalid", "R", "B", "U", "I", "V", "CV", "CV", "N/A", "CV", "N/A" }
};

// Return list of filter names, separated by \n.
const char *AllDefinedFilterNames(void) {
  static char filter_names[NUM_FILTERS*8];
  filter_names[0] = 0;

  for(int n=0; n < NUM_FILTERS; n++) {
    strcat(filter_names, filters[n].filter_name);
    strcat(filter_names, "\n");
  }
  return filter_names;
}
  

int
Filter::FlagWordValue(void) const {
  if(filter_ID < 0 || filter_ID >= NUM_FILTERS) {
    fprintf(stderr,
	    "Filter::FlagWordValue: invalid filter_ID(%d)\n", filter_ID);
    return 0;
  } else {
    return filters[filter_ID].filter_flag;
  }
}
  

Filter::Filter(const char *filter_name) {
  const char *orig_filter_name = filter_name;
  char uc_f_name[80];
  if(filter_name == 0 || filter_name[0] == 0) {
    fprintf(stderr, "Filter: <nil> filter name\n");
    filter_ID = FILTER_None;
  }
  if(strlen(filter_name) >= sizeof(uc_f_name)) {
    fprintf(stderr, "Filter: bad filter name: %s\n", filter_name);
    filter_ID = FILTER_None;
  } else {
    char *d = uc_f_name;
    const char *s = filter_name;
    do {
      *d++ = toupper(*s);
    } while (*s++);

    for (auto x : synonyms) {
      if (strcmp(x.synonym, filter_name) == 0) {
	filter_ID = x.filter_number;
	return;
      }
    }
    // first try for exact match
    for(int n=0; n < NUM_FILTERS; n++) {
      if (strcmp(filters[n].uc_filter_name, uc_f_name) == 0) {
	filter_ID = n;
	return;
      }
    }
    
    // then settle for first character match
    for(int n=0; n < NUM_FILTERS; n++) {
      if (filters[n].uc_filter_name[0] == uc_f_name[0]) { // only check first letter
	filter_ID = n;
	return;
      }
    }

    fprintf(stderr, "Filter: filter name '%s' unrecognized\n",
	    orig_filter_name);
    filter_ID = FILTER_None;
  }
}

const char *
Filter::NameOf(void) const {
  if(filter_ID < 0 || filter_ID >= NUM_FILTERS) {
    fprintf(stderr, "Filter::NameOf: invalid filter_ID(%d)\n", filter_ID);
    return "";
  } else {
    return filters[filter_ID].filter_name;
  }
}

//inline int operator==(const Filter f1, const Filter f2) {
//return f1.filter_ID == f2.filter_ID;
//}

////////////////////////////////////////////////////////////////
//        Default Filter
////////////////////////////////////////////////////////////////

// The filter specified by "f" will become the default filter to be
// used in future expose() commands.
void SetDefaultFilter(const Filter *f) {
  int valid = 0;
  const char *f_name = "";
  // verify the filter and the filter name
  if (f) {
    f_name = f->NameOf();
    for(int n=0; n < NUM_FILTERS; n++) {
      if(strcmp(filters[n].filter_name, f_name) == 0) {
	valid = 1;
	break;
      }
    }
  }

  if (valid) {
    FILE *fp = fopen(FILTER_DEFAULT_FILE, "w");
    if (!fp) {
      fprintf(stderr, "Filter.cc: Cannot create default filter file: %s\n",
	      FILTER_DEFAULT_FILE);
    } else {
      fprintf(fp, "%s", f_name);
      fclose(fp);
    }
  } else {
    fprintf(stderr, "Filter.cc: Invalid default filter name: %s\n", f_name);
  }
}
    
// Returns "1" on success, returns "0" if there is no default
// filter. If a default filter exists, will be stored into "f". 
int GetDefaultFilter(Filter &f) {
  FILE *fp = fopen(FILTER_DEFAULT_FILE, "r");
  // If there is no default filter file, then return a zero and make
  // no change to "f"
  if (!fp) {
    return 0;
  }

  char f_name[64];
  int valid = 0; // later on, will be used as the function's return value
  if (fgets(f_name, sizeof(f_name), fp)) {
    // successful read
    for(int n=0; n < NUM_FILTERS; n++) {
      if(strcmp(filters[n].filter_name, f_name) == 0) {
	valid = 1;
	break;
      }
    }
    if (valid) {
      f = Filter(f_name);
    }
  } else {
    // bad read from filter default file
    fprintf(stderr, "Filter.cc: Unable to read default filter file.\n");
  }
  fclose(fp);
  return valid;
}

const char *
Filter::AAVSO_FilterName(void) const {
  const char *local_filter_name = NameOf();
  if (strcmp(local_filter_name, "Vc") == 0) return "V";
  if (strcmp(local_filter_name, "Rc") == 0) return "R";
  if (strcmp(local_filter_name, "Ic") == 0) return "I";
  if (strcmp(local_filter_name, "Bc") == 0) return "B";
  if (strcmp(local_filter_name, "CBB") == 0) return "CBB";
  fprintf(stderr, "AAVSO_FilterName: unrecognized filter: %s\n",
	  local_filter_name);
  return "0";
}

const char *
Filter::AppName(int app) const {
  return filter_app_names[app][this->FilterIDIndex()].c_str();
}
  
/*
 * FILTER handling
 */
static int filter_info_available = 0;
static int num_filters = 0;
// the following array is indexed by filter wheel slot number. The
// array holds the filter color (using the integers defined in
// Filter.h)
#define FILTER_WHEEL_POS 8 /* for QHY CFW */
static Filter filter_slot_info[FILTER_WHEEL_POS];

//#define FILTER_FILE "/var/local/filter.data"
#define FILTER_FILE "/home/ASTRO/CURRENT_DATA/filter.data"

void SetCFWSize(int n) {
  if (n > FILTER_WHEEL_POS) {
    fprintf(stderr, "ERROR: Filter Wheel has too many positions.\n");
    return;
  }

  num_filters = n;
}

void WriteFilterData(void) {
  if(filter_info_available) {
    FILE *fp = fopen(FILTER_FILE, "w");
    if(!fp) {
      perror("Cannot open filter file");
    } else {
      if(num_filters > 10) {
	fprintf(stderr, "WriteFilterData: invalid num_filters = %d\n",
		num_filters);
	num_filters = 10;
      }
      fprintf(fp, "%d ", num_filters);
      int n;
      for(n=0; n<num_filters; n++) {
	fprintf(fp, "%s ", filter_slot_info[n].CanonicalNameOf());
      }
      while(n < FILTER_WHEEL_POS) {
	fprintf(fp, "N ");
	n++;
      }
      fprintf(fp, "\n");
      fclose(fp);
    }
  }
}

void ReadFilterData(void) {
  FILE *fp = fopen(FILTER_FILE, "r");
  if(!fp) {
    perror("Cannot find pre-existing filter file.");
    filter_info_available = 0;
    num_filters = 0;
  } else {
    {
      int fc = fscanf(fp, "%d", &num_filters);
      if (fc != 1) {
	fprintf(stderr, "ReadFilterData: invalid filter file info.\n");
	return;
      }
    }
    if(num_filters < 0 || num_filters > FILTER_WHEEL_POS) {
      fprintf(stderr, "ReadFilterData: invalid num_filters: %d\n",
	      num_filters);
      filter_info_available = 0;
    } else {
      int n;
      for(n = 0; n < num_filters; n++) {
	char this_filter[32];
	int fc = fscanf(fp, "%s", this_filter);
	if (fc != 1) {
	  fprintf(stderr, "ReadFilterData: unable to parse filter file.\n");
	  break;
	}
	Filter f(this_filter);
	filter_slot_info[n] = f;
	filters[f.FilterIDIndex()].filter_position = n;
      }
    }
    fclose(fp);
    filter_info_available = 1;
  }
}

// Position-counting starts with '0'
void SetCFWFilter(int n, Filter &filter) {
  if (not filter_info_available) ReadFilterData();

  if (n < 0) {
    fprintf(stderr, "SetCFWFilter(): invalid CFW slot number: %d\n", n);
    return;
  }

  if (n >= num_filters) {
    fprintf(stderr, "Warning: Updating CFW size to %d slots.\n", n+1);
    num_filters = n+1;
  }

  filter_slot_info[n] = filter;
  WriteFilterData();
  ReadFilterData();
}

int
Filter::PositionOf(void) {
  if (not filter_info_available) ReadFilterData();

  return filters[this->FilterIDIndex()].filter_position;
}

int FilterWheelSlots(void) {
  if (not filter_info_available) ReadFilterData();

  return num_filters;
}

std::vector<Filter> &InstalledFilters(void) {
  static std::vector<Filter> ret_array;

  if (not filter_info_available) ReadFilterData();

  ret_array.clear();
  ret_array.resize(num_filters);

  for (int n=0; n<num_filters; n++) {
    ret_array[n] = filter_slot_info[n];
  }
  return ret_array;
}



  
