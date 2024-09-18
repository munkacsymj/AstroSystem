// This may look like C code, but it is really -*- C++ -*-
/*  script_out.h -- writes and reads output from execution of a
 *  script embedded in a strategy
 *
 *  Copyright (C) 2007 Mark J. Munkacsy

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
#include <stdio.h>
#include <stdlib.h>
#include <list>

#define SCRIPT_LOG	      1	// message is valid
#define SCRIPT_ASSIGN_SIMPLE  3	// var_value & var_name is valid
#define SCRIPT_ASSIGN_LIST    4	// var_name & var_value_list & num_var_values
#define SCRIPT_ASSIGN_VARIANT 2	// var_name & variant & var_value are valid
#define SCRIPT_COMMENT	      5	// message is valid
#define SCRIPT_EOF	      6	// nothing is valid

class Script_Entry {
public:
  int EntryType;
  int num_var_values;		// number of values available
  char *message;
  char *var_name;
  char *variant;
  char *var_value;		// valid if num_var_values == 1
  char **var_value_list;		// valid if num_var_values > 1

  Script_Entry(void) {
    message = var_name = var_value = 0;
    num_var_values = 0;
    var_value_list = 0;
  }

  ~Script_Entry(void) {
    if(message) free(message);
    if(var_name) free(var_name);
    if(var_value) free(var_value);
    if(var_value_list) {
      for (int i=0; i<num_var_values; i++) {
	free (var_value_list[i]);
      }
      free (var_value_list);
    }
  }
  
};

class Script_Output {
public:
  Script_Output(const char *filename, int newfile);
  ~Script_Output(void);

  void UnlinkWhenDone(void);

  void Add_Entry(Script_Entry *entry);

  Script_Entry *Next_Entry(void);

private:
  FILE *fp;
  char *script_filename;
  int unlink_when_done;
};

class ParameterSet {
public:
  enum ResultStatus { PARAM_OKAY, NO_VALUE };

  ParameterSet(Script_Output *script);
  ~ParameterSet(void);

  // DefineParameter should be called before any GetValue() functions
  // are called. 
  enum ParameterType { SINGLE_VALUE, VARIANT, LIST_VALUE };
  void DefineParameter(const char *variable_name, ParameterType p_type);
		       
  // Overloaded GET functions
  int GetValueInt(const char *variable_name,
		  ResultStatus &result,
		  const char *variant_name = 0,
		  int index = 0);
  double GetValueDouble(const char *variable_name,
			ResultStatus &result,
			const char *variant_name = 0,
			int index = 0);
  const char *GetValueString(const char *variable_name,
			     ResultStatus &result,
			     const char *variant_name = 0,
			     int index = 0);

  // only valid for LIST_VALUE parameters
  int GetListSize(const char *variable_name, ResultStatus &result);

private:
  // set to 1 when the script_output is pulled into the ParameterSet
  int script_processed;
  Script_Output *script_entries;

  class Parameter {
  public:
    Parameter(void) { variable_name = variant_name = values = 0;
      value_list = 0;
      value_set = false;
      number_values =0; }
    ~Parameter(void) {
      if (variable_name) free(variable_name);
      if (variant_name) free(variant_name);
      if (values) free(values);
      if (value_list) free(value_list);
    }
    ParameterType param_type;
    char *variable_name;
    int number_values;		// only if type LIST_VALUE
    char *variant_name;		// only if type VARIANT
    char *values;		// space-separated value string
    char **value_list;		// only if type LIST_VALUE
    bool value_set;		// false until a value is established
  };

  std::list <Parameter *> all_parameters;

  void ProcessScript(void);

  Parameter *Lookup(const char *var_name, const char *variant_name);
};

    
  
