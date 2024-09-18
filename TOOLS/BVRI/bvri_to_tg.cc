/*  bvri_to_tg.cc -- Takes photometry and creates input file for
 *  Transform Generator
 *
 *  Copyright (C) 2016 Mark J. Munkacsy
 *
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
#include <time.h>		// ctime()
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>		// for strcat()
#include <stdio.h>
#include <unistd.h> 		// for getopt()
#include <stdlib.h>		// for atof()
#include <assert.h>
#include <HGSC.h>
#include <Image.h>
#include <strategy.h>
#include "trans_coef.h"
#include <gendefs.h>
#include <math.h>		// NaN
#include <dbase.h>
#include "colors.h"
#include "groups.h"
#include "strategy.h"
#include <list>

//****************************************************************
//        FIELD SOURCES
//  STARID:     strategy file
//  DATE:       bvri.db/TOBS
//  MAGNITUDE:  bvri.db/RAWMAG or /TRMAG
//  MAGERR:     bvri.db/MAGERR
//  FILTER:     bvri.db/FILTER
//  TRANS:      bvri.db/RAWMAG or /TRMAG
//  MTYPE:      fixed "STD"
//  CNAME:      bvri.db/COMP
//  CMAG:       bvri.db/COMP -- instrumental
//  KNAME:      catalog (HGSC)
//  KMAG:       bvri.db/?? -- instrumental
//  AIRMASS:    <??>
//  GROUP:      <internally-generated>
//  CHART:      strategy file
//  NOTES:      bvri.db/REMARKS
//****************************************************************

void usage(void) {
  fprintf(stderr,
	  "usage: bvri_to_tg -n catalogname -i bvri.db -o report.txt\n");
  exit(-2);
}

// simplify_path() will create a copy of the pathname pointed at by
// <p> and in the process will turn any consecutive pair of '//' into
// a single '/'
const char *simplify_path(const char *p) {
  char *result = (char *) malloc(strlen(p) + 2);
  const char *s = p;
  char *o = result;
  do {
    if (*s == '/' && *(s+1) == '/') s++;
    *o++ = *s;
  } while(*s++);
  return result;
}

//****************************************************************
//        Class TargetStar
//****************************************************************

// forward declaration
struct ResultData;

struct TargetStar {
  const char             *starname;
  HGSCList               *catalog;
  Strategy               *strategy;
  ResultData             *comp_star;
  std::list<ResultData *> all_results;
};

class TargetStarTable {
public:
  TargetStarTable(void) {;}
  ~TargetStarTable(void);

  void Add(const char *starname, TargetStar *data);
  TargetStar *LookupByName(const char *name);

  std::list<TargetStar *> entries;
};

TargetStarTable::~TargetStarTable(void) {
  std::list<TargetStar *>::iterator it;
  for (it = entries.begin(); it != entries.end(); it++) {
    free((void *) (*it)->starname);
  }
}

void
TargetStarTable::Add(const char *name, TargetStar *data) {
  entries.push_back(data);
}

TargetStar *
TargetStarTable::LookupByName(const char *name) {
  std::list<TargetStar *>::iterator it;
  for (it = entries.begin(); it != entries.end(); it++) {
    if (strcmp(name, (*it)->starname) == 0) {
      return (*it);
    }
  }
  return 0;
}

//****************************************************************
//        End of TargetStarTable
//****************************************************************

const char *AAVSO_FilterName(const Filter &f) {
  const char *local_filter_name = f.NameOf();
  if (strcmp(local_filter_name, "Vc") == 0) return "V";
  if (strcmp(local_filter_name, "Rc") == 0) return "R";
  if (strcmp(local_filter_name, "Ic") == 0) return "I";
  if (strcmp(local_filter_name, "Bc") == 0) return "B";
  fprintf(stderr, "AAVSO_FilterName: unrecognized filter: %s\n",
	  local_filter_name);
  return "X";
}

static Filter F_V("Vc");
static Filter F_R("Rc");
static Filter F_I("Ic");
static Filter F_B("Bc");

int filter_to_index(Filter &f) {
  const char *local_filter_name = f.NameOf();
  if (strcmp(local_filter_name, "Vc") == 0) return 1;
  if (strcmp(local_filter_name, "Rc") == 0) return 2;
  if (strcmp(local_filter_name, "Ic") == 0) return 3;
  if (strcmp(local_filter_name, "Bc") == 0) return 0;
  assert(0); // trigger error
  return -1;
}

const Filter index_to_filter(int f_i) {
  switch(f_i) {
  case 1:
    return F_V;
  case 2:
    return F_R;
  case 3:
    return F_I;
  case 0:
    return F_B;
  default:
    assert(0); // trigger error
    /*NOTREACHED*/
    return F_V;
  }
}

int filter_to_index(char filter_letter) {
  switch(filter_letter) {
  case 'B':
    return 0;
  case 'V':
    return 1;
  case 'R':
    return 2;
  case 'I':
    return 3;
  default:
    assert(0); //trigger error
    /*NOTREACHED*/
    return 1;
  }
}

// one of these for each star for each color
struct Measurement {
  JULIAN jd_exposure_midpoint;
  double magnitude_raw;
  double magnitude_tr;	 // transformed
  double instrumental_mag;
  double magnitude_err;
  bool   is_transformed;
  double stddev;
  int    stddev_valid;
  int    num_exp;

  double sum_phot;
  double sum_err;
  double sum_phot_sq;
  int    num_err;
  int    num_phot;
  double error_sum;
  int    error_count;
  double sum_jd;
  char   *remarks;
};  

// A single one of these is created for each star.
struct ResultData {
  char   A_Unique_ID[16];
  char   ReportName[32];
  char   *common_name;
  HGSC   *hgsc_star;
  int    is_comp;
  int    is_check;
  TargetStar *target_star;

  Measurement measurement[NUM_FILTERS]; // indexed by filter ID
};

char *aavso_format(const char *name) {
  static char buffer[32];

  register char *d = buffer;

  do {
    if(*name == '-') {
      *d++ = ' ';
    } else {
      *d++ = toupper(*name);
    }
  } while(*name++);

  return buffer;
}

inline void put_repeat(char c, int count, FILE *fp) {
  while(count--) fputc(c, fp);
}

struct Single_Record {
  double r_t_obs;
  char *r_comp;
  int r_is_comp;
  int r_is_check;
  Filter r_filter;
  char *r_starname;
  char *r_auid;
  char *r_target_star;
  double r_airmass;
  double r_rawmag;
  double r_instmag;
  double r_trmag;
  double r_V_R;
  double r_B_V;
  double r_R_I;
  double r_V_I;
  double r_mag_err;
  char *remarks;
  bool r_is_transformed;
};  

bool TargetNameMatch(const char *name1, const char *name2) {
  // names can only get shorter during "compression" (removing
  // punctuation and changing to all UC)
  char *n1 = strdup(name1);
  char *n2 = strdup(name2);
  char *s = 0;
  // Yes, I know this looks strange. Go through the loop twice, once
  // for n1 and once for n2. "s" points to either n1 or n2.
  for (int i=0, s=n1; i<2; i++, s=n2) {
    char *p = *s;
    while(*s) {
      if (isdigit(*s)) {
	*p++ = *s++;
      } else if (islower(*s)) {
	*p++ = toupper(*s++);
      } else {
	s++;
      }
    }
    *p = 0;
  }

  bool answer = (strcmp(n1, n2) == 0);
  free(n1);
  free(n2);
  return answer;
}

void
print_line(ResultData *r,
	   int c, // color_index
	   FILE *fp_out);
void PrintHeader(FILE *fp);

void
read_record(DBASE::DB_Record *r, struct Single_Record *d) {
  std::list<DBASE::DB_Element *>::iterator it;
  d->r_comp = 0;
  d->r_is_comp = d->r_is_check = 0;
  d->r_starname = 0;
  d->r_auid = 0;
  d->r_rawmag = 99.9;
  d->r_instmag = 99.9;
  d->r_trmag = 99.9;
  d->r_V_R = 99.9;
  d->r_B_V = 99.9;
  d->r_R_I = 99.9;
  d->r_V_I = 99.9;
  d->r_mag_err = 99.9;
  d->remarks = 0;
  d->r_airmass = -1.0;
  d->r_is_transformed = false;
  
  for (it = r->elements.begin(); it != r->elements.end(); it++) {
    DBASE::DB_Element *e = (*it);
    if (strcmp(e->att_name, "TOBS") == 0) {
      d->r_t_obs = e->value.double_value;
    } else if (strcmp(e->att_name, "IS_COMP") == 0) {
      d->r_is_comp = e->value.int_value;
    } else if (strcmp(e->att_name, "IS_CHECK") == 0) {
      d->r_is_check = e->value.int_value;
    } else if (strcmp(e->att_name, "COMP") == 0) {
      d->r_comp = strdup(e->value.char_value);
    } else if (strcmp(e->att_name, "FILTER") == 0) {
      // want to handle both long ("Bc") and short ("B") filter
      // strings here.
      char full_filter_name[8];
      strcpy(full_filter_name, e->value.char_value);
      if (full_filter_name[1] == 0) {
	// received a short filter name
	full_filter_name[1] = 'c';
	full_filter_name[2] = 0;
      }
      d->r_filter = Filter(full_filter_name);
    } else if (strcmp(e->att_name, "STARNAME") == 0) {
      d->r_starname = strdup(e->value.char_value);
    } else if (strcmp(e->att_name, "AUID") == 0) {
      d->r_auid = strdup(e->value.char_value);
    } else if (strcmp(e->att_name, "AIRMASS") == 0) {
      d->r_airmass = e->value.double_value;
    } else if (strcmp(e->att_name, "RAWMAG") == 0) {
      d->r_rawmag = e->value.double_value;
    } else if (strcmp(e->att_name, "TRMAG") == 0) {
      d->r_trmag = e->value.double_value;
      d->r_is_transformed = true;
    } else if (strcmp(e->att_name, "INSTMAG") == 0) {
      d->r_instmag = e->value.double_value;
    } else if (strcmp(e->att_name, "V_R") == 0) {
      d->r_V_R = e->value.double_value;
    } else if (strcmp(e->att_name, "B_V") == 0) {
      d->r_B_V = e->value.double_value;
    } else if (strcmp(e->att_name, "R_I") == 0) {
      d->r_R_I = e->value.double_value;
    } else if (strcmp(e->att_name, "V_I") == 0) {
      d->r_V_I = e->value.double_value;
    } else if (strcmp(e->att_name, "MAGERR") == 0) {
      d->r_mag_err = e->value.double_value;
    } else if (strcmp(e->att_name, "TARGET") == 0) {
      d->r_target_star = strdup(e->value.char_value);
    } else if (strcmp(e->att_name, "REMARKS") == 0) {
      d->remarks = strdup(e->value.char_value);
    } else {
      fprintf(stderr, "bvri_to_tg: read_record(): invalid element name: %s\n",
	      e->att_name);
    }
  } // end loop over all elements in the record
}  

std::list<ResultData *> all_results;

// Add this record to the dictionary and the set of ResultData
void
process_record(TargetStarTable *dictionary,
	       DBASE::DB_Record *r) {
  Single_Record *d = new Single_Record;
  read_record(r, d);

  TargetStar *t = dictionary->LookupByName(d->r_target_star);
  if (t==0) {
    // This is a new target star
    t = new TargetStar;
    t->starname = strdup(d->r_target_star);
    t->catalog = new HGSCList(d->r_target_star);
    t->strategy = new Strategy(d->r_target_star, 0);
    dictionary->Add(d->r_target_star, t);
  }
    
  ResultData *p = 0;
  // Do we have an existing ResultData for this star?
  std::list<ResultData *>::iterator it;
  for (it = t->all_results.begin(); it != t->all_results.end(); it++) {
    if (strcmp((*it)->common_name, d->r_starname) == 0) {
      p = (*it);
      break;
    }
  } // end search for existing ResultData
  if (p == 0) {
    // need to create new ResultData
    p = new ResultData;
    // Give the new ResultData some safe default values
    p->is_comp = d->r_is_comp;
    p->is_check = d->r_is_check;
    p->common_name = strdup(d->r_starname);
    p->ReportName[0] = 0;
    for (int c = 0; c < NUM_FILTERS; c++) {
      p->measurement[c].stddev_valid = 0;
      p->measurement[c].magnitude_raw = 
	p->measurement[c].magnitude_tr = 
	p->measurement[c].instrumental_mag = 99.9;
    }
    p->hgsc_star = t->catalog->FindByLabel(d->r_starname);
    p->target_star = t;
    t->all_results.push_back(p);
  }

  // if this is the comp star, remember this important detail
  if (d->r_is_comp) {
    t->comp_star = p;
  }

  // now translate from Single_Record into ResultData
  int color = filter_to_index(d->r_filter);
  if (d->r_is_comp) p->is_comp = 1;
  if (d->r_is_check) p->is_check = 1;
  p->measurement[color].jd_exposure_midpoint = JULIAN(d->r_t_obs);
  if (d->r_auid) {
    strcpy(p->A_Unique_ID, d->r_auid);
  } else {
    p->A_Unique_ID[0] = 0;
  }
  // skip airmass for now
  p->measurement[color].magnitude_raw = d->r_rawmag;
  p->measurement[color].magnitude_tr = d->r_trmag;
  p->measurement[color].instrumental_mag = d->r_instmag;
  p->measurement[color].magnitude_err = d->r_mag_err;
  p->measurement[color].is_transformed = d->r_is_transformed;
  p->measurement[color].remarks = d->remarks;
}

//****************************************************************
//        main()
//****************************************************************
int
main(int argc, char **argv) {
  int ch;			// option character
  FILE *fp_out = 0;
  DBASE *db = 0;
  char *catalog_name = 0;
  char filter_letter = ' ';

  // Command line options:
  // -c {BVRI}          Single-letter name of desired filter
  // -n catalog_name
  // -o output_filename
  // -i bvri.db         Name of the database to be used

  while((ch = getopt(argc, argv, "c:n:o:i:")) != -1) {
    switch(ch) {
    case 'o':
      fp_out = fopen(optarg, "w");
      if(!fp_out) {
	fprintf(stderr, "bvri_to_tg: cannot open output file %s\n", optarg);
	exit(-2);
      }
      break;

    case 'c':
      filter_letter = *optarg;
      break;

    case 'n':
      catalog_name = strdup(optarg);
      break;

    case 'i':
      db = new DBASE(optarg, DBASE_MODE_READONLY);
      if (!db) {
	fprintf(stderr, "bvri_to_tg: cannot open database file %s\n", optarg);
	usage();
	/*NOTREACHED*/
      }
      break;

    case '?':
    default:
      usage();
    }
  }

  if(fp_out == 0 || db == 0 ) {
    usage();
  }

  // Now loop through all the records in the db, grabbing the HGSC
  // structure for each target star and building the star dictionary

  TargetStarTable dictionary;
  char HGSCfilename[132];
  sprintf(HGSCfilename, CATALOG_DIR "/%s", starname);
  FILE *HGSC_fp = fopen(HGSCfilename, "r");
  if(!HGSC_fp) {
    fprintf(stderr, "Cannot open catalog file for %s\n", starname);
    exit(-2);
  }

  HGSCList Catalog(HGSC_fp);
  
  const int num_recs = db->get_number_records();
  for (int i=0; i < num_recs; i++) {
    DBASE::DB_Record r;
    if (db->get(i, &r) != DBASE_SUCCESS) {
      fprintf(stderr, "bvri_to_tg: Error fetching record number %d from database.\n", i);
    } else {
      DBASE::DB_Element *e = r.find_by_att_name("ERRORS");
      if (e == 0) {
	process_record(&dictionary, &r);
      }
    }
  }
  
  // Now we can start printing lines
  PrintHeader(fp_out);
  std::list<TargetStar *>::iterator it_t;
  std::list<ResultData *>::iterator it;
  for (it_t = dictionary.entries.begin(); it_t != dictionary.entries.end(); it_t++) {
    TargetStar *t = (*it_t);
    if (TargetNameMatch(t->starname, catalog_name)) {
      fprintf(stderr, "Target found.\n");
      for (it = t->all_results.begin(); it != t->all_results.end(); it++) {
	ResultData *r = (*it);
	fprintf(stderr, "star = %s", r->common_name);
	if (r->hgsc_star && r->hgsc_star->is_check) {
	  const int c = filter_to_index(filter_letter);
	  if (r->measurement[c].magnitude_raw < 90.0) {
	    fprintf(stderr, "     invoking print_line\n");
	    print_line(r, c, fp_out);
	  } // end if valid data present for this color
	} // end if star is being included
      } // end loop over all stars for this target
    } // end if target name matched
  } // end loop over all targets
  
  fclose(fp_out);
}

//****************************************************************
//        print_line()
//  Prints a single line of output from a single ResultData structure
//  (relying on the comp_star data).
//****************************************************************
void
print_line(ResultData *r,
	   int c, // pri_color_index
	   int c0, // secondary_color_index
	   FILE *fp_out) {
  static GroupData group;
  
  Measurement *m = &(r->measurement[c]);
  // STARID
  if (r->A_Unique_ID[0]) {
    fprintf(fp_out, "%s ", r->A_Unique_ID);
  } else {
    fprintf(fp_out, "%s ", aavso_format(r->common_name));
  }
  // INSTRUMENTAL MAGNITUDE
  fprintf(fp_out, "%.3lf ", m->magnitude_raw);
  // SNR
  fprintf(fp_out, "100 ");
  // X,Y
  fprintf(fp_out, "0.000 0.000 ");
  // SKY
  fprintf(fp_out, "50 ");
  // AIRMASS
  fprintf(fp_out, "1.000 ");
  // COLOR
  
  // CHART
  fprintf(fp_out, "%s,", r->target_star->strategy->ObjectChart());
  // NOTES
  fprintf(fp_out, "%s", (m->remarks ? m->remarks : ""));

  fprintf(fp_out, "\n");
}
  
void PrintHeader(FILE *fp) {
  fprintf(fp, "Primary target: %s\n", something);
  fprintf(fp, "Exposure time: %.1lf\n", something);
  fprintf(fp, "Filter: %c\n", something);
  fprintf(fp, "Observation date/time: %s\n", something);
  fprintf(fp, "JD: %.5lf\n", something);
  fprintf(fp, "Decimal date: %s\n", something);
  fprintf(fp, "R.A.: %s\n", something);
  fprintf(fp, "Dec.: %s\n", something);
  fprintf(fp, "Airmass: %.4lf\n", something);
  fprintf(fp, "Calibration: BDF\n");
  fprintf(fp, "Aperture radius: 3.5 pixels\n");
  fprintf(fp, "File name: %s\n");
  fprintf(fp, "\n\n\n");
}
