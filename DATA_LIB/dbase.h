// This may look like C code, but it is really -*- C++ -*-
/*  dbase.h -- manage a database
 *
 *  Copyright (C) 2016 Mark J. Munkacsy

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

#ifndef _DBASE_H
#define _DBASE_H

#include <stdio.h>
#include <list>

#define DBASE_MODE_READONLY 0
#define DBASE_MODE_WRITE 1

#define DBASE_SUCCESS 0
#define DBASE_FAIL 1

#define DBASE_TYPE_INT 0
#define DBASE_TYPE_DOUBLE 1
#define DBASE_TYPE_STRING 2

class DBASE {
public:
  struct DB_Element {
    const char *att_name;
    int att_type; // e.g., DBASE_TYPE_DOUBLE
    union {
      double double_value;
      int    int_value;
      const char *char_value;
    } value;
  };

  struct DB_Record {
    int record_number;
    bool is_dirty;
    std::list<DB_Element *> elements;
    void erase(void);

    void add_double(const char *AttName, double value);
    void add_string(const char *AttName, const char *value);
    void add_int(const char *AttName, int value);
    DB_Element *find_by_att_name(const char *AttName);
  };

 public:
  DBASE(const char *pathname, int mode = DBASE_MODE_READONLY);
  ~DBASE(void);

  // This will erase all existing records that contain the specified element
  int erase(DB_Element *e); // returns DBASE_SUCCESS/FAIL
  int append(DB_Record *record); // returns record_number
  int get(int record_number, DB_Record *record); // returns DBASE_SUCCESS/FAIL
  DB_Record *get_reference(int record_number);
  int update(int record_number, DB_Record *record); // returns DBASE_SUCCESS/FAIL
  int update(DB_Record *record); // returns DBASE_SUCCESS/FAIL
  int get_number_records(void);
  int close(void); // returns DBASE_SUCCESS/FAIL

 private:
  FILE *db_fp;
  int db_mode; // DBASE_MODE_READONLY/WRITE
  const char *db_filename;
  std::list<DB_Record *> contents;
  int dbase_state; // values defined in dbase.cc

  bool any_record_is_dirty(void);
};

class OBS_DBASE : public DBASE {
public:
  OBS_DBASE(const char *pathname, int mode = DBASE_MODE_READONLY);
  ~OBS_DBASE(void);

  int find(const char *target,
	   const char *starname,
	   const char *filter);
};

#endif
