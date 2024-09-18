// This may look like C code, but it is really -*- C++ -*-
/*  bvri_db.h -- manage a BVRI database
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

#ifndef _BVRI_DB_H
#define _BVRI_DB_H

#include "dbase.h"
#include "julian.h"

// Some members of this structure may be undefined. Use the following
// conventions to test for (and to indicate) undefined entries:
//    const char * -- set to <nil>
//    double -- set to NAN [test with isnormal()]
// Special rules: setting DB_colorvalue to NAN invalidates DB_colorname
//                setting DB_AAVSO_filter_letter to ' ' invalidates // it.
//
struct BVRI_DB_REC {
  JULIAN DB_obs_time;
  const char *DB_fieldname; // name of the target (variable)
  const char *DB_comparison_star_auid;
  char DB_AAVSO_filter_letter;
  const char *DB_starname; // typically, HGSC label
  bool DB_is_comp;
  bool DB_is_check;
  const char *DB_AUID;
  double DB_airmass;
  double DB_rawmag; // not transformed
  double DB_instmag; // instrumental (raw) mag
  double DB_transformed_mag;
  double DB_magerr;
  const char *DB_remarks; // "remarks" go into the report
  char DB_colorname[4]; // e.g., "B_V" means (b-v)
  double DB_colorvalue;
  int DB_status; // see below for flags
  const char *DB_comments; // "comments" are for my own private use
};

#define DB_FLAG_EXCLUDE 0x01

struct BVRI_DB_ERRORS {
  const char *DB_fieldname; // name of the target (variable)
  double DB_check_err_B;
  double DB_check_err_V;
  double DB_check_err_R;
  double DB_check_err_I;
};

typedef std::list<BVRI_DB_REC *> BVRI_REC_list;
void DeepDelete(BVRI_REC_list *p);

class BVRI_DB {
 public:
  // If you want to update the database, must supply mode=DBASE_MODE_WRITE
  BVRI_DB(const char *name, int mode=DBASE_MODE_READONLY) { db = new DBASE(name, mode); }
  // Warning: This will not write the database if it was changed. If
  // you want it written, you must invoke Close() first.
  ~BVRI_DB(void) { delete db; }

  // Save all changes onto the disk.
  void Close(void) { (void) db->close(); }

  // Returns the number of records currently in the database
  // (including unwritten changes, if any)
  int NumRecords(void) { return db->get_number_records(); }

  // Delete all records for the named star
  void DeleteStarRecords(const char *starname);

  // Delete the returned structure when you're done.
  BVRI_DB_ERRORS *GetErrors(const char *starname);

  // Adds the error structure. 
  void AddErrors(const char *starname, const BVRI_DB_ERRORS *errs);

  BVRI_REC_list *GetRecords(const char *starname);
  BVRI_REC_list *GetAllRecords(void);
  void AddRecords(const char *starname, BVRI_REC_list *records);


 private:
  DBASE *db;
};


#endif
