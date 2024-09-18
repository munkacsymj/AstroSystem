/* This may look like C code, but it is really -*-c++-*- */
/*  lex.h -- lexical analyzer of script text for script in a strategy
 *
 *  Copyright (C) 2007 Mark J. Munkacsy
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
#ifndef _LEX_H
#define _LEX_H

#include <list>
#include <stdio.h>
#include <stdlib.h>		// free()

#define TOK_IF			1
#define TOK_SET			2
#define TOK_BRIGHTER		3
#define TOK_MAG			4
#define TOK_INTEGER		5 // see value_int
#define TOK_STRING		6 // see value
#define TOK_DOUBLE		7 // see value_double
#define TOK_EOF			8 // end of file
#define TOK_OPEN_PAREN		9
#define TOK_CLOSE_PAREN		10
#define TOK_VARIABLE		11
#define TOK_LIST                12 // used when variable assigned a
				   // list of values (not used by lex)
#define TOK_OPEN_BRACKET        13
#define TOK_CLOSE_BRACKET       14
#define TOK_DEFINE              15

enum ValueType { VAL_INT, VAL_DOUBLE, VAL_STRING, VAL_LIST, VAL_NOVAL };

class Variable;

// A Value can be a single value, a single variant value, or a single
// value list. It cannot be a set of variant values. That requires a
// set of Values (as is found in a Variable). Similarly, a Token
// (because it is associated with a single Value) cannot be directly
// associated with a set of variant values.
class Value {
public:
  Value(void) { is_variant_value = 0; variant_name = 0; value_string = 0; value_type = VAL_NOVAL; }
  ~Value(void) {
    if (variant_name) free((void *)variant_name);
    if (value_string) free((void *)value_string); }

  int is_variant_value;
  const char *variant_name;

  ValueType value_type;
  int value_int;
  double value_double;
  const char *value_string;
  std::list <Value> value_list;
};

class Token {
public:
  int TokenType;
  Value token_value;
  Variable *var;		// valid if TokenType == TOK_VARIABLE

  // This returns the TokenType in the form of a string (useful for
  // printing) 
  const char *StringType(void);
};

Token *Get_Next_Token(FILE *fp);
Token *LookAhead_Token(FILE *fp);

#endif
