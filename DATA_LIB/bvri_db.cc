// This may look like C code, but it is really -*- C++ -*-
/*  dbase_db.cc -- manage a database
 *
 *  Copyright (C) 2017 Mark J. Munkacsy

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

#include "bvri_db.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

bool isvalid(double x) { return isnormal(x) or x == 0.0; }

void DeepDelete(BVRI_REC_list *p) {
  BVRI_REC_list::iterator it;

  for (it=p->begin(); it != p->end(); it++) {
    BVRI_DB_REC *r = (*it);
    if (r->DB_fieldname) free((char *)r->DB_fieldname);
    if (r->DB_comparison_star_auid) free((char *)r->DB_comparison_star_auid);
    if (r->DB_starname) free((char *)r->DB_starname);
    if (r->DB_AUID) free((char *)r->DB_AUID);
    if (r->DB_remarks) free((char *)r->DB_remarks);
    if (r->DB_comments) free((char *)r->DB_comments);
    delete r;
  }

  delete p;
}

// Delete all records for the named star
void
BVRI_DB::DeleteStarRecords(const char *starname) {
  DBASE::DB_Element e;
  e.att_name = "TARGET";
  e.att_type = DBASE_TYPE_STRING;
  e.value.char_value = strdup(starname);
  db->erase(&e);
  free((void *) e.value.char_value);
}

// Delete the returned structure when you're done.
BVRI_DB_ERRORS *
BVRI_DB::GetErrors(const char *starname) {
  const int num_recs = db->get_number_records();
  for (int i=0; i<num_recs; i++) {
    DBASE::DB_Record r;
    if (db->get(i, &r) != DBASE_SUCCESS) {
      fprintf(stderr, "bvri_db: Error fetching record number %d from database.\n", i);
    } else {
      enum { TARGET_SEARCHING, TARGET_MISMATCH, TARGET_MATCH } search_state = TARGET_SEARCHING;
      
      std::list<DBASE::DB_Element *>::iterator it;
      for (it = r.elements.begin(); it != r.elements.end(); it++) {
	if (strcmp((*it)->att_name, "TARGET") == 0) {
	  if ((*it)->att_type != DBASE_TYPE_STRING) {
	    fprintf(stderr, "bvri_db: TARGET element has wrong type.\n");
	  } else {
	    if (strcmp(starname, (*it)->value.char_value) == 0) {
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
	  BVRI_DB_ERRORS *err = new BVRI_DB_ERRORS;
	  if ((e = r.find_by_att_name("KERR_B"))) {
	    err->DB_check_err_B = e->value.double_value;
	  } else {
	    err->DB_check_err_B = NAN;
	  }
	  if ((e = r.find_by_att_name("KERR_V"))) {
	    err->DB_check_err_V = e->value.double_value;
	  } else {
	    err->DB_check_err_V = NAN;
	  }
	  if ((e = r.find_by_att_name("KERR_R"))) {
	    err->DB_check_err_R = e->value.double_value;
	  } else {
	    err->DB_check_err_R = NAN;
	  }
	  if ((e = r.find_by_att_name("KERR_I"))) {
	    err->DB_check_err_I = e->value.double_value;
	  } else {
	    err->DB_check_err_I = NAN;
	  }
	  return err;
	}
      }
    }
  } // end loop over all records
  return 0;
}
    
// Adds the error structure. 
void
BVRI_DB::AddErrors(const char *starname, const BVRI_DB_ERRORS *errs) {
  DBASE::DB_Record *rec = new DBASE::DB_Record;
  rec->add_string("TARGET", starname);
  rec->add_string("ERRORS", "YES");
  if (isvalid(errs->DB_check_err_B)) {
    rec->add_double("KERR_B", errs->DB_check_err_B);
  }
  if (isvalid(errs->DB_check_err_V)) {
    rec->add_double("KERR_V", errs->DB_check_err_V);
  }
  if (isvalid(errs->DB_check_err_R)) {
    rec->add_double("KERR_R", errs->DB_check_err_R);
  }
  if (isvalid(errs->DB_check_err_I)) {
    rec->add_double("KERR_I", errs->DB_check_err_I);
  }
  db->append(rec);
}

BVRI_DB_REC *convert_to_bvri_db_rec(const DBASE::DB_Record *r) {
  BVRI_DB_REC *t = new BVRI_DB_REC;
  std::list<DBASE::DB_Element *>::const_iterator it;
  
  t->DB_obs_time = JULIAN(0.0);
  t->DB_fieldname = 0;
  t->DB_comparison_star_auid = 0;
  t->DB_AAVSO_filter_letter = ' ';
  t->DB_starname = 0;
  t->DB_is_comp = false;
  t->DB_is_check = false;
  t->DB_AUID = 0;
  t->DB_airmass = NAN;
  t->DB_rawmag = NAN;
  t->DB_instmag = NAN;
  t->DB_transformed_mag = NAN;
  t->DB_magerr = NAN;
  t->DB_remarks = 0;
  t->DB_comments = 0;
  t->DB_status = 0;
  t->DB_colorname[0] = 0;
  t->DB_colorvalue = NAN;

  for (it = r->elements.begin(); it != r->elements.end(); it++) {
    const DBASE::DB_Element *e = (*it);
    
    if (strcmp(e->att_name, "TOBS") == 0) {
      t->DB_obs_time = JULIAN(e->value.double_value);
    } else if (strcmp(e->att_name, "IS_COMP") == 0) {
      t->DB_is_comp = e->value.int_value;
    } else if (strcmp(e->att_name, "IS_CHECK") == 0) {
      t->DB_is_check = e->value.int_value;
    } else if (strcmp(e->att_name, "COMP") == 0) {
      t->DB_comparison_star_auid = strdup(e->value.char_value);
    } else if (strcmp(e->att_name, "FILTER") == 0) {
      // want to handle both long ("Bc") and short ("B") filter
      // strings here.
      t->DB_AAVSO_filter_letter = e->value.char_value[0];
    } else if (strcmp(e->att_name, "STARNAME") == 0) {
      t->DB_starname = strdup(e->value.char_value);
    } else if (strcmp(e->att_name, "AUID") == 0) {
      t->DB_AUID = strdup(e->value.char_value);
    } else if (strcmp(e->att_name, "AIRMASS") == 0) {
      t->DB_airmass = e->value.double_value;
    } else if (strcmp(e->att_name, "RAWMAG") == 0) {
      t->DB_rawmag = e->value.double_value;
    } else if (strcmp(e->att_name, "TRMAG") == 0) {
      t->DB_transformed_mag = e->value.double_value;
    } else if (strcmp(e->att_name, "INSTMAG") == 0) {
      t->DB_instmag = e->value.double_value;
    } else if (strcmp(e->att_name, "V_R") == 0) {
      t->DB_colorvalue = e->value.double_value;
      strcpy(t->DB_colorname, "V_R");
    } else if (strcmp(e->att_name, "B_V") == 0) {
      t->DB_colorvalue = e->value.double_value;
      strcpy(t->DB_colorname, "B_V");
    } else if (strcmp(e->att_name, "R_I") == 0) {
      t->DB_colorvalue = e->value.double_value;
      strcpy(t->DB_colorname, "R_I");
    } else if (strcmp(e->att_name, "V_I") == 0) {
      t->DB_colorvalue = e->value.double_value;
      strcpy(t->DB_colorname, "V_I");
    } else if (strcmp(e->att_name, "MAGERR") == 0) {
      t->DB_magerr = e->value.double_value;
    } else if (strcmp(e->att_name, "REMARKS") == 0) {
      t->DB_remarks = strdup(e->value.char_value);
    } else if (strcmp(e->att_name, "TARGET") == 0) {
      t->DB_fieldname = strdup(e->value.char_value);
    } else if (strcmp(e->att_name, "COMMENTS") == 0) {
      t->DB_comments = strdup(e->value.char_value);
    } else if (strcmp(e->att_name, "STATUS") == 0) {
      t->DB_status = e->value.int_value;
    } else {
      fprintf(stderr, "bvri_pretty: read_record(): invalid element name: %s\n",
	      e->att_name);
    }
  }
  return t;
}

BVRI_REC_list *
BVRI_DB::GetRecords(const char *starname) {
  const int num_recs = db->get_number_records();
  BVRI_REC_list *d = new BVRI_REC_list;
  
  for (int i=0; i<num_recs; i++) {
    DBASE::DB_Record r;
    if (db->get(i, &r) != DBASE_SUCCESS) {
      fprintf(stderr, "bvri_db: Error fetching record number %d from database.\n", i);
    } else {
      enum { TARGET_SEARCHING, TARGET_MISMATCH, TARGET_MATCH } search_state = TARGET_SEARCHING;
      
      std::list<DBASE::DB_Element *>::iterator it;
      for (it = r.elements.begin(); it != r.elements.end(); it++) {
	if (strcmp((*it)->att_name, "TARGET") == 0) {
	  if ((*it)->att_type != DBASE_TYPE_STRING) {
	    fprintf(stderr, "bvri_db: TARGET element has wrong type.\n");
	  } else {
	    if (strcmp(starname, (*it)->value.char_value) == 0) {
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
	if (!e) {
	  // This is a record to convert!
	  BVRI_DB_REC *t = convert_to_bvri_db_rec(&r);
	  d->push_back(t);
	}
      }
    }
  } // end loop over all records
  return d;
}

BVRI_REC_list *
BVRI_DB::GetAllRecords(void) {
  const int num_recs = db->get_number_records();
  BVRI_REC_list *d = new BVRI_REC_list;
  
  for (int i=0; i<num_recs; i++) {
    DBASE::DB_Record r;
    if (db->get(i, &r) != DBASE_SUCCESS) {
      fprintf(stderr,
	      "bvri_db: Error fetching record number %d from database.\n", i);
    } else {
      DBASE::DB_Element *e = r.find_by_att_name("ERRORS");
      if (!e) {
	// This is a record to convert!
	BVRI_DB_REC *t = convert_to_bvri_db_rec(&r);
	d->push_back(t);
      }
    }
  } // end loop over all records
  return d;
}

void
BVRI_DB::AddRecords(const char *starname, BVRI_REC_list *records) {
  BVRI_REC_list::iterator it;
  
  for (it=records->begin(); it != records->end(); it++) {
    BVRI_DB_REC *r = (*it);
    DBASE::DB_Record *rec = new DBASE::DB_Record;
    char temp[80];

    rec->add_double("TOBS", r->DB_obs_time.day());
    rec->add_string("TARGET", r->DB_fieldname);
    if (r->DB_comparison_star_auid) 
      rec->add_string("COMP", r->DB_comparison_star_auid);
    temp[0] = r->DB_AAVSO_filter_letter;
    temp[1] = 0;
    rec->add_string("FILTER", temp);
    rec->add_string("STARNAME", r->DB_starname);
    if (r->DB_is_comp) rec->add_int("IS_COMP", 1);
    if (r->DB_is_check) rec->add_int("IS_CHECK", 1);
    if (r->DB_AUID) rec->add_string("AUID", r->DB_AUID);
    if (isvalid(r->DB_airmass))
      rec->add_double("AIRMASS", r->DB_airmass);
    if (isvalid(r->DB_rawmag))
      rec->add_double("RAWMAG", r->DB_rawmag);
    if (isvalid(r->DB_instmag))
      rec->add_double("INSTMAG", r->DB_instmag);
    if (isvalid(r->DB_transformed_mag))
      rec->add_double("TRMAG", r->DB_transformed_mag);
    if (isvalid(r->DB_magerr))
      rec->add_double("MAGERR", r->DB_magerr);
    if (r->DB_remarks)
      rec->add_string("REMARKS", r->DB_remarks);
    if (r->DB_status)
      rec->add_int("STATUS", r->DB_status);
    if (r->DB_comments)
      rec->add_string("COMMENTS", r->DB_comments);
    if (isvalid(r->DB_colorvalue))
      rec->add_double(r->DB_colorname, r->DB_colorvalue);

    db->append(rec);
  }
}
    
