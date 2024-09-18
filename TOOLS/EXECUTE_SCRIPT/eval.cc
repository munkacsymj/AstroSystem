/*  eval.cc -- Interpreter that executes the tokens from a strategy script.
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
#include "eval.h"
#include <string.h>
#include "lex.h"
#include <mag_from_image.h>		// magnitude_from_image()
#include "execute_script.h"
#include <list>

//****************************************************************
//        Substitutes
//****************************************************************
struct Substitute {
  const char *replacement_value;
  const char *typed_value;
};

std::list<Substitute *> all_substitutes;

const char *find_substitute(const char *entered_value);
void add_substitute(const char *rep_val, const char *typed_val);

//****************************************************************
//        Variables
//****************************************************************
class Variable {
public:
  char *var_name;
  enum { VAR_SIMPLE, VAR_VARIANT, VAR_LIST, VAR_VOID } var_type;
  std::list <Value> var_assignments;

  Variable *next;
};

Variable *first_variable = 0;

Token *eval_to_literal(Token *t) {
  if(t->TokenType == TOK_VARIABLE) {
    if (t->var->var_type != Variable::VAR_SIMPLE || t->var->var_assignments.size() != 1) {
      fprintf(stderr, "eval: cannot fetch value of variable of this type.\n");
      return t;
    }
      
    Token *lit = new Token;
    // grab the first value
    Value *var_value = &(t->var->var_assignments.front());

    lit->token_value = *var_value;
    if (var_value->value_type == VAL_INT) {
      lit->TokenType = TOK_INTEGER;
    } else if (var_value->value_type == VAL_DOUBLE) {
      lit->TokenType = TOK_DOUBLE;
    } else if (var_value->value_type == VAL_STRING) {
      lit->TokenType = TOK_STRING;
    } else {
      fprintf(stderr, "eval: Invalid simple variable value.\n");
    }

    return lit;
  }
  return t;
}
  

Token *eval_exp_seq(FILE *fp,
		    int execute,
		    int need_val,
		    int must_be_variable) {
  Token *t;
  while ((t = LookAhead_Token(fp))->TokenType == TOK_OPEN_PAREN) {
    eval(fp, execute, need_val, must_be_variable);
  }
  return t;
}

Token *eval(FILE *fp, int execute, int need_val, int must_be_variable) {
  Token *token = Get_Next_Token(fp);

  fprintf(stderr, "Starting eval with token %s\n", token->StringType());

  if(token->TokenType == TOK_EOF) {
    return token;
  }

  if(token->TokenType == TOK_INTEGER ||
     token->TokenType == TOK_DOUBLE) {
    return token;
  }

  if(token->TokenType == TOK_STRING) {
    // if must_be_variable is set, then this will define a new
    // variable if it isn't already a variable. Otherwise, if this
    // isn't a variable, it will be kept a string.
    Variable *v;

    for(v=first_variable; v; v=v->next) {
      if(strcmp(v->var_name, token->token_value.value_string) == 0) {
	break;
      }
    }

    if(!v) {
      if (must_be_variable) {
	// is a new variable name: create a new variable
	v = new Variable;
	v->next = first_variable;
	first_variable = v;
	v->var_name = strdup(token->token_value.value_string);
	v->var_type = Variable::VAR_VOID;	// means no value defined
      } else {
	return token; // stays a string
      }
    }
    
    token->TokenType = TOK_VARIABLE;
    token->var = v;
    return token;
  }

  if (token->TokenType == TOK_OPEN_PAREN) {
    Token *return_value = 0;
    token = LookAhead_Token(fp);
    if (token->TokenType == TOK_OPEN_PAREN) {
      token = eval_exp_seq(fp, execute, need_val, must_be_variable);
      if (token->TokenType != TOK_CLOSE_PAREN) {
	fprintf(stderr, "  Syntax error in sequence list.\n");
	return 0;
      } else {
	// eat the final closing paren
	token = Get_Next_Token(fp);
      }
      return token;
    }
    
    token = Get_Next_Token(fp);
    fprintf(stderr, "    (eval function = %s)\n", token->StringType());

    switch(token->TokenType) {
    case TOK_IF:
      {
	Token *test_val = eval(fp, execute, 1);
	int is_true;

	if(test_val->TokenType != TOK_INTEGER) {
	  fprintf(stderr, "<if> clause must evaluate to integer\n");
	  eval(fp, 0, 0);
	  eval(fp, 0, 0);
	} else {
	  is_true = (test_val->token_value.value_int != 0);

	  if(is_true) {
	    eval(fp, execute, 0);
	    eval(fp, 0, 0);
	  } else {
	    eval(fp, 0, 0);
	    eval(fp, execute, 0);
	  }
	}
      }
      break;

    case TOK_DEFINE:
      {
	Token *t1 = eval(fp, execute, 1);
	Token *t2 = eval(fp, execute, 1);

	if (t1->TokenType != TOK_STRING ||
	    t2->TokenType != TOK_STRING) {
	  fprintf(stderr, "<define> must receive two strings\n");
	  return 0;
	}
	add_substitute(t2->token_value.value_string,
		       t1->token_value.value_string);
	delete t1;
	delete t2;
      }
      break;

      // this handles simple value set and variant value set
    case TOK_SET:
      {
	Token *var_tok = eval(fp, execute, 1, 1/*MUST_BE_VARIABLE*/);
	if(var_tok->TokenType != TOK_VARIABLE) {
	  fprintf(stderr, "<set> must operate on variable\n");
	  return 0;
	}

	// we don't yet know how many values we will encounter in this
	// SET. 
	Token *var_value = eval(fp, execute, 1); // either a value or
						 // a variant name

	if (var_value->TokenType != TOK_CLOSE_PAREN) {
	  Token *second_value = eval(fp, execute, 1);

	  if (second_value->TokenType != TOK_CLOSE_PAREN) {

	    // This HAS to be a variant assignment
	    if ((second_value->TokenType != TOK_INTEGER &&
		 second_value->TokenType != TOK_STRING &&
		 second_value->TokenType != TOK_DOUBLE) ||
		var_value->TokenType != TOK_STRING) {
	      fprintf(stderr, "eval: improper SET structure.\n");
	    } else {
	      // otherwise okay
	      if (var_tok->var->var_type == Variable::VAR_LIST) {
		fprintf(stderr, "eval: improper SET of variant to LIST variable %s\n",
			var_tok->var->var_name);
	      } else {
		if (execute) {
		  var_tok->var->var_type = Variable::VAR_VARIANT;
		  second_value->token_value.is_variant_value = 1;
		  second_value->token_value.variant_name = strdup(var_value->token_value.value_string);
		  var_tok->var->var_assignments.push_back(second_value->token_value);
		}
	      }
	    }
	    if (eval(fp, 0, 0)->TokenType != TOK_CLOSE_PAREN) {
	      fprintf(stderr, "eval: syntax error: expected ')' to end SET variant.\n");
	    }
	  } else {
	    // not a variant SET: either a simple value or a LIST
	    // assignment
	    if (var_value->TokenType == TOK_LIST) {
	      // LIST assignment
	      if (var_tok->var->var_type == Variable::VAR_VARIANT ||
		  var_tok->var->var_type == Variable::VAR_SIMPLE) {
		fprintf(stderr, "eval: err: cannot assign LIST to SIMPLE or VARIANT variable %s\n", 
			var_tok->var->var_name);
	      } else {
		if (execute) {
		  var_tok->var->var_assignments.push_back(var_value->token_value);
		  var_tok->var->var_type = Variable::VAR_LIST;
		}
	      }
	    } else {
	      // SIMPLE value assignment
	      if (var_tok->var->var_type == Variable::VAR_LIST) {
		fprintf(stderr, "eval: err: cannot assign simple value to LIST variable %s\n",
			var_tok->var->var_name);
	      } else {
		if (execute) {
		  var_tok->var->var_assignments.push_back(var_value->token_value);
		  var_tok->var->var_type = Variable::VAR_SIMPLE;
		}
	      }
	    }
	  }
	}
      }
      return 0; // SET has no return value

    case TOK_BRIGHTER:
      {
	// TOK_BRIGHTER receives to numbers. If the first number is
	// numerically smaller than the second (brighter), it
	// evaluates to the integer 1, otherwise it evaluates to the
	// integer 0.
	Token *mag1 = eval_to_literal(eval(fp, execute, 1));
	Token *mag2 = eval_to_literal(eval(fp, execute, 1));

	double magnitude1, magnitude2;

	if(mag1->TokenType == TOK_DOUBLE) {
	  magnitude1 = mag1->token_value.value_double;
	} else if(mag1->TokenType == TOK_INTEGER) {
	  magnitude1 = mag1->token_value.value_int;
	} else {
	  fprintf(stderr, "<brighter>: non-numeric value?\n");
	  magnitude1 = 99.9;
	}

	if(mag2->TokenType == TOK_DOUBLE) {
	  magnitude2 = mag2->token_value.value_double;
	} else if(mag2->TokenType == TOK_INTEGER) {
	  magnitude2 = mag2->token_value.value_int;
	} else {
	  fprintf(stderr, "<brighter>: non-numeric value?\n");
	  magnitude2 = 99.9;
	}

	return_value = new Token;

	return_value->TokenType = TOK_INTEGER;
	return_value->token_value.value_type = VAL_INT;
	return_value->token_value.value_int = (magnitude1 < magnitude2);
      }
      break;
	  
    case TOK_INTEGER:
    case TOK_STRING:
    case TOK_DOUBLE:
      return_value = token;
      break;

    case TOK_MAG:
      {
	// the next token will be a string holding the starname.
	Token *star_name = eval(fp, execute, 1);
	if (star_name->TokenType != TOK_STRING) {
	  fprintf(stderr, "eval: MAG keyword must be followed by string.\n");
	} else {
	  const char *tgtstarname = star_name->token_value.value_string;

	  const char *alt_name = find_substitute(tgtstarname);
	  if (alt_name) tgtstarname = alt_name;
	  
	  return_value = new Token;
	  return_value->TokenType = TOK_DOUBLE;
	  return_value->token_value.value_type = VAL_DOUBLE;
	  return_value->token_value.value_double =
	    magnitude_from_image(image_name,
				 dark_name,
				 tgtstarname,
				 starname);
	}
      }
      break;

    }
    token = Get_Next_Token(fp);
    if(token->TokenType != TOK_CLOSE_PAREN) {
      fprintf(stderr, "Syntax error: expected close paren\n");
      return 0;
    } else {
      fprintf(stderr, "Close paren ends eval()\n");
    }
    return return_value;
  }

  if(token->TokenType == TOK_OPEN_BRACKET) {
    // OPEN BRACKET: LIST beginning
    Token *return_value = new Token;
    return_value->TokenType = TOK_LIST;
    return_value->token_value.value_type = VAL_LIST;
    return_value->var = 0;

    do {
      Token *tok = eval(fp, execute, 1);
      if (tok->TokenType == TOK_CLOSE_BRACKET ||
	  tok->TokenType == TOK_EOF) break;
      
      if (tok->TokenType == TOK_INTEGER ||
	  tok->TokenType == TOK_DOUBLE ||
	  tok->TokenType == TOK_STRING) {
	return_value->token_value.value_list.push_back(tok->token_value);
      } else {
	fprintf(stderr, "eval: err: illegal token inside LIST: %s\n",
		tok->StringType());
      }
    } while (1); // exit is via earlier "break"
    return return_value;
  } else {
    return token;
  }
  /*NOTREACHED*/
}
  
void print_variables(FILE *fp) {
  Variable *v;
  for(v=first_variable; v; v=v->next) {
    fprintf(fp, "'%-32s' = ", v->var_name);
    if(v->var_type == TOK_EOF)
      fprintf(fp, "<unset>");
    else if(v->var_type == TOK_INTEGER) {
      fprintf(fp, "%d (int)", v->var_assignments.front().value_int);
    } else if(v->var_type == TOK_DOUBLE) {
      fprintf(fp, "%lf (double)", v->var_assignments.front().value_double);
    } else if(v->var_type == TOK_STRING) {
      fprintf(fp, "'%s' (string)", v->var_assignments.front().value_string);
    } else {
      fprintf(fp, "<unrecognized type>");
    }
    fprintf(fp, "\n");
  }
}

char *dump_value_to_string(Value &v) {
  switch (v.value_type) {
  case VAL_INT:
    {
      char buffer[32];
      sprintf(buffer, "%d", v.value_int);
      return strdup(buffer);
    }

  case VAL_DOUBLE:
    {
      char buffer[32];
      sprintf(buffer, "%lf", v.value_double);
      return strdup(buffer);
    }

  case VAL_STRING:
    return strdup(v.value_string);

  case VAL_LIST:
    return 0;

  case VAL_NOVAL:
    return 0;

  default:
    fprintf(stderr, "dump_value_to_string: illegal value_type\n");
    return 0;
  }
  /*NOTREACHED*/
}

void dump_vars_to_output(Script_Output *output) {
  Variable *v;
  for(v=first_variable; v; v=v->next) {
    if(v->var_type == Variable::VAR_VOID) continue;

    Script_Entry entry;
    int entry_is_valid = 1;
    entry.var_name = strdup(v->var_name);
    char *value_string;

    switch (v->var_type) {
    case Variable::VAR_SIMPLE:
      entry.EntryType = SCRIPT_ASSIGN_SIMPLE;
      value_string = dump_value_to_string(v->var_assignments.front());
      entry.var_value = value_string;
      break;

    case Variable::VAR_VARIANT:
      entry.EntryType = SCRIPT_ASSIGN_VARIANT;
      {
	Script_Entry loc_entry;
	entry_is_valid = 0;
	for (std::list<Value>::iterator it = v->var_assignments.begin();
	     it != v->var_assignments.end(); it++) {
	  if ((*it).is_variant_value == 0) {
	    entry.num_var_values = 1;
	    entry.EntryType = SCRIPT_ASSIGN_SIMPLE;
	    entry.var_value = dump_value_to_string(*it);
	    entry_is_valid = 1;
	  } else {
	    loc_entry.EntryType = SCRIPT_ASSIGN_VARIANT;
	    loc_entry.num_var_values = 1;
	    loc_entry.var_name = strdup(v->var_name);
	    loc_entry.variant = strdup((*it).variant_name);
	    loc_entry.var_value = dump_value_to_string((*it));
	    output->Add_Entry(&loc_entry);
	  }
	}
      }
      break;

    case Variable::VAR_LIST:
      {
	entry.EntryType = SCRIPT_ASSIGN_LIST;
	// use the value from the final assignment
	Value &val = v->var_assignments.back();
	entry.num_var_values = val.value_list.size();
	entry.var_value_list = (char **) malloc (sizeof(char *) * entry.num_var_values);
	int i = 0;
	for (std::list<Value>::iterator it = val.value_list.begin();
	     it != val.value_list.end(); it++) {
	  entry.var_value_list[i++] = dump_value_to_string(*it);
	}
      }
      break;

    default:
      fprintf(stderr, "dump_vars_to_output: invalid variable type for %s\n",
	      v->var_name);
    }
    if (entry_is_valid) {
      output->Add_Entry(&entry);
    }
  }
}

const char *find_substitute(const char *entered_value) {
  std::list<Substitute *>::iterator it;

  for (it = all_substitutes.begin(); it != all_substitutes.end(); it++) {
    if (strcmp(entered_value, (*it)->typed_value) == 0) {
      // found!
      return (*it)->replacement_value;
    }
  }
  return 0;
}

void add_substitute(const char *rep_val, const char *typed_val) {
  Substitute *s = new Substitute;
  s->replacement_value = strdup(rep_val);
  s->typed_value = strdup(typed_val);
  all_substitutes.push_back(s);
}
