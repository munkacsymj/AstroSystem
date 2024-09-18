/*  script_out.cc -- writes and reads output from execution of a
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
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include "script_out.h"
#include <unistd.h>
#include <list>

/*
 * USAGE:
 * During the execution of a script, the Script_Output class is used
 * to represent the output file. Instead of writing to a file,
 * "entries" are put into the Script_Output. The Script_Output writes to
 * the file in some hidden format.  When a user wants to read the output
 * file from a script execution, a Script_Output is created and
 * associated with the filename. Then, when member function Next_Entry()
 * is called, the Script_Output will parse the file and return an
 * entry. (In other words, this file implements a mechanism to persist a
 * sequence of Entries via the filesystem.)
 */

void
Script_Output::UnlinkWhenDone(void) {
  unlink_when_done = 1;
}

Script_Output::Script_Output(const char *filename, int newfile) {

  script_filename = strdup(filename);
  unlink_when_done = 0;		// default

  if(newfile) {
    fp = fopen(filename, "w");
    if(!fp) {
      fprintf(stderr, "script_out: cannot create script output %s\n",
	      filename);
    }
  } else {
    fp = fopen(filename, "r");
    if(!fp) {
      fprintf(stderr, "script_out: cannot open script output %s\n",
	      filename);
    }
  }
}

Script_Output::~Script_Output(void) {
  if(fp) fclose(fp);

  if(unlink_when_done) unlink(script_filename);

  free(script_filename);
}

void 
Script_Output::Add_Entry(Script_Entry *entry) {
  switch(entry->EntryType) {
  case SCRIPT_LOG:
    fprintf(fp, "$%s\n", entry->message);
    break;

  case SCRIPT_ASSIGN_SIMPLE:
    fprintf(fp, "=1 %s %s\n", entry->var_name, entry->var_value);
    break;

  case SCRIPT_ASSIGN_LIST:
    fprintf(fp, "=%d %s ", entry->num_var_values, entry->var_name);
    for (int i=0; i<entry->num_var_values; i++) {
      fprintf(fp, "%s ", entry->var_value_list[i]);
    }
    fprintf(fp, "\n");
    break;

  case SCRIPT_ASSIGN_VARIANT:
    fprintf(fp, "=V %s %s %s\n",
	    entry->var_name, entry->variant, entry->var_value);
    break;

  case SCRIPT_COMMENT:
    fprintf(fp, "#%s\n", entry->message);
    break;

  case SCRIPT_EOF:
    fprintf(stderr, "script_out: cannot write EOF\n");
    break;

  default:
    fprintf(stderr, "script_out: bad entry type = %d\n", entry->EntryType);
    break;
  }
}

char *strdup_nonl(const char *s) {
  int len = 1+strlen(s);

  char *d = (char *) malloc(len);
  char *result = d;

  while(*s && *s != '\n') *d++ = *s++;
  *d = 0;

  return result;
}

Script_Entry *
Script_Output::Next_Entry(void) {
  char buffer[256];
  Script_Entry *entry = new Script_Entry;

retry:
  if(fgets(buffer, sizeof(buffer), fp) == 0) {
    entry->EntryType = SCRIPT_EOF;
  } else {
    if(buffer[0] == '$') {
      entry->EntryType = SCRIPT_LOG;
      entry->message = strdup_nonl(buffer+1);
    } else if(buffer[0] == '#') {
      entry->EntryType = SCRIPT_COMMENT;
      entry->message = strdup_nonl(buffer+1);
    } else if(buffer[0] == '=') {
      char buf_varname[64];
      char buf_varvalue[64];
      char buf_variantname[64];
      if (buffer[1] == '1') {
	// SIMPLE assignment
	entry->EntryType = SCRIPT_ASSIGN_SIMPLE;
	entry->num_var_values = 1;
	sscanf(buffer+2, "%s %s", buf_varname, buf_varvalue);
	entry->var_name = strdup(buf_varname);
	entry->var_value = strdup(buf_varvalue);
      } else if (buffer[1] == 'V') {
	// VARIANT assignment
	entry->EntryType = SCRIPT_ASSIGN_VARIANT;
	entry->num_var_values = 1;
	sscanf(buffer+2, "%s %s %s", buf_varname, buf_variantname,
	       buf_varvalue);
	entry->var_name = strdup(buf_varname);
	entry->var_value = strdup(buf_varvalue);
	entry->variant = strdup(buf_variantname);
      } else if (isdigit(buffer[1])) {
	// LIST assignment
	sscanf(buffer+1, "%d", &entry->num_var_values);

	const char *s = buffer+1;
	while (isdigit(*s)) s++; // move to end of integer

	entry->EntryType = SCRIPT_ASSIGN_LIST;
	entry->var_value_list = (char **) malloc(entry->num_var_values *
						 sizeof(char *));
	if (!entry->var_value_list) {
	  fprintf(stderr, "Script_out: error allocating memory.\n");
	  exit(-2);
	}
	for (int i=0; i<=entry->num_var_values; i++) {
	  while (isspace(*s)) s++;
	  const char *str_start = s;
	  while (!isspace(*s)) s++;
	  const int num_char = (s - str_start);
	  char *p = (char *) malloc(num_char +2);

	  // the first string is the variable name
	  if (i == 0) {
	    entry->var_name = p;
	  } else {
	    // subsequent entries are values for the list
	    entry->var_value_list[i-1] = p;
	  }
	  while (str_start < s) {
	    *p++ = *str_start++;
	  }
	  *p = 0;
	}
	fprintf(stderr, "Processed incoming list for variable '%s':\n",
		entry->var_name);
	for (int i=0; i<entry->num_var_values; i++) {
	  fprintf(stderr, "%s ", entry->var_value_list[i]);
	}
	fprintf(stderr, "\n");
      } else {
	fprintf(stderr, "script_out: invalid assignment letter '%c'\n",
		buffer[1]);
      }

    } else
      goto retry;
  }

  return entry;
}

//****************************************************************
//        ParameterSet
//****************************************************************
ParameterSet::ParameterSet(Script_Output *script) {
  script_processed = 0;
  script_entries = script;
}

ParameterSet::~ParameterSet(void) {
  for (std::list <Parameter *>::iterator it = all_parameters.begin();
       it != all_parameters.end(); it++) {
    Parameter *p = (*it);
    if (p->variable_name) free(p->variable_name);
    if (p->variant_name) free(p->variant_name);
    if (p->values) free(p->values);
  }
}

void
ParameterSet::DefineParameter(const char *variable_name,
			      ParameterType p_type) {
  if (script_processed) {
    fprintf(stderr,
	    "script_out: DefineParameter() called after script processed.\n");
    return;
  }

  Parameter *p = new Parameter;
  p->param_type = p_type;
  p->variable_name = strdup(variable_name);
  p->value_set = false;

  all_parameters.push_back(p);
}

// Overloaded GET functions
int
ParameterSet::GetValueInt(const char *variable_name,
			  ResultStatus &result,
			  const char *variant_name,
			  int index) {
  const char *ret_string = GetValueString(variable_name,
					  result,
					  variant_name,
					  index);
  if (result == PARAM_OKAY) {
    int return_int;
    sscanf(ret_string, "%d", &return_int);
    return return_int;
  } else {
    return 0;
  }
}


double
ParameterSet::GetValueDouble(const char *variable_name,
			     ResultStatus &result,
			     const char *variant_name,
			     int index) {
  const char *ret_string = GetValueString(variable_name,
					  result,
					  variant_name,
					  index);
  if (result == PARAM_OKAY) {
    double return_double;
    sscanf(ret_string, "%lf", &return_double);
    return return_double;
  } else {
    return 0.0;
  }
}

const char *
ParameterSet::GetValueString(const char *variable_name,
			     ResultStatus &result,
			     const char *variant_name,
			     int index) {
  // This has meaning if we end up looking at a variant
  Parameter *default_parameter = 0;
  
  if (script_processed == 0) ProcessScript();
  result = NO_VALUE;

  for (std::list <Parameter *>::iterator it = all_parameters.begin();
       it != all_parameters.end(); it++) {
    Parameter *p = (*it);
    if (p->value_set == false) continue;

    if (strcmp(p->variable_name, variable_name) == 0) {
      // variable name match!
      switch (p->param_type) {
      case SINGLE_VALUE:
	if (variant_name == 0) {
	  result = PARAM_OKAY;
	  return p->values;
	}
	// looking for a variant. This becomes the default value.
	default_parameter = p;
	break;

      case VARIANT:
	if (variant_name) {
	  if (strcmp(variant_name, p->variant_name) == 0) {
	    // both variable and variant match!
	    result = PARAM_OKAY;
	    return p->values;
	  }
	}
	break;

      case LIST_VALUE:
	if (index >= p->number_values || index < 0) {
	  fprintf(stderr, "script_out: invalid index (%d) for variable %s\n",
		  index, variable_name);
	  return 0;
	}
	result = PARAM_OKAY;
	return p->value_list[index];
      }
    }
  }
  // getting here means we didn't have a perfect match.
  if (default_parameter) {
    result = PARAM_OKAY;
    return default_parameter->values;
  }
  fprintf(stderr, "script_out: no value for variable %s\n", variable_name);
  return 0;
}

ParameterSet::Parameter *
ParameterSet::Lookup(const char *var_name,
		     const char *variant_name) {
  for (std::list <Parameter *>::iterator it = all_parameters.begin();
       it != all_parameters.end(); it++) {
    Parameter *p = (*it);

    if (strcmp(p->variable_name, var_name) == 0) {
      if (variant_name == 0) return p;
      if (p->variant_name && (strcmp(p->variant_name, variant_name) == 0)) {
	return p;
      }
    }
  }
  return 0;
}

void
ParameterSet::ProcessScript(void) {
  int any_errors = 0;
  
  if (script_processed == 0) {
    script_processed = 1;

    Script_Entry *e;
    Parameter *p;

    while ((e = script_entries->Next_Entry())) {
      if (e->EntryType == SCRIPT_EOF) break;
      switch (e->EntryType) {

      case SCRIPT_LOG:
      case SCRIPT_COMMENT:
	// ignore these
	break;

      case SCRIPT_ASSIGN_SIMPLE:
	p = Lookup(e->var_name, 0);
	if (!p) {
	  p = new Parameter;
	  p->param_type = SINGLE_VALUE;
	  p->variable_name = strdup(e->var_name);
	  all_parameters.push_back(p);
	}
	// variable found. Verify type matches
	if (p->param_type != SINGLE_VALUE) {
	  fprintf(stderr, "Script type mismatch for %s\n", e->var_name);
	  any_errors++;
	} else {
	  p->value_set = true;
	  p->values = strdup(e->var_value);
	}
	break;

      case SCRIPT_ASSIGN_LIST:
	p = Lookup(e->var_name, 0); // no variant
	if (!p) {
	  p = new Parameter;
	  p->param_type = LIST_VALUE;
	  p->variable_name = strdup(e->var_name);
	  all_parameters.push_back(p);
	}
	// variable found. Verify type matches
	if (p->param_type != LIST_VALUE) {
	  fprintf(stderr, "Script type mismatch for %s\n", e->var_name);
	  any_errors++;
	} else {
	  p->number_values = e->num_var_values;
	  p->value_list = (char **) malloc(sizeof(char *) * e->num_var_values);
	  for (int i=0; i<e->num_var_values; i++) {
	    p->value_list[i] = strdup(e->var_value_list[i]);
	  }
	  p->value_set = true;
	}
	break;
	  

      case SCRIPT_ASSIGN_VARIANT:
	p = Lookup(e->var_name, e->variant);
	if (!p) {
	  p = new Parameter;
	  p->param_type = VARIANT;
	  p->variable_name = strdup(e->var_name);
	  p->variant_name = strdup(e->variant);
	  all_parameters.push_back(p);
	}
	// variable found. Verify type matches
	if (p->param_type != VARIANT) {
	  fprintf(stderr, "Script type mismatch for %s\n", e->var_name);
	  any_errors++;
	} else {
	  p->value_set = true;
	  p->values = strdup(e->var_value);
	}
	break;

      default:
	fprintf(stderr, "script_out: bad entry type: %d\n",
		e->EntryType);
      }
    }
  }
}
  
// only valid for LIST_VALUE parameters
int
ParameterSet::GetListSize(const char *variable_name, ResultStatus &result) {
  if (script_processed == 0) ProcessScript();

  result = NO_VALUE;
  for (std::list <Parameter *>::iterator it = all_parameters.begin();
       it != all_parameters.end(); it++) {
    Parameter *p = (*it);
    if (p->value_set && strcmp(p->variable_name, variable_name) == 0) {
      if (p->param_type != LIST_VALUE) {
	fprintf(stderr, "Script_out: Warning: GetListSize(%s): not a list\n",
		variable_name);
      } else {
	result = PARAM_OKAY;
	return p->number_values;
      }
    }
  }
  fprintf(stderr, "Script_out: Warning: GetListSize(%s): problem.\n",
	  variable_name);
  return -1;
}

  
