/* This may look like C code, but it is really -*-c++-*- */
/*  HGSC.h -- Get star info from Hubble Guide Star Catalog
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
#ifndef _HGSC_H
#define _HGSC_H

#include <stdio.h>
#include <dec_ra.h>
#include <vector>
#include <string>
#include <list>
#include <Filter.h>

enum PhotometryColor { PHOT_V, PHOT_B, PHOT_U, PHOT_R, PHOT_I, PHOT_J,
		       PHOT_H, PHOT_K, PHOT_NONE };

PhotometryColor FilterToColor(Filter f);
const char *ColorToName(PhotometryColor c);

class MultiColorData {
public:
  void Add(PhotometryColor color, double magnitude, double uncertainty=-1.0);
  bool IsAvailable(PhotometryColor color) { return lookup_color(color) >= 0; }
  double Get(PhotometryColor color);
  // GetUncertainty returns a nonsense value < 0.0 if not available
  double GetUncertainty(PhotometryColor color);

private:
  std::vector<PhotometryColor> color_array;
  std::vector<double>          magnitude_array;
  std::vector<double>          uncertainty_array;
  int lookup_color(PhotometryColor color);
};
  

class HGSCIterator;
class HGSCList;

class HGSC {
public:
  HGSC(double dec_in_radians,
       double ra_in_radians,
       double mag,
       const char *label_text);
  HGSC(void);
  ~HGSC(void);

  void AddToFile(FILE *fp);

  // magnitude = advertised magnitude in the GSC
  // photometry = photometry advertised by AAVSO on chart
  // photometry_ensemble = ensemble photometry
  DEC_RA location;
  double magnitude;		// V magnitude
  int is_comp;			// 1=is comparison star
  int is_check;			// 1=is check star
  int is_reference;		// 1=is ensemble reference
  int is_ensemble(Filter &f);	// 1=is ensemble star
  int do_not_trust_position;	// 1=skip this star
  int is_official_check;        // 1=reported as KNAME to AAVSO
  int is_backup_check;		// 1=a secondary check star
  int do_submit;                // 1=submit report to AAVSO
  int force;			// 1=force correlation to this point
  double photometry;		// comparison magnitude
  int photometry_valid;		// 1="photometry" is valid value
  double photometry_ensemble;	// ensemble value
  int photometry_ensemble_valid; // 1="photometry_ensemble" is valid value
  int is_widefield;		// 1=is widefield star
  int is_variable;		// don't trust magnitude
  char *label;
  char *A_unique_ID;		// AAVSO Unique ID
  char *report_ID;		// common text ID name
  char *comment;		// text comment
  MultiColorData multicolor_data; // all colors, including repeat of V
private:
  HGSC *next;
  int ensemble_all_filters;
  std::list<std::string> ensemble_filters; // use this as ensemble for these filters only
  friend class HGSCIterator;
  friend class HGSCList;
};

class HGSCList {
public:
  HGSCList(void);		// null list
  void Add(HGSC &star);		// unordered list
  HGSCList(FILE *mapfile);	// picking up previously-stored file
  HGSCList(const char *starname);
  HGSCList(const DEC_RA &center,
	   const double radius_radians); // building list from HGSC itself
  void Write(const char *filename);
  int length(void) const { return list_size; }
  HGSC *FindByLabel(const char *label_string);

  ~HGSCList(void);		// destructor
  bool NameOK(void) { return name_okay; }

private:
  int  list_size;
  HGSC *head;
  void AddStarsFromFile(const DEC_RA &center,
			const double radius_radians,
			int hours, // 0 through 23
			int low_dec); // -90 through +80
  void AddStarsFromFile(const DEC_RA &center,
			const double radius_radians,
			char *filename);
  void RelabelAllStars(void);
  friend class HGSCIterator;
  void CreateFromFile(FILE *mapfile);
  bool name_okay;
};

class HGSCIterator {
public:
  HGSCIterator(HGSCList &host);	// creates iterator for a list
  HGSC *First(void) {		// first item in list
    current = (master->head ? master->head->next : 0);
    return master->head;
  }
  HGSC *Next(void) {		// next item in list
    HGSC *n = current;
    if(current) current = current->next;
    return n;
  }

private:
  HGSCList *master;
  HGSC *current;
};
  
#endif
