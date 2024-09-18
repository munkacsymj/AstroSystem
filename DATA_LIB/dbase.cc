// This may look like C code, but it is really -*- C++ -*-
/*  dbase.cc -- manage a database
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

#include "dbase.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Defined values for dbase_state:
#define DBS_NEW 0
#define DBS_OPEN 1
#define DBS_MODIFIED 2
#define DBS_CLOSED 3

//****************************************************************
//        starts_with()
// tests whether "long_string" starts with the string contained in
// "pattern"
//****************************************************************
bool starts_with(const char *long_string, const char *pattern) {
  while (*pattern) {
    if (*pattern != *long_string) return false;
    pattern++;
    long_string++;
  }
  return true;
}

//****************************************************************
//        bracket_split(string, char **words)
// Splits a line into three "words", each of which is enclosed in
// square brackets.
//****************************************************************
void bracket_split(const char *string, char **answer) {
  // have already verified that there are 6 brackets, but can't be
  // sure that they're the right flavor. ("[][]]]" has six brackets,
  // but it's broken)
  char *p;
  answer[0] = answer[1] = answer[2] = 0;

  if (*string != '[') {
    fprintf(stderr, "bracket_split: illegal string[0]: %s\n", string);
    return;
  }
  answer[0] = strdup(string+1);
  for (p = answer[0]; *p; p++) {
    if (*p == ']') {
      *p++ = 0;
      break;
    }
  }

  if (*p != '[') {
    fprintf(stderr, "bracket_split: illegal string[1]: %s\n", string);
    return;
  }
  answer[1] = strdup(p+1);
  for (p = answer[1]; *p; p++) {
    if (*p == ']') {
      *p++ = 0;
      break;
    }
  }

  if (*p != '[') {
    fprintf(stderr, "bracket_split: illegal string[2]: %s\n", string);
    return;
  }
  answer[2] = strdup(p+1);
  for (p = answer[2]; *p; p++) {
    if (*p == ']') {
      *p++ = 0;
      break;
    }
  }
}

//****************************************************************
//        DBASE Constructor
// (Constructor reads in all the current content of the database and
//  then closes the file.)
//****************************************************************
DBASE::DBASE(const char *pathname, int mode) {
  db_filename = strdup(pathname);
  db_mode = mode;
  
  // open the file
  db_fp = fopen(pathname, "r");

  // handle open errors
  if (!db_fp) {
    dbase_state = DBS_NEW;
    fprintf(stderr, "%s: ", pathname);
    perror("Error opening dbase: ");
    return;
  }

  // read contents
  char buffer[4096];
  DB_Record *r = 0; // current record
  int record_number = 0;
  
  while(fgets(buffer, sizeof(buffer), db_fp)) {
    // count left and right brackets
    int count = 0;
    for(char *s = buffer; *s; s++) {
      if (*s == '[' || *s == ']') count++;
    }

    // two types of lines: record start (with 2 brackets) and value
    // entry (6 brackets)
    if (count == 2) {
      if (starts_with(buffer, "[RECORD]")) {
	r = new DB_Record;
	r->record_number = record_number++;
	r->is_dirty = false;
	contents.push_back(r);
      } else {
	fprintf(stderr, "dbase: invalid 2-bracket line: %s\n", buffer);
      }
    } else if (count == 6) {
      char *words[3];
      bracket_split(buffer, words);
      DB_Element *e = new DB_Element;
      e->att_name = words[0];
      r->elements.push_back(e);
      
      // Second word must be single character (value type)
      if (words[1][1] != 0) {
	fprintf(stderr, "dbase: invalid type: %s\n", words[1]);
      } else {
	if (words[1][0] == 'S') {
	  e->att_type = DBASE_TYPE_STRING;
	  e->value.char_value = words[2];
	} else if (words[1][0] == 'D') {
	  e->att_type = DBASE_TYPE_DOUBLE;
	  e->value.double_value = strtod(words[2], 0);
	} else if (words[1][0] == 'I') {
	  e->att_type = DBASE_TYPE_INT;
	  e->value.int_value = atoi(words[2]);
	} else {
	  fprintf(stderr, "dbase: invalid type: %s\n", words[1]);
	}
      }
      free(words[1]);
      if (e->att_type != DBASE_TYPE_STRING) free(words[2]);
    } else {
      fprintf(stderr, "dbase: invalid bracket count (%d): %s\n", count, buffer);
    }
  } // end looping over all lines in the file
  // close the file when we're done
  fclose(db_fp);
  dbase_state = DBS_CLOSED;
}

//****************************************************************
//        DB_Record destructor
//****************************************************************
void
DBASE::DB_Record::erase(void) {
  std::list<DB_Element *>::iterator e;
  for (e = elements.begin(); e != elements.end(); e++) {
    DB_Element *elem = (*e);

    free((void *) elem->att_name);
    if (elem->att_type == DBASE_TYPE_STRING)
      free((void *) elem->value.char_value);
    delete elem;
  }
}

// This will erase all existing records that contain the specified element
int
DBASE::erase(DB_Element *e) {	// returns DBASE_SUCCESS/FAIL
  // iterate through all records
  std::list<DB_Record *>::iterator it;
  it = contents.begin();
  while(it != contents.end()) {
    DB_Record *r = (*it);
    bool is_match = false;
    std::list<DB_Element *>::iterator q;
    for (q = r->elements.begin(); q != r->elements.end(); q++) {
      DB_Element *elem = (*q);
      if (strcmp(e->att_name, elem->att_name) == 0 &&
	  e->att_type == elem->att_type) {
	if (e->att_type == DBASE_TYPE_DOUBLE &&
	    e->value.double_value == elem->value.double_value) {
	  is_match = true;
	}
	if (e->att_type == DBASE_TYPE_INT &&
	    e->value.int_value == elem->value.int_value) {
	  is_match = true;
	}
	if (e->att_type == DBASE_TYPE_STRING &&
	    strcmp(e->value.char_value, elem->value.char_value) == 0) {
	  is_match = true;
	}
      }
      if (is_match) {
	break; // out of "for" loop
      } // end if was a match
    } // end loop over all elements
    if (is_match) {
      it = contents.erase(it);
      r->erase();
    } else {
      it++;
    }
  } // end loop over all records
  dbase_state = DBS_MODIFIED;
  return DBASE_SUCCESS;
}

//****************************************************************
//        DBASE destructor
//****************************************************************
DBASE::~DBASE(void) {
  if (dbase_state == DBS_MODIFIED || any_record_is_dirty()) close();

  free((void *) db_filename);

  std::list<DB_Record *>::iterator it;
  for (it = contents.begin(); it != contents.end(); it++) {
    DB_Record *r = (*it);
    r->erase();
    delete r;
  }
  dbase_state = DBS_CLOSED;
}

//****************************************************************
//        DBASE destructor
//****************************************************************
bool
DBASE::any_record_is_dirty(void) {
  std::list<DB_Record *>::iterator it;
  for (it = contents.begin(); it != contents.end(); it++) {
    DB_Record *r = (*it);
    if (r->is_dirty) return true;
  }
  return false;
}

//****************************************************************
//        append() -- adds a new record
//****************************************************************
int
DBASE::append(DB_Record *record) { // returns record_number
  if (db_mode == DBASE_MODE_READONLY) {
    fprintf(stderr, "DBASE: error: attempt to append to read-only db.\n");
    return -1;
  }
  DB_Record *r = new DB_Record;
  *r = *record;
  r->is_dirty = true;
  contents.push_back(r);
  r->record_number = (contents.size() - 1);
  dbase_state = DBS_MODIFIED;
  return r->record_number;
}

//****************************************************************
//        get() -- gets an existing record
//****************************************************************
int
DBASE::get(int record_number, DB_Record *record) { // returns DBASE_SUCCESS/FAIL
  DB_Record *r = get_reference(record_number);
  if (r) {
    *record = *r;
    return DBASE_SUCCESS;
  }
  return DBASE_FAIL;
}

DBASE::DB_Record *
DBASE::get_reference(int record_number) {
  std::list<DB_Record *>::iterator it;
  for (it = contents.begin(); it != contents.end(); it++) {
    DB_Record *r = (*it);
    if (r->record_number == record_number) {
      return r;
    }
  }
  return 0;
}

//****************************************************************
//        update() -- change an existing record
//****************************************************************
int
DBASE::update(int record_number, DB_Record *record) { // returns DBASE_SUCCESS/FAIL
  if (db_mode == DBASE_MODE_READONLY) {
    fprintf(stderr, "DBASE: error: attempt to update read-only db.\n");
    return DBASE_FAIL;
  }
  std::list<DB_Record *>::iterator it;
  for (it = contents.begin(); it != contents.end(); it++) {
    DB_Record *r = (*it);
    if (r->record_number == record_number) {
      r->erase();
      contents.erase(it);
      (void) append(record);
      record->record_number = record_number;
      dbase_state = DBS_MODIFIED;
      return DBASE_SUCCESS;
    }
  }
  return DBASE_FAIL;
}

int
DBASE::update(DB_Record *record) { // returns DBASE_SUCCESS/FAIL
  if (db_mode == DBASE_MODE_READONLY) {
    fprintf(stderr, "DBASE: error: attempt to update read-only db.\n");
    return DBASE_FAIL;
  }
  dbase_state = DBS_MODIFIED;
  record->is_dirty = true;
  return DBASE_SUCCESS;
}

//****************************************************************
//        get_number_records()
//****************************************************************
int
DBASE::get_number_records(void) {
  return contents.size();
}

//****************************************************************
//        close() -- close and write records to disk
//****************************************************************
int
DBASE::close(void) { // returns DBASE_SUCCESS/FAIL
  // if read-only, a "close" is a no-op.
  if (db_mode == DBASE_MODE_READONLY) {
    return DBASE_SUCCESS;
  }
  if (any_record_is_dirty() == false && dbase_state != DBS_MODIFIED) {
    return DBASE_SUCCESS;
  }

  // truncate any existing file (delete old contents)
  db_fp = fopen(db_filename, "w");
  if (!db_fp) {
    fprintf(stderr, "dbase: cannot open for writing: %s\n", db_filename);
    perror("Error: ");
  }

  std::list<DB_Record *>::iterator it;
  for (it = contents.begin(); it != contents.end(); it++) {
    DB_Record *r = (*it);
    fprintf(db_fp, "[RECORD]\n");

    std::list<DB_Element *>::iterator e;
    for (e = r->elements.begin(); e != r->elements.end(); e++) {
      DB_Element *elem = (*e);
      char elem_buffer[128];
      char elem_letter;
      const char *elem_pointer = elem_buffer;

      if (elem->att_type == DBASE_TYPE_INT) {
	elem_letter = 'I';
	sprintf(elem_buffer, "%d", elem->value.int_value);
      } else if (elem->att_type == DBASE_TYPE_STRING) {
	elem_letter = 'S';
	elem_pointer = elem->value.char_value;
      } else if (elem->att_type == DBASE_TYPE_DOUBLE) {
	elem_letter = 'D';
	sprintf(elem_buffer, "%lf", elem->value.double_value);
      } else {
	fprintf(stderr, "dbase: close(): invalid type: %d\n", elem->att_type);
	fprintf(stderr, "    record_number = %d, element keyword = %s\n",
		r->record_number, elem->att_name);
	continue;
      }
    
      fprintf(db_fp, "[%s][%c][%s]\n", elem->att_name, elem_letter, elem_pointer);
    } // end loop over all elements in the record
  } // end loop over all records
      
  fclose(db_fp);
  dbase_state = DBS_CLOSED;
  db_fp = 0; // how we remember that the close() was performed
  return DBASE_SUCCESS;
}

//****************************************************************
//        add(element to a record)
//****************************************************************
void
DBASE::DB_Record::add_double(const char *AttName, double value) {
  DB_Element *e = new DB_Element;
  if (!e) {
    fprintf(stderr, "add_double(): unable to allocate element.\n");
  } else {
    e->att_name = strdup(AttName);
    e->att_type = DBASE_TYPE_DOUBLE;
    e->value.double_value = value;

    elements.push_back(e);
  }
  is_dirty = true;
}
  
void
DBASE::DB_Record::add_string(const char *AttName, const char *value) {
  DB_Element *e = new DB_Element;
  if (!e) {
    fprintf(stderr, "add_string(): unable to allocate element.\n");
  } else {
    e->att_name = strdup(AttName);
    e->att_type = DBASE_TYPE_STRING;
    e->value.char_value = strdup(value);

    elements.push_back(e);
  }
  is_dirty = true;
}

void
DBASE::DB_Record::add_int(const char *AttName, int value) {
  DB_Element *e = new DB_Element;
  if (!e) {
    fprintf(stderr, "add_int(): unable to allocate element.\n");
  } else {
    e->att_name = strdup(AttName);
    e->att_type = DBASE_TYPE_INT;
    e->value.int_value = value;

    elements.push_back(e);
  }
  is_dirty = true;
}  

// Look for an element with a specific name. If not found, return
// <nil>, otherwise return pointer to the element.
DBASE::DB_Element *
DBASE::DB_Record::find_by_att_name(const char *AttName) {
  std::list<DB_Element *>::iterator e;
  for (e = elements.begin(); e != elements.end(); e++) {
    DB_Element *elem = (*e);
    if (strcmp(elem->att_name, AttName) == 0) {
      return elem;
    }
  }
  return 0;
}
  
