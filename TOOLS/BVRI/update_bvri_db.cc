/*  bvri_report.cc -- Takes photometry and creates AAVSO Extended
 *  Format Report
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
//        Usage:
// update_bvri_db -n target_starname -s this_starname -f R -i bvri.db '[keyword][type][value]'
//****************************************************************

void usage(void) {
  fprintf(stderr,
	  "usage: update_bvri_db -n target_starname -s starname -f R -i bvri.db '[keyword][type][value]'");
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
  ResultData             *first_check_star[NUM_FILTERS];
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

// one of these for each star for each color
struct Measurement {
  JULIAN jd_exposure_midpoint;
  double airmass;
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

  char *d = buffer;

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
  d->r_airmass = -1.0;
  d->remarks = 0;
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
      fprintf(stderr, "bvri_report: read_record(): invalid element name: %s\n",
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
    for (int f=0; f<NUM_FILTERS; f++) {
      t->comp_star = t->first_check_star[f] = 0;
    }
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
      p->measurement[c].airmass = d->r_airmass;
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
  p->measurement[color].airmass = d->r_airmass;
  p->measurement[color].magnitude_raw = d->r_rawmag;
  p->measurement[color].magnitude_tr = d->r_trmag;
  p->measurement[color].instrumental_mag = d->r_instmag;
  p->measurement[color].magnitude_err = d->r_mag_err;
  p->measurement[color].is_transformed = d->r_is_transformed;
  p->measurement[color].remarks = d->remarks;

  // And see if this becomes "the" check star for this target
  if (d->r_is_check) {
    if (t->first_check_star[color] == 0 ||
	(p->hgsc_star && p->hgsc_star->is_reference)) {
      t->first_check_star[color] = p;
    }
  }
}

void update_record(DBASE *db, DBASE::DB_Record *r, const char *element_text) {
  const unsigned int t_len = strlen(element_text);
  char *field0 = (char *) malloc(t_len);
  char *field1 = (char *) malloc(t_len);
  char *field2 = (char *) malloc(t_len);

  int num_fields = sscanf(element_text, "[%[^]]][%[^]]][%[^]]]", field0, field1, field2);
  if (num_fields != 3) {
    fprintf(stderr, "Argument %s doesn't have exactly three fields.\n", element_text);
  } else {
    DBASE::DB_Element *e = r->find_by_att_name(field0);

    if (!e) {
      switch (field1[0]) {
      case 'I':
	r->add_int(field0, atoi(field2));
	break;
	
      case 'S':
	r->add_string(field0, field2);
	break;
	
      case 'D':
	r->add_double(field0, atof(field2));
	break;
	
      default:
	fprintf(stderr, "Illegal type letter: %s\n", field1);
      }
    } else {
      // otherwise, there's already an element with this keyword
      switch(field1[0]) {
      case 'I':
	e->value.int_value = atoi(field2);
	break;

      case 'S':
	e->value.char_value = strdup(field2);
	break;

      case 'D':
	e->value.double_value = atof(field2);
	break;

      default:
	fprintf(stderr, "Illegal type letter: %s\n", field1);
      }
      db->update(r);
    }
  }
  free(field0);
  free(field1);
  free(field2);
}
  
  
//****************************************************************
//        main()
//****************************************************************
int
main(int argc, char **argv) {
  int ch;			// option character
  DBASE *db = 0;
  const char *target_starname = 0;
  const char *this_starname = 0;
  char color_letter = ' ';
  std::list<const char *> element_args;

  // Command line options:
  // -i bvri.db         Name of the database to be used
  // -n target_starname
  // -s this_starname
  // -f color_letter
  
  while((ch = getopt(argc, argv, "i:n:s:f:")) != -1) {
    switch(ch) {
    case 'n':
      target_starname = optarg;
      break;

    case 's':
      this_starname = optarg;
      break;

    case 'f':
      color_letter = optarg[0];
      break;

    case 'i':
      db = new DBASE(optarg, DBASE_MODE_WRITE);
      if (!db) {
	fprintf(stderr, "update_bvri_db: cannot open database file %s\n", optarg);
	usage();
	/*NOTREACHED*/
      }
      break;

    case '?':
    default:
      usage();
      /*NOTREACHED*/
    }
  }

  if (optind >= argc) {
    fprintf(stderr, "No element [keyword][value] pairs found.\n");
    usage();
    /*NOTREACHED*/
  }

  while(optind < argc) {
    element_args.push_back(argv[optind]);
    optind++;
  }

  if(db == 0 || target_starname == 0 || this_starname == 0 || color_letter == ' ') {
    usage();
    /*NOTREACHED*/
  }

  // Now loop through all the records in the db

  const int num_recs = db->get_number_records();
  int found = false;
  for (int i=0; i < num_recs; i++) {
    DBASE::DB_Record *r = db->get_reference(i);
    if (!r) {
      fprintf(stderr, "bvri_report: Error fetching record number %d from database.\n", i);
    } else {
      DBASE::DB_Element *e_target = r->find_by_att_name("TARGET");
      DBASE::DB_Element *e_filter = r->find_by_att_name("FILTER");
      DBASE::DB_Element *e_starname = r->find_by_att_name("STARNAME");

      if (e_target == 0 || (strcmp(e_target->value.char_value, target_starname) != 0)) continue;
      if (e_starname == 0 || (strcmp(e_starname->value.char_value, this_starname) != 0)) continue;
      if (e_filter == 0 || (e_filter->value.char_value[0] != color_letter)) continue;

      // Everything matched!
      found = true;
      std::list<const char *>::iterator it;
      for (it=element_args.begin(); it != element_args.end(); it++) {
	update_record(db, r, *it);
	db->update(r);
      }
      db->close();
      break; // all done. No need to look at any more stars in the DB
    }
  }
  if (!found) {
    fprintf(stderr, "Unable to find matching record in DB file.\n");
  }
  return 0; // done
}

