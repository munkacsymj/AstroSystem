/*  aavso_photometry.cc -- parses AAVSO response to VSP request for photmetry table
 *
 *  Copyright (C) 2020 Mark J. Munkacsy

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
#include "aavso_photometry.h"
#include <ctype.h>
#include <string.h>
#include <list>
#include <stdlib.h>

struct FieldPair {
  const char *fieldname;
  const char *fieldvalue;
};

class PPair;
class PObject;
class PRecord;
class PList;
typedef std::list<PPair *> PairList;
typedef std::list<PObject *> PObjectList;

enum PObjectType { PTRecord, PTList, PTPair, PTBoolean, PTNumber, PTString };

class PObject {
public:
  PObject(PObjectType t) { p_type = t; }
  virtual ~PObject(void) {;}
  PObjectType p_type;

  bool ObjectIsRecord(void);
  bool ObjectIsList(void);
  bool ObjectIsPair(void);
  bool ObjectIsString(void);
  bool ObjectIsNumber(void);
  PRecord *AsRecord(void);
  PList *AsList(void);
  PPair *AsPair(void);
  const char *AsString(void);
  double AsNumber(void);
};

class PRecord : public PObject {
public:
  PRecord(void) : PObject(PTRecord) {;}
  ~PRecord(void) {;}
  PairList pairs;
  PObject *GetValue(const char *fieldname);
};

class PList : public PObject {
public:
  PList(void) : PObject(PTList) {;}
  ~PList(void) {;}
  PObjectList items;
};

class PPair : public PObject {
public:
  PPair(void) : PObject(PTPair) {;}
  ~PPair(void) { free((void *)fieldname); delete value_object; }
  const char *fieldname;
  PObject *value_object;
};

class PBoolean : public PObject {
public:
  PBoolean(void) : PObject(PTBoolean) {;}
  ~PBoolean(void) {;}
  bool bool_value;
};

class PNumber : public PObject {
public:
  PNumber(void) : PObject(PTNumber) {;}
  ~PNumber(void) {;}
  double number_value;
};

class PString : public PObject {
public:
  PString(void) : PObject(PTString) {;}
  ~PString(void) { free((void *)string_value); }
  const char *string_value;
};

PObject *ParseObject(const char *buffer, const char **next);
PString *ParseString(const char *buffer, const char **next);
PPair *ParsePair(const char *buffer, const char **next);
PRecord *ParseRecord(const char *buffer, const char **next);
PList *ParseList(const char *buffer, const char **next);
PBoolean *ParseBoolean(const char *buffer, const char **next);
PNumber *ParseNumber(const char *buffer, const char **next);

bool buffer_match(const char *phrase, const char *buffer) {
  while(*phrase) {
    if (*phrase != *buffer) return false;
    phrase++;
    buffer++;
  }
  return true;
}

// "start" and "end" should point to the two quotation marks at either
// end of the string. They will not be copied.

const char *mystringdup(const char *start, const char *end) {
  const unsigned long str_length = (end-start)+1;
  char *copy = (char *) malloc(str_length+1);
  if (!copy) {
    fprintf(stderr, "mystringdup(): malloc error.\n");
    return 0;
  }

  const char *s = start+1;
  char *d = copy;
  while (s != end) {
    *d++ = *s++;
  }
  *d = 0;

  return copy;
}

PObject *ParseObject(const char *buffer, const char **next) {
  if (*buffer == '{') return ParseRecord(buffer, next);
  if (*buffer == '[') return ParseList(buffer, next);
  if (isdigit(*buffer) || (*buffer == '-')) return ParseNumber(buffer, next);
  if (buffer_match("true", buffer) || buffer_match("false", buffer)) {
    return ParseBoolean(buffer, next);
  }
  if (buffer_match("null", buffer)) {
    return ParseBoolean(buffer, next);
  }
  if (*buffer == '"') return ParseString(buffer, next);
  fprintf(stderr, "ParseObject(): parse error. Next char = '%c'\n", *buffer);
  return 0;
}

PString *ParseString(const char *buffer, const char **next) {
  if (*buffer != '"') {
    fprintf(stderr, "ParseString(): invalid initial quote\n");
    return 0;
  }

  const char *s;
  for (s=buffer+1; *s != '"'; s++) {
    ;
  }
  PString *ps = new PString;
  ps->string_value = mystringdup(buffer, s);
  fprintf(stderr, "ParseString() returned string %s\n", ps->string_value);
  *next = s+1;
  return ps;
}

PPair *ParsePair(const char *buffer, const char **next) {
  PPair *result = new PPair;
  PObject *first = ParseObject(buffer, next);
  if (first->p_type != PTString) {
    fprintf(stderr, "ParsePair(): fieldname is not string.\n");
    return 0;
  } else {
    PString *field_name = (PString *) first;
    result->fieldname = field_name->string_value;
  }
  
  // verify ':' separates field name and value
  if (**next != ':') {
    fprintf(stderr, "ParsePair(): ':' not between name and value.\n");
    return 0;
  }
  const char *p = (*next)+1;
  result->value_object = ParseObject(p, next);
  fprintf(stderr, "ParsePair() returned value for keyword %s\n", result->fieldname);
  return result;
}

PRecord *ParseRecord(const char *buffer, const char **next) {
  if (*buffer != '{') {
    fprintf(stderr, "parseRecord: record doesn't start with { character\n");
    return 0;
  }
  PRecord *fpl = new PRecord;
  buffer++;
  do {
    PPair *p = ParsePair(buffer, next);
    fpl->pairs.push_back(p);
    // Next character can be either a comma or a closing brace }
    if (**next == '}') {
      (*next)++;
      fprintf(stderr, "ParseRecord() returned %ld pairs.\n", fpl->pairs.size());
      return fpl;
    }
    if (**next != ',') {
      fprintf(stderr, "ParseRecord(): expected comma. Found '%c'\n", **next);
      fprintf(stderr, "%s\n", *next);
      return fpl;
    }
    buffer = (*next)+1;
  } while(1);
  /*NOTREACHED*/
  return 0;
}
	
PList *ParseList(const char *buffer, const char **next) {
  if (*buffer != '[') {
    fprintf(stderr, "parseList: list doesn't start with [ character\n");
    return 0;
  }
      
  PList *fpl = new PList;
  buffer++;
  if (*buffer == ']') { // empty list
    *next = buffer+1;
    return fpl;
  }
  do {
    PObject *p = ParseObject(buffer, next);
    fpl->items.push_back(p);
    // Next character can be either a comma or a closing bracket ]
    if (**next == ']') {
      (*next)++;
      fprintf(stderr, "ParseList() returns %ld items.\n", fpl->items.size());
      return fpl;
    }
    if (**next != ',') {
      fprintf(stderr, "ParseList(): expected comma.\n");
      return fpl;
    }
    buffer = (*next)+1;
  } while(1);
  /*NOTREACHED*/
  return 0;
}

PBoolean *ParseBoolean(const char *buffer, const char **next) {
  PBoolean *p = new PBoolean;
  if (buffer_match("true", buffer)) {
    (*next) = buffer+4;
    p->bool_value = true;
  } else if (buffer_match("false", buffer)) {
    (*next) = buffer+5;
    p->bool_value = false;
  } else if (buffer_match("null", buffer)) {
    (*next) = buffer+4;
    p->bool_value = false;
  } else {
    fprintf(stderr, "ParseBoolean(): Bad boolean value.\n");
    return 0;
  }
  fprintf(stderr, "ParseBoolean() returns %d\n", p->bool_value);
  return p;
}



PNumber *ParseNumber(const char *buffer, const char **next) {
  PNumber *p = new PNumber;
  const char *s = buffer;
  while (isdigit(*s) || (*s == '.') || (*s == '-') || (*s == '+')) {
    s++;
  }
  (*next) = s;
  sscanf(buffer, "%lf", &p->number_value);
  fprintf(stderr, "ParseNumber() returns %.1lf\n", p->number_value);
  return p;
}

bool PObject::ObjectIsRecord(void) { return p_type == PTRecord; }
bool PObject::ObjectIsList(void) { return p_type == PTList; }
bool PObject::ObjectIsPair(void) { return p_type == PTPair; }
bool PObject::ObjectIsString(void) { return p_type == PTString; }
bool PObject::ObjectIsNumber(void) { return p_type == PTNumber; }
PRecord *PObject::AsRecord(void) { return dynamic_cast<PRecord *>(this); }
PList *PObject::AsList(void) { return dynamic_cast<PList *>(this); }
PPair *PObject::AsPair(void) { return dynamic_cast<PPair *>(this); }
const char *PObject::AsString(void) { return (dynamic_cast<PString *>(this))->string_value; }
double PObject::AsNumber(void) { return (dynamic_cast<PNumber *>(this))->number_value; }

PObject *
PRecord::GetValue(const char *fieldname) {
  for(auto item : pairs) {
    if (!item->ObjectIsPair()) {
      fprintf(stderr, "ERROR: item in record is not pair.\n");
    } else {
      PPair *p = item->AsPair();
      if (strcmp(p->fieldname, fieldname) == 0) {
	return p->value_object;
      }
    }
  }
  fprintf(stderr, "ERROR: record doesn't contain field '%s'\n",
	  fieldname);
  return 0;
}

PhotometryColor StringToColor(const char *filtername) {
  if (strcmp(filtername, "U") == 0) return PHOT_U;
  if (strcmp(filtername, "B") == 0) return PHOT_B;
  if (strcmp(filtername, "V") == 0) return PHOT_V;
  if (strcmp(filtername, "Rc") == 0) return PHOT_R;
  if (strcmp(filtername, "Ic") == 0) return PHOT_I;
  fprintf(stderr, "StringToColor(): invalid filtername: %s\n",
	  filtername);
  return PHOT_NONE;
}

PhotometryRecord *NewPhotometryRecord(PRecord *r) {
  PhotometryRecord *result = new PhotometryRecord;
  const char *auid = r->GetValue("auid")->AsString();
  PObject *label_obj = r->GetValue("label");

  // Sometimes a "label" is provided as a string, other times as a number
  const char *label = 0;
  if (label_obj->ObjectIsNumber()) {
    static char num_label[12];
    sprintf(num_label, "%.0lf", label_obj->AsNumber());
    label = num_label;
  }
  if (label_obj->ObjectIsString()) {
    label = label_obj->AsString();
  }

  //const char *chartid = r->GetValue("chartid")->AsString();
  const char *ra_string = r->GetValue("ra")->AsString();
  const char *dec_string = r->GetValue("dec")->AsString();
  PList *p_list = r->GetValue("bands")->AsList();

  strcpy(result->PR_AUID, auid);
  result->PR_ChartID[0] = 0;
  //strcpy(result->PR_ChartID, chartid);
  strcpy(result->PR_chart_label, label);

  int status = STATUS_OK;
  DEC_RA loc(dec_string, ra_string, status);
  if (status != STATUS_OK) {
    fprintf(stderr, "Bad dec/ra = %s, %s\n",
	    dec_string, ra_string);
  } else {
    result->PR_location = loc;
  }

  for (auto band : p_list->items) {
    static std::list<const char *> relevant_filter_names =
      { "B", "V", "Rc", "Ic", "U" };
    PRecord *measurement = band->AsRecord();
    // filter_name one of { U V B Rc Ic }
    const char *filter_name = measurement->GetValue("band")->AsString();
    bool relevant = false;
    for (const char *f : relevant_filter_names) {
      if (strcmp(f, filter_name) == 0) {
	relevant = true;
	break;
      }
    }

    if (relevant) {
      double magnitude = measurement->GetValue("mag")->AsNumber();
      PObject *uncty_link = measurement->GetValue("error");
      double uncertainty = -1.0;
      if (uncty_link) {
	uncertainty = uncty_link->AsNumber();
      }
      result->PR_colordata.Add(StringToColor(filter_name), magnitude, uncertainty);
    }
  }

  if (result->PR_colordata.IsAvailable(PHOT_V)) {
    result->PR_V_mag = result->PR_colordata.Get(PHOT_V);
  }
  
  return result;
}

//****************************************************************
//        ParseAAVSOResponse()
//    Takes a .json string from AAVSO and turns it into a set of
//    Photometry Records.
//****************************************************************
PhotometryRecordSet *ParseAAVSOResponse(const char *buffer) {
  // I know this is ugly. I apologize. It should be fixed.
  extern char last_sequence_name[];
  
  PhotometryRecordSet *prs = new PhotometryRecordSet;
  const char *next = buffer;
  PObject *top = ParseObject(buffer, &next);
  fprintf(stderr, "Done parsing.\n");
  
  // grab the chart ID from the top-level record
  PRecord *r = top->AsRecord();
  PObject *chart_obj = r->GetValue("chartid");
  // if ChartID is missing, then something is seriously wrong.
  if (!chart_obj) return 0;
  const char *chartid = chart_obj->AsString();
  
  fprintf(stderr, "ChartID = %s\n", chartid);
  strcpy(last_sequence_name, chartid);
  
  PList *ph = r->GetValue("photometry")->AsList();
  for (auto item : ph->items) {
    PRecord *check_star = item->AsRecord();
    fprintf(stderr, "%s\n", check_star->GetValue("auid")->AsString());
    PhotometryRecord *pr = NewPhotometryRecord(check_star);
    prs->push_back(pr);
  }
  return prs;
}

