/*  bvri_pretty.cc -- Takes photometry and creates pretty text file
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
#include <string.h>		// for strcat()
#include <stdio.h>
#include <unistd.h> 		// for getopt()
#include <stdlib.h>		// for atof()
#include <assert.h>
#include <named_stars.h>
#include <HGSC.h>
#include <Image.h>
#include <strategy.h>
#include "trans_coef.h"
#include <report_file.h>
#include <gendefs.h>
#include <math.h>		// NaN
#include <dbase.h>
#include "colors.h"

void usage(void) {
  fprintf(stderr,
	  "usage: bvri_pretty [-e errorfilename] -n targetname -i bvri.db -o starname.phot\n");
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

class ResultData; // forward declaration
class StarNameTable {
public:
  StarNameTable(void) {;}
  ~StarNameTable(void);

  void Add(const char *name, ResultData *data);
  ResultData *Lookup(const char *name);

private:
  struct SNT_Entry {
    const char *starname;
    ResultData *value;
  };
  std::list<SNT_Entry *> entries;
};

StarNameTable::~StarNameTable(void) {
  std::list<SNT_Entry *>::iterator it;
  for (it = entries.begin(); it != entries.end(); it++) {
    free((void *) (*it)->starname);
  }
}

void
StarNameTable::Add(const char *name, ResultData *data) {
  const ResultData *tmp = Lookup(name);
  if (tmp == 0) {
    // time to make a new entry
    SNT_Entry *entry = new SNT_Entry;
    entry->starname = strdup(name);
    entry->value = data;
    entries.push_back(entry);
  } else if (tmp != data) {
    // Same name refers to two different entries!!!
    fprintf(stderr, "bvri_pretty: StarNameTable: non-unique name: %s\n", name);
  }
}

ResultData *
StarNameTable::Lookup(const char *name) {
  std::list<SNT_Entry *>::iterator it;
  for (it = entries.begin(); it != entries.end(); it++) {
    if (strcmp(name, (*it)->starname) == 0) {
      return (*it)->value;
    }
  }
  return 0;
}

const char *AAVSO_FilterName(Filter &f) {
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

Filter index_to_filter(int f_i) {
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
  
class EachStar;

class AnalysisImage {
public:
  const char *image_filename;
  IStarList *image_starlist;
  ImageInfo *image_info;
  int       image_index;
  int       zero_point_adjusted;
  double    comp_point;		// instrumental mag of comp
  double    zero_point;		// instrumental magnitude minus
				// zero_point gives true magnitude
  double    zero_point_sigma;
  int       color_index;
};

// each star in each image gets one of these
class EachStar {
public:
  HGSC                    *hgsc_star;
  IStarList::IStarOneStar *image_star;
  AnalysisImage           *host_image;
  int                     processed;
  EachStar                *next_star;
};

// one of these for each star for each color
struct Measurement {
  JULIAN jd_exposure_midpoint;
  double instrumental_diff; // instrumental raw
  double instrumental_mag;
  double magnitude_tr;	 // transformed
  double magnitude_err;
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
};  

// A single one of these is created for each star.
struct ResultData {
  char   A_Unique_ID[16];
  char   *common_name;
  HGSC   *hgsc_star;
  int    is_comp;
  int    is_check;
  int    do_submit;
  int    is_reference;

  Measurement measurement[NUM_FILTERS]; // indexed by filter ID

  // instrumental differential colors
  double inst_diff_b_v;
  double inst_diff_v_r;
  double inst_diff_r_i;
  double inst_diff_v_i;
  
  ResultData *next_result;
};
  
EachStar *AnalysisHead = 0;
ResultData *ResultHead = 0;

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
  double r_airmass;
  double r_rawmag;
  double r_trmag;
  double r_instmag;
  double r_V_R;
  double r_B_V;
  double r_R_I;
  double r_V_I;
  double r_mag_err;
  char *remarks;
  bool r_is_transformed;
};  

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
    } else if (strcmp(e->att_name, "REMARKS") == 0) {
      ; // ignore
    } else if (strcmp(e->att_name, "TARGET") == 0) {
      ; // ignore
    } else {
      fprintf(stderr, "bvri_pretty: read_record(): invalid element name: %s\n",
	      e->att_name);
    }
  } // end loop over all elements in the record
}  

ResultData *comp_star = 0;
std::list<ResultData *> all_results;
DBASE::DB_Record check_star_errors; // initially empty

// Add this record to the dictionary and the set of ResultData
void
process_record(HGSCList *Catalog,
	       StarNameTable *dictionary,
	       DBASE::DB_Record *r,
	       FILE *fp_out) {
  Single_Record *d = new Single_Record;
  read_record(r, d);

  ResultData *p = dictionary->Lookup(d->r_starname);
  // If the lookup found nothing, then this is the first reference to
  // a new star (not previously encountered)
  if (p == 0) {
    p = new ResultData;
    // Give the new ResultData some safe default values
    p->inst_diff_b_v =
      p->inst_diff_v_r =
      p->inst_diff_v_i =
      p->inst_diff_r_i = 99.9;
    p->is_comp = 0;
    p->is_check = 0;
    p->do_submit = 0;
    p->is_reference = 0;
    p->common_name = 0;
    dictionary->Add(d->r_starname, p);
    for (int c = 0; c < NUM_FILTERS; c++) {
      p->measurement[c].stddev_valid = 0;
      p->measurement[c].magnitude_tr = 
	p->measurement[c].instrumental_mag = 99.9;
    }

    // Use the common name to look this star up in the HGSC catalog
    // and remember this so we can compare measured magnitude to
    // cataloged photometry.
    p->hgsc_star = Catalog->FindByLabel(d->r_starname);
    if (p->hgsc_star && p->hgsc_star->do_submit) {
      p->do_submit = true;
    }
    // Add this to the "all_results"
    all_results.push_back(p);
  }

  // if this is the comp star, remember this important detail
  if (d->r_is_comp) {
    comp_star = p;
  }

  // now translate from Single_Record into ResultData
  int color = filter_to_index(d->r_filter);
  if (d->r_is_comp) p->is_comp = 1;
  if (d->r_is_check) p->is_check = 1;
  p->measurement[color].jd_exposure_midpoint = JULIAN(d->r_t_obs);
  p->common_name = strdup(d->r_starname);
  if (d->r_auid) {
    strcpy(p->A_Unique_ID, d->r_auid);
  } else {
    p->A_Unique_ID[0] = 0;
  }
  // skip airmass for now
  p->measurement[color].instrumental_mag = d->r_rawmag;
  p->measurement[color].magnitude_tr = d->r_trmag;
  p->measurement[color].magnitude_err = d->r_mag_err;
  if (d->r_V_R < 90.0) { // value of 99.9 means no value present
    p->inst_diff_v_r = d->r_V_R;
  }
  if (d->r_B_V < 90.0) {
    p->inst_diff_b_v = d->r_B_V;
  }
  if (d->r_R_I < 90.0) {
    p->inst_diff_r_i = d->r_R_I;
  }
  if (d->r_V_I < 90.0) {
    p->inst_diff_v_i = d->r_V_I;
  }
}

// This points to the name of the file that is to receive the check
// star error, for display later on. If left <nil>, then no check star
// error file is to be created.
static char *error_filename_string = 0;
static FILE *error_file = 0;

void
remember_error(double error,
	       double mag_err,
	       const char *filter,
	       const char *starname) {
  if (error_filename_string) {
    if (!error_file) {
      error_file = fopen(error_filename_string, "w");
      if (!error_file) {
	perror("Unable to create error file:");
	exit(-1);
      }
    }

    fprintf(error_file, "%lf %lf %s '%s'\n",
	    error, mag_err, filter, starname);
  }
}

void
finish_error_report(void) {
  if (error_file) {
    fclose(error_file);
  }
}

void
print_check_star_errors(FILE *fp_out) {
  // iterate over all elements in the check_star_record
  std::list<DBASE::DB_Element *>::iterator it;
  for (it = check_star_errors.elements.begin();
       it != check_star_errors.elements.end();
       it++) {
    DBASE::DB_Element *e = (*it);
    if (e->att_name[0] == 'K' &&
	e->att_name[1] == 'E' &&
	e->att_name[2] == 'R' &&
	e->att_name[3] == 'R' &&
	e->att_name[4] == '_' &&
	e->att_name[6] == 0) {
      char color_letter = e->att_name[5];
      fprintf(fp_out, "Check star error (%c) = %.4lf\n",
	      color_letter, e->value.double_value);
    }
  }
}

//****************************************************************
//        print_line()
//  Prints a single line of output from a single ResultData structure
//  (relying on the comp_star data).
//****************************************************************
void
print_color(StarNameTable *dictionary,
	    ResultData *r,
	    Filter f,
	    Measurement *m,
	    FILE *fp_out) {
  bool transformed = false;
  double mag;
  double mag_offset;

  if (m->magnitude_tr < 90.0) {
    transformed = true;
    mag = m->magnitude_tr;
  } else if (m->instrumental_mag < 90.0) {
    mag = m->instrumental_mag;
  } else {
    fprintf(fp_out, "                       ");
    return;
  }
  fprintf(fp_out, "%6.3lf%c", mag, (transformed ? 't' : ' '));

  bool mag_offset_avail = false;
  if (r->is_check) {
    PhotometryColor pc = FilterToColor(f);
    if (r->hgsc_star &&
	r->hgsc_star->multicolor_data.IsAvailable(pc)) {
      double ref_mag = r->hgsc_star->multicolor_data.Get(pc);
      mag_offset_avail = true;
      const double mag_err = ref_mag - mag;
      remember_error(mag_err,
		     m->magnitude_err,
		     AAVSO_FilterName(f),
		     r->common_name);
      mag_offset = (mag_err);
    }
  }

  if (mag_offset_avail) {
    fprintf(fp_out, "%7.3lf", mag_offset);
  } else {
    // comp and variable stars don't have an error
    fprintf(fp_out, "       ");
  }

  // Now deal with the SNR or stddev "err"
  if (m->stddev_valid) {
    fprintf(fp_out, "%7.3lf*", m->stddev);
  } else {
    fprintf(fp_out, "         ");
  }
}
  
void
print_line(StarNameTable *dictionary,
	   ResultData *r,
	   FILE *fp_out) {
  // print star name
  fprintf(fp_out, "%-14s ", r->common_name);

  // print chart name, if present
  HGSC *h = r->hgsc_star;
  if (h && h->report_ID && h->report_ID[0]) {
    fprintf(fp_out, "%-5.5s ", h->report_ID);
  } else {
    fprintf(fp_out, "      ");
  }
  
  // print comp/check or nothing
  fprintf(fp_out, "%-6s ", (r->do_submit ? "SUBMT" :
			    (r->is_comp ? "COMP" :
			     (r->is_check ? "CHECK" :
			      " "))));
  // print color
  if (r->inst_diff_b_v < 90.0) {
    fprintf(fp_out, "%6.3lf (b-v) ", r->inst_diff_b_v);
  } else if (r->inst_diff_v_r < 90.0) {
    fprintf(fp_out, "%6.3lf (v-r) ", r->inst_diff_v_r);
  } else if (r->inst_diff_r_i < 90.0) {
    fprintf(fp_out, "%6.3lf (r-i) ", r->inst_diff_r_i);
  } else if (r->inst_diff_v_i < 90.0) {
    fprintf(fp_out, "%6.3lf (v-i) ", r->inst_diff_v_i);
  } else {
    fprintf(fp_out, "             ");
  }

  for (int c = 0; c < NUM_FILTERS; c++) {
    Filter f;
    int subscript;
    switch (c) {
    case 0:
      f = Filter("Bc");
      subscript = 0;
      break;
    case 1:
      f = Filter("Vc");
      subscript = 1;
      break;
    case 2:
      f = Filter("Rc");
      subscript = 2;
      break;
    case 3:
      f = Filter("Ic");
      subscript = 3;
      break;
    }
    print_color(dictionary, r, f, &r->measurement[subscript], fp_out);
  }

  fprintf(fp_out, "\n");
}
  
//****************************************************************
//        main()
//****************************************************************
int
main(int argc, char **argv) {
  int ch;			// option character
  FILE *fp_out = 0;
  DBASE *db = 0;
  const char *target_name;

  // Command line options:
  // -n star_name       Name of region around which image was taken
  // -o output_filename
  // -i bvri.db         Name of the database to be used

  while((ch = getopt(argc, argv, "e:n:o:i:")) != -1) {
    switch(ch) {
    case 'e':
      error_filename_string = optarg;
      break;

    case 'n':			// name of star
      target_name = optarg;
      break;

    case 'o':
      fp_out = fopen(optarg, "w");
      if(!fp_out) {
	fprintf(stderr, "bvri_pretty: cannot open output file %s\n", optarg);
	exit(-2);
      }
      break;

    case 'i':
      db = new DBASE(optarg, DBASE_MODE_READONLY);
      if (!db) {
	fprintf(stderr, "bvri_pretty: cannot open database file %s\n", optarg);
	usage();
	/*NOTREACHED*/
      }
      break;

    case '?':
    default:
      usage();
    }
  }

  if(fp_out == 0 || db == 0 || target_name == 0) {
    usage();
  }

  char HGSCfilename[132];
  sprintf(HGSCfilename, CATALOG_DIR "/%s", target_name);
  FILE *HGSC_fp = fopen(HGSCfilename, "r");
  if(!HGSC_fp) {
    fprintf(stderr, "Cannot open catalog file for %s\n", target_name);
    exit(-2);
  }

  // Get the strategy for this star so we can pull off any info that's
  // of use (in particular, the REMARKS).
  Strategy::FindAllStrategies(0);
  Strategy *strategy = Strategy::FindStrategy(target_name);

  fprintf(fp_out, "##################################################\n");
  fprintf(fp_out, "                  %s\n", target_name);
  fprintf(fp_out, "##################################################\n");
  fprintf(fp_out, "\n\n");

  const char *general_remarks = strategy->remarks();
  if(general_remarks) {
    fprintf(fp_out, "%s", general_remarks);
    fprintf(fp_out, "################################################\n");
  }

  HGSCList Catalog(HGSC_fp);

  put_repeat(' ', 40, fp_out);
  fputc('|', fp_out);
  put_repeat(' ', 8, fp_out);
  fprintf(fp_out, "BLUE          |        GREEN         |         RED          |         IR\n");
  fprintf(fp_out, "Name           Chart Status    Color    |  b     del-b    err  |  v     del-v");
  fprintf(fp_out, "    err  |  r     del-r    err  |  i     del-i    err\n");
  fprintf(fp_out, "-------------- ----- ------ ------------|------  ------  ------|");
  fprintf(fp_out, "------  ------  ------|------  ------  ------|------  ------  ------\n");

  // Now loop through all the records in the db, looking for those
  // that have target_name mentioned in the record.
  StarNameTable dictionary;
  const int num_recs = db->get_number_records();
  for (int i=0; i < num_recs; i++) {
    DBASE::DB_Record r;
    if (db->get(i, &r) != DBASE_SUCCESS) {
      fprintf(stderr, "bvri_pretty: Error fetching record number %d from database.\n", i);
    } else {
      enum { TARGET_SEARCHING, TARGET_MISMATCH, TARGET_MATCH } search_state = TARGET_SEARCHING;
      
      std::list<DBASE::DB_Element *>::iterator it;
      for (it = r.elements.begin(); it != r.elements.end(); it++) {
	if (strcmp((*it)->att_name, "TARGET") == 0) {
	  if ((*it)->att_type != DBASE_TYPE_STRING) {
	    fprintf(stderr, "bvri_pretty: TARGET element has wrong type.\n");
	  } else {
	    if (strcmp(target_name, (*it)->value.char_value) == 0) {
	      search_state = TARGET_MATCH;
	    } else {
	      search_state = TARGET_MISMATCH;
	    }
	  }
	  break;
	} // end if TARGET element was found
      } // end loop to search for matching TARGET
      if (search_state == TARGET_MATCH) {
	DBASE::DB_Element *e = r.find_by_att_name("ERRORS");
	if (e) {
	  // this is a special record (an "ERRORS" record)
	  // store it into check_star_errors
	  db->get(i, &check_star_errors);
	} else {
	  // this is a normal star magnitude record
	  process_record(&Catalog, &dictionary, &r, fp_out);
	}
      }
    }
  } // end loop over all records

  // Now we can start printing lines
  std::list<ResultData *>::iterator it;
  for (it = all_results.begin(); it != all_results.end(); it++) {
    print_line(&dictionary, (*it), fp_out);
  }
  print_check_star_errors(fp_out);
  
  fclose(fp_out);
}

