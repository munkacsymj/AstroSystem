/*  HGSC.cc -- Get star info from Hubble Guide Star Catalog
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
#include "HGSC.h"
#include <string.h>		// strdup()
#include <stdio.h>		// fgets(), sscanf()
#include <stdlib.h>		// free() -- related to strdup()
				// system()
#include <iostream>
#include <sys/types.h>		// for stat()
#include <sys/stat.h>		// for stat()
#include <fcntl.h>		// for open()
#include <sys/uio.h>		// for read()
#include <unistd.h>		// for read()
#include <gendefs.h>
#include <assert.h>

// create an HGSC star given all its parts
HGSC::HGSC(double dec_in_radians,
	   double ra_in_radians,
	   double mag,
	   const char *label_text) :
     location(dec_in_radians, ra_in_radians),
  magnitude(mag) {
    label = strdup(label_text);
    photometry_valid =
      photometry_ensemble_valid =
      is_reference =
      is_check =
      is_variable =
      do_not_trust_position =
      is_backup_check =
      is_official_check =
      is_comp =
      ensemble_all_filters =
      do_submit =
      force =
      (is_widefield = 0);
    A_unique_ID = 0;
    report_ID = 0;
    comment = 0;
}

// destroy an HGSC star
//#define BROKEN
HGSC::~HGSC(void) {
#ifdef BROKEN
  return;
#else
  if(label) {
    //fprintf(stderr, "HGSC free(%s)\n", label);
    free(label);
  }
  if(comment) free(comment);
  if(A_unique_ID) free(A_unique_ID);
#endif
}

// create a "null" star
HGSC::HGSC(void) : location(), magnitude(0.0) {
  label = 0;
  comment = 0;
  photometry_valid = photometry_ensemble_valid = is_reference = is_check = do_submit = is_official_check = do_not_trust_position = is_variable = is_comp = is_backup_check = force = is_widefield = 0;
  A_unique_ID = 0;
}

void
HGSC::AddToFile(FILE *fp) {
    fprintf(fp, "%s %f %f %f ",
	    label,
	    location.dec(),
	    location.ra_radians(),
	    magnitude);
    if(is_comp)              fprintf(fp, "COMP ");
    if(is_variable)          fprintf(fp, "VARIABLE ");
    if(is_reference)         fprintf(fp, "REF ");
    if(is_backup_check)      fprintf(fp, "BACKUP ");
    if(is_official_check)    fprintf(fp, "OFFICIAL_CHECK ");
    if(is_check)             fprintf(fp, "CHECK ");
    if(ensemble_all_filters) fprintf(fp, "ENSEMBLE ");
    if(do_submit)            fprintf(fp, "SUBMIT ");
    if(do_not_trust_position) fprintf(fp, "NOPOSIT ");
    if(force)                fprintf(fp, "FORCE ");
    if(is_widefield)         fprintf(fp, "WIDE ");
    if(photometry_valid)     fprintf(fp, "MV=%.3f ", photometry);
    if(ensemble_filters.size() > 0) {
      char buffer[32] {""};
      for(std::string &f_name : ensemble_filters) {
	strcat(buffer, ",");
	strcat(buffer, f_name.c_str());
      }
      fprintf(fp, "ENSEMBLE:%s ", buffer+1);
    }
    if(photometry_ensemble_valid)
      fprintf(fp, "MVE=%.3f ", photometry_ensemble);
    if(A_unique_ID && A_unique_ID[0])
      fprintf(fp, "AUID=%s ", A_unique_ID);
    if(report_ID && report_ID[0])
      fprintf(fp, "REPORT=%s ", report_ID);
    if(multicolor_data.IsAvailable(PHOT_V)) 
      fprintf(fp, "PV=%.3lf|%.3lf ", multicolor_data.Get(PHOT_V),
	      multicolor_data.GetUncertainty(PHOT_V));
    if(multicolor_data.IsAvailable(PHOT_B)) 
      fprintf(fp, "PB=%.3lf|%.3lf ", multicolor_data.Get(PHOT_B),
	      multicolor_data.GetUncertainty(PHOT_B));
    if(multicolor_data.IsAvailable(PHOT_U)) 
      fprintf(fp, "PU=%.3lf|%.3lf ", multicolor_data.Get(PHOT_U),
	      multicolor_data.GetUncertainty(PHOT_U));
    if(multicolor_data.IsAvailable(PHOT_R)) 
      fprintf(fp, "PR=%.3lf|%.3lf ", multicolor_data.Get(PHOT_R),
	      multicolor_data.GetUncertainty(PHOT_R));
    if(multicolor_data.IsAvailable(PHOT_I)) 
      fprintf(fp, "PI=%.3lf|%.3lf ", multicolor_data.Get(PHOT_I),
	      multicolor_data.GetUncertainty(PHOT_I));
    if(multicolor_data.IsAvailable(PHOT_J)) 
      fprintf(fp, "PJ=%.3lf|%.3lf ", multicolor_data.Get(PHOT_J),
	      multicolor_data.GetUncertainty(PHOT_J));
    if(multicolor_data.IsAvailable(PHOT_H)) 
      fprintf(fp, "PH=%.3lf|%.3lf ", multicolor_data.Get(PHOT_H),
	      multicolor_data.GetUncertainty(PHOT_H));
    if(multicolor_data.IsAvailable(PHOT_K)) 
      fprintf(fp, "PK=%.3lf|%.3lf ", multicolor_data.Get(PHOT_K),
	      multicolor_data.GetUncertainty(PHOT_K));
    if(comment) fprintf(fp, "#%s", comment);
      
    fprintf(fp, "\n");
}

// create an empty list
HGSCList::HGSCList(void) {
  list_size = 0;
  head = 0;
}

// add one star to a list
void
HGSCList::Add(HGSC &star) {
  star.next = head;
  head = &star;
  list_size++;
}

// write the list onto one of the "catalog" files we use with the
// correlate1 routines.  
void
HGSCList::Write (const char *filename) {
  FILE *fp = fopen(filename, "w");

  HGSCIterator i(*this);
  HGSC *h;

  for(h=i.First(); h; h=i.Next()) {
    h->AddToFile(fp);
  }
  fclose(fp);
}

HGSCList::HGSCList(const char *starname) {
  char HGSCfilename[132];
  sprintf(HGSCfilename, CATALOG_DIR"/%s", starname);
  FILE *fp = fopen(HGSCfilename, "r");
  if (fp) {
    CreateFromFile(fp);
    fclose(fp);
    name_okay = true;
  } else {
    name_okay = false;
  }
}

HGSCList::HGSCList(FILE *mapfile) {
  CreateFromFile(mapfile);
}

int
HGSC::is_ensemble(Filter &f) {
  if (this->ensemble_all_filters) {
    return true;
  }

  for(std::string &f_name : ensemble_filters) {
    Filter f_equiv(f_name.c_str());
    if (strcmp(f_equiv.NameOf(), f.NameOf()) == 0) {
      return true;
    }
  }
  return false;
}

// Read an HGSC list from one of our star-specific, local HGSC files
// (e.g., a "rr-boo" starlist file).
void
HGSCList::CreateFromFile(FILE *mapfile) {
  char buffer[256];		// holds input line from file
  char label[256];		// holds star's label
  int linecount = 0;
  char *comment_string;
  list_size = 0;
  head = 0;

  fprintf(stderr, "Reading stars from file.\n");
  while(fgets(buffer, sizeof(buffer), mapfile)) {
    char *s;
    comment_string = 0;

    linecount++;
    for(s=buffer; *s != '\n' && *s; s++) {
      // find start of comment and erase
      if(*s == '#') {
	*s = 0;
	comment_string = s+1;
	break;
      }
    }

    double dec, ra, magnitude;
    int chars_read_so_far;
    int count = sscanf(buffer, "%s %lf %lf %lf %n",
		       label,
		       &dec,
		       &ra,
		       &magnitude,
		       &chars_read_so_far);
    if(count <= 0) continue;	// blank line?
    if(count != 4) {
      fprintf(stderr, "Cannot parse line %d in mapfile (count=%d):\n%s\n",
	      linecount, count, buffer);
      
      continue;
    }
	      
    HGSC *new_one = new HGSC(dec, ra, magnitude, label);

    if (comment_string) {
      for (char *s=comment_string; *s; s++) {
	if (*s == '\n') {
	  *s = 0;
	  break;
	}
      }
      new_one->comment = strdup(comment_string);
    } else {
      new_one->comment = 0;
    }

    char attribute[80];
    int extra_chars;
    while(sscanf(buffer+chars_read_so_far, "%s %n", attribute, &extra_chars) > 0) {
      char AUID_buffer[80];
      chars_read_so_far += extra_chars;
      // got another attribute
      if(attribute[0] == 0 || attribute[0] == ' '  || attribute[0] == '\n') {
	break; // ignore it
      } else if(strcmp(attribute, "WIDE") == 0) {
	new_one->is_widefield = 1;
      } else if(strcmp(attribute, "REF") == 0) {
	new_one->is_reference = 1;
      } else if(strcmp(attribute, "NOPOSIT") == 0) {
	new_one->do_not_trust_position = 1;
      } else if(strcmp(attribute, "FORCE") == 0) {
	new_one->force = 1;
      } else if(strcmp(attribute, "SUBMIT") == 0) {
	new_one->do_submit = 1;
      } else if(strcmp(attribute, "OFFICIAL_CHECK") == 0) {
	new_one->is_official_check = 1;
      } else if(strcmp(attribute, "CHECK") == 0) {
	new_one->is_check = 1;
      } else if(strcmp(attribute, "ENSEMBLE") == 0) {
	new_one->ensemble_all_filters = 1;
      } else if(strncmp(attribute, "ENSEMBLE:", 9) == 0 or
		strncmp(attribute, "ENSEMBLE=", 9) == 0) {
	// extended ENSEMBLE format..."ENSEMBLE:B,V,R"
	const char *p = attribute+9;
	char ebuffer[32];
	if (strlen(p) > sizeof(ebuffer)) {
	  std::cerr << "ERROR: Invalid ensemble filter string: " << buffer << '\n';
	} else {
	  while (*p and *p != '\n') {
	    char *d = ebuffer;
	    while (*p and *p != ',' && *p != '\n') {
	      *d++ = *p++;
	    }
	    *d = 0;
	    Filter f(ebuffer);
	    new_one->ensemble_filters.push_back(f.NameOf());
	    if (*p == ',') p++;
	  }
	}
      } else if(strcmp(attribute, "VARIABLE") == 0) {
	new_one->is_variable = 1;
      } else if(strcmp(attribute, "COMP") == 0) {
	new_one->is_comp = 1;
      } else if(attribute[0] == 'M' &&
		attribute[1] == 'V' &&
		attribute[2] == '=') {
	sscanf(attribute+3, "%lf", &new_one->photometry);
	new_one->photometry_valid = 1;
      } else if(attribute[0] == 'M' &&
		attribute[1] == 'V' &&
		attribute[2] == 'E' &&
		attribute[3] == '=') {
	sscanf(attribute+4, "%lf", &new_one->photometry_ensemble);
	new_one->photometry_ensemble_valid = 1;
      } else if(attribute[0] == 'R' &&
		attribute[1] == 'E' &&
		attribute[2] == 'P' &&
		attribute[3] == 'O' &&
		attribute[4] == 'R' &&
		attribute[5] == 'T' &&
		attribute[6] == '=') {
	sscanf(attribute+7, "%s", AUID_buffer);
	new_one->report_ID = strdup(AUID_buffer);
      } else if(attribute[0] == 'A' &&
		attribute[1] == 'U' &&
		attribute[2] == 'I' &&
		attribute[3] == 'D' &&
		attribute[4] == '=') {
	sscanf(attribute+5, "%s", AUID_buffer);
	new_one->A_unique_ID = strdup(AUID_buffer);
      } else if(attribute[0] == 'P' && attribute[2] == '=') {
	double value;
	double uncertainty = -1.0;
	bool uncty_present = false;
	// scan for a | character (signalling an uncertainty value)
	for (const char *x = attribute; *x; x++) {
	  if (*x == '|') {
	    uncty_present = true;
	    break;
	  }
	}
	if (uncty_present) {
	  sscanf(attribute+3, "%lf|%lf", &value, &uncertainty);
	} else {
	  sscanf(attribute+3, "%lf", &value);
	}
	switch (attribute[1]) {
	case 'V':
	  new_one->multicolor_data.Add(PHOT_V, value, uncertainty);
	  break;
	case 'U':
	  new_one->multicolor_data.Add(PHOT_U, value, uncertainty);
	  break;
	case 'B':
	  new_one->multicolor_data.Add(PHOT_B, value, uncertainty);
	  break;
	case 'R':
	  new_one->multicolor_data.Add(PHOT_R, value, uncertainty);
	  break;
	case 'I':
	  new_one->multicolor_data.Add(PHOT_I, value, uncertainty);
	  break;
	case 'J':
	  new_one->multicolor_data.Add(PHOT_J, value, uncertainty);
	  break;
	case 'H':
	  new_one->multicolor_data.Add(PHOT_H, value, uncertainty);
	  break;
	case 'K':
	  new_one->multicolor_data.Add(PHOT_K, value, uncertainty);
	  break;
	default:
	  fprintf(stderr, "Invalid color attribute: %s\n", attribute);
	}
      } else {
	fprintf(stderr, "Invalid attribute: %s\n", attribute);
      }
    }

    Add(*new_one);
  }
  fprintf(stderr, "%d stars processed.\n", linecount);
}

// Kill off a list
HGSCList::~HGSCList(void) {
  if(name_okay) {
    HGSC *p = head;

    while(p) {
      HGSC *p1 = p->next;
      delete p;
      p = p1;
    }
  }
}

// List iterator
HGSCIterator::HGSCIterator(HGSCList &host) {
  master = &host;
  current = master->head;
}

				// building list from HGSC itself
				// read as many subfiles as necessary
				// to cover the entire radius
HGSCList::HGSCList(const DEC_RA &center,
		   const double radius_radians) {
  list_size = 0;
  head = 0;

  double adj = cos(center.dec());

  // We make simple-minded, rectangular assumptions.  Should be okay
  // except really, really close to the pole.
  double NorthLimitDegrees = (180.0/M_PI) * (center.dec() + radius_radians);
  double SouthLimitDegrees = (180.0/M_PI) * (center.dec() - radius_radians);
  double EastLimitHours = center.ra() - radius_radians * (24.0/(2.0*M_PI))/adj;
  double WestLimitHours = center.ra() + radius_radians * (24.0/(2.0*M_PI))/adj;

  if(EastLimitHours < 0.0) EastLimitHours += 24.0;

  const int EastLimit = (int) EastLimitHours;

  int WestLimit = (int) WestLimitHours;
  if(WestLimit >= 24) WestLimit -= 24;

  fprintf(stderr, "EastLimit = %d, WestLimit = %d\n",
	  EastLimit, WestLimit);

  double NS = SouthLimitDegrees;
  for(NS = SouthLimitDegrees; NS <= NorthLimitDegrees; NS += 10.0) {
    int EW = EastLimit;
    int low_dec;

    low_dec = 10 * ((int) floor(NS/10.0));

    do {
      AddStarsFromFile(center,
		       radius_radians,
		       EW,
		       low_dec);

      if(EW == WestLimit) break;

      EW += 1;		// add one hour
      if(EW >= 24) EW -= 24;
    } while (1);
  }
  RelabelAllStars();		// give all stars unique label
}
		     
// Read from a single file. Get it from the disk cache if it's there,
// otherwise read it from the CD.
void
HGSCList::AddStarsFromFile(const DEC_RA &center,
			   const double radius_radians,
			   int hours, // 0 through 23
			   int low_dec) { // -90 through +80
  struct stat stat_info;
  int abs_low_dec = low_dec;
  if(low_dec < 0) abs_low_dec = -abs_low_dec;

  fprintf(stderr, "AddStarsFromFile(hour=%d, dec=%d)\n",
	  hours, low_dec);
  const char *cache_dir = HGSC_CATALOG_DIR "/CACHE";
  //  const char *orig_source_dir = "/cd/hgc";
  const char *orig_source_dir = HGSC_CATALOG_DIR;

  char simple_filename[64];
  char filename[128];

  // First try the local disk cache
  sprintf(simple_filename, "%02d%c%02d.dat",
	  hours, (low_dec < 0 ? 's' : 'n'), abs_low_dec);
  sprintf(filename, "%s/%s", cache_dir, simple_filename);
  if(stat(filename, &stat_info)== 0)
    return AddStarsFromFile(center, radius_radians, filename);

  // Not in the cache. Try the CD.
  sprintf(filename, "%s/%s", orig_source_dir, simple_filename);
  if(stat(filename, &stat_info) == 0) {
    AddStarsFromFile(center, radius_radians, filename);
    char system_buffer[256];
    sprintf(system_buffer, "cp %s/%s %s/%s",
	    orig_source_dir, simple_filename, cache_dir, simple_filename);
    fprintf(stderr, "Putting %s into HGSC cache.\n", simple_filename);
    if (system(system_buffer) < 0) {  // put into cache
      fprintf(stderr, "HGSC: cp command failed.\n");
    }
  } else {
    fprintf(stderr, "Cannot read %s from CD.\n", simple_filename);
  }
}

// Read from a single file. Doesn't understand caches. Just reads the
// specified file (could be either the CD or the cache file).
void
HGSCList::AddStarsFromFile(const DEC_RA &center,
			   const double radius_radians,
			   char *filename) {
  fprintf(stderr, "Reading from %s\n", filename);

  struct stat stat_info;
  int fd = open(filename, O_RDONLY, 0);
  if(fd < 0) {
    perror(filename);
    return;
  }
  
  // Find out how big the file is.
  fstat(fd, &stat_info);
  
  char *buffer = (char *) malloc(stat_info.st_size);
  if(!buffer) {
    perror("Cannot allocate memory to read HGSC catalog file");
    return;
  }
  
  if(read(fd, buffer, stat_info.st_size) != stat_info.st_size) {
    perror("Could not read entire HGSC catalog file");
  } else {

    char *s = buffer;
    int count = stat_info.st_size;
    int added = 0;

    double adj = cos(center.dec());
    float min_ra = (24.0/(2.0*M_PI))*(center.ra_radians() -
				      radius_radians/adj);
    float max_ra = (24.0/(2.0*M_PI))*(center.ra_radians() +
				      radius_radians/adj);
    float min_dec = (180.0/M_PI)*(center.dec() - radius_radians/adj);
    float max_dec = (180.0/M_PI)*(center.dec() + radius_radians/adj);

    while(count > 0) {
      float *ra_hours = (float *) (s+0);
      float *dec_deg = (float *) (s+4);
      unsigned char *mag_10 = (unsigned char *) (s+8);

      if(*ra_hours >= min_ra &&
	 *ra_hours <= max_ra &&
	 *dec_deg >= min_dec &&
	 *dec_deg <= max_dec) {
	HGSC *new_star = new HGSC((double) (M_PI/180.0)*(*dec_deg),
				  (double) (M_PI/12.0)*(*ra_hours),
				  ((double)(*mag_10))/10.0,
				  "catalog");
	Add(*new_star);
	added++;
      }

      s += 9;			// next star
      count -= 9;
    }
    fprintf(stderr, "Added %d stars from %s\n", added, filename);
  }

  free(buffer);
  close(fd);
}
  
void
HGSCList::RelabelAllStars(void) {
  HGSCIterator it(*this);
  HGSC *star;
  char buffer[12];
  int counter = 0;

  // For all stars in the list
  for(star = it.First(); star; star = it.Next()) {
    counter++;
    sprintf(buffer, "&%03d", counter);
    if(star->label) free(star->label);
    star->label = strdup(buffer);
  }
}

//****************************************************************
//        FindByLabel()
//   Checks to make sure that there is only one star with that
//        label. If multiple stars are found, will print error message
//        and return a <nil>
//****************************************************************
HGSC *
HGSCList::FindByLabel(const char *label_string) {
  HGSCIterator it(*this);
  HGSC *first_found = 0;
  HGSC *star;

  for(star = it.First(); star; star = it.Next()) {
    if(strcmp(label_string, star->label) == 0) {
      if (first_found) {
	// bad news..... this label_string isn't unique in this
	// HGSCList
	fprintf(stderr, "HGSC: Label %s not unique.\n",
		label_string);
	return 0;
      } else {
	// first find
	first_found = star;
      }
    }
  }

  return first_found;
}

int MultiColorData::lookup_color(PhotometryColor color) {
  for (int i=0; i< (int) color_array.size(); i++) {
    if (color == color_array[i]) return i;
  }
  return -1;
}

void 
MultiColorData::Add(PhotometryColor color, double magnitude, double uncertainty) {
  int i = lookup_color(color);
  if (i >= 0) {
    // entry already exists for this color
    magnitude_array[i] = magnitude;
    uncertainty_array[i] = uncertainty;
  } else {
    // no entry yet for this color
    color_array.push_back(color);
    magnitude_array.push_back(magnitude);
    uncertainty_array.push_back(uncertainty);
  }
}

double 
MultiColorData::Get(PhotometryColor color) {
  int i = lookup_color(color);
  if (i >= 0) {
    // entry exists
    return magnitude_array[i];
  } else {
    return -99.9;
  }
}
	       
double 
MultiColorData::GetUncertainty(PhotometryColor color) {
  int i = lookup_color(color);
  if (i >= 0) {
    // entry exists
    return uncertainty_array[i];
  } else {
    return -99.9;
  }
}
	       
PhotometryColor FilterToColor(Filter f) {
  const char *filter_name = f.NameOf();

  assert(filter_name);
  assert(*filter_name);

  if (strcmp(filter_name, "Invalid") == 0) return PHOT_NONE;
  if (strcmp(filter_name, "R") == 0) return PHOT_R;
  if (strcmp(filter_name, "Rc") == 0) return PHOT_R;
  if (strcmp(filter_name, "Bc") == 0) return PHOT_B;
  if (strcmp(filter_name, "B") == 0) return PHOT_B;
  if (strcmp(filter_name, "U") == 0) return PHOT_U;
  if (strcmp(filter_name, "Ic") == 0) return PHOT_I;
  if (strcmp(filter_name, "I") == 0) return PHOT_I;
  if (strcmp(filter_name, "Vc") == 0) return PHOT_V;
  if (strcmp(filter_name, "V") == 0) return PHOT_V;
  if (strcmp(filter_name, "Clear") == 0) return PHOT_NONE;
  if (strcmp(filter_name, "None") == 0) return PHOT_NONE;

  fprintf(stderr, "FilterToColor: cannot convert '%s'\n", filter_name);
  return PHOT_NONE;
}

const char *ColorToName(PhotometryColor c) {
  switch(c) {
  case PHOT_V:
    return "V";
  case PHOT_B:
    return "B";
  case PHOT_U:
    return "U";
  case PHOT_R:
    return "R";
  case PHOT_I:
    return "I";
  case PHOT_J:
    return "J";
  case PHOT_H:
    return "H";
  case PHOT_K:
    return "K";
  default:
  case PHOT_NONE:
    return "*";
  }
  /*NOTREACHED*/
  return "";
}
    
