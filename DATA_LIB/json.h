/* This may look like C code, but it is really -*-c++-*- */
/*  json.h -- Implements a really crude JSON parser.
 *
 *  Copyright (C) 2022 Mark J. Munkacsy

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
#ifndef _JSON_H
#define _JSON_H
#include <string>
#include <list>

class JSON_Token;

constexpr static int JSON_READONLY = 0x01;
constexpr static int JSON_READWRITE = 0x02;

enum JSON_TYPE { JSON_SEQ, JSON_STRING, JSON_LIST, JSON_BOOL,
  JSON_FLOAT, JSON_INT, JSON_NONE, JSON_ASSIGNMENT, JSON_EMPTY };

class JSON_Expression {
 public:
  JSON_Expression(void);
  ~JSON_Expression(void) {;}
  JSON_Expression(const char *byte_string);
  JSON_Expression Lookup(const char *lookup_seq);
  JSON_Expression *Value(const char *keyword) const;
  JSON_Expression(std::list<JSON_Token *> &tokens) {InitializeFromTokens(tokens);}
  JSON_Expression(JSON_TYPE type, const char *s, JSON_Expression *value); // assignment
  JSON_Expression(JSON_TYPE type); // empty, seq, list
  JSON_Expression(JSON_TYPE type, long value); // int, bool
  JSON_Expression(JSON_TYPE type, double value); // float
  JSON_Expression(JSON_TYPE type, const char *value); // string
  JSON_Expression(JSON_TYPE type, const char *s, const char *value); // assignment
  JSON_Expression(JSON_TYPE type, const char *s, long value); // assignment
  JSON_Expression(JSON_TYPE type, const char *s, double value); // assignment
  JSON_Expression(JSON_TYPE type, const char *s, std::list<std::string> &input); // list of strings
  JSON_Expression(JSON_TYPE type, std::list<long> &input);

  void Kill(void); // does what you'd expect a destructor to do

  void Print(FILE *fp, int indent=0) const;

  JSON_Expression *GetValue(const char *dot_string) const;

  double                       Value_double(void) const;
  bool                         Value_bool(void) const;
  long                         Value_int(void) const;
  std::string                  Value_string(void) const;
  const char *                 Value_char(void) const;
  const char *                 Assignment_variable(void) const;
  JSON_Expression &GetAssignment(void) const;
  
  std::list<JSON_Expression *> &Value_list(void);
  std::list<JSON_Expression *> &Value_seq(void);
  
  bool        IsEmpty(void)  const { return j_type == JSON_EMPTY; }
  bool        IsAssignment(void) const { return j_type == JSON_ASSIGNMENT; }
  bool        IsSeq(void)    const { return j_type == JSON_SEQ; }
  bool        IsInt(void)    const { return j_type == JSON_INT; }
  bool        IsBool(void)   const { return j_type == JSON_BOOL; }
  bool        IsDouble(void) const { return j_type == JSON_FLOAT; }
  bool        IsList(void)   const { return j_type == JSON_LIST; }
  bool        IsNone(void)   const { return j_type == JSON_NONE; }
  bool        IsString(void) const { return j_type == JSON_STRING; }

  // Modification methods
  void InsertAssignmentIntoSeq(JSON_Expression *assignment); // "this" is seq
  void DeleteAssignmentFromSeq(JSON_Expression *assignment); // "this" is seq
  void InsertUpdateTSTAMPInSeq(void); // "this" is the seq
  void AddToArrayEnd(JSON_Expression *to_add); // "this" is the list
  void InsertIntoArray(JSON_Expression *to_add, int n); // "this" is the list
  // Note: the following function invalidates any active iterators
  // related to "this".  
  void DeleteFromArray(JSON_Expression *item_to_delete); // "this" is
							 // the list
  void ReplaceAssignment(JSON_Expression *new_value);
  void ReplaceAssignment(const char *key, JSON_Expression *new_value);
  JSON_Expression *FindAssignment(const char *key);
  
  JSON_Expression *CreateBlankTopLevelSeq(void);

  // Associate with filename; uses locks
  void SyncWithFile(const char *pathname, int mode);
  void WriteAndReleaseFileSync(void);
  void ReSyncWithFile(int mode, bool *anything_changed=nullptr);

  // Check validity/errors
  void Validate(void) const; // Throws SIGABRT if bad
  
 private:
  double float_val;
  const char *string_val {nullptr};
  long int_val;
  const char *assignment_variable {nullptr};
  JSON_Expression *assignment_expression {nullptr};
  std::list<JSON_Expression *> seq_val; // both SEQ's and LIST's

  int json_fd{-1}; // file descriptor (set with SyncWithFile())

  bool is_dirty {false};
  int sync_flags {0};
  struct timeval time_of_release;
  const char *file_pathname {nullptr};

  void WriteJSON(int file_fd) const;
  void InitializeFromTokens(std::list<JSON_Token *> &tokens);
  JSON_TYPE j_type {JSON_EMPTY};
  bool file_is_active {false};
};



#endif
