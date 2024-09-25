#include <list>
#include <ctype.h>
#include <stdlib.h>		// atol(), malloc()
#include <stdio.h>
#include <string.h>		// strdup()
#include "json.h"

enum TokenType {
		TOK_LEFTBRACKET,
		TOK_RIGHTBRACKET,
		TOK_STRING,
		TOK_INT,
		TOK_COMMA,
		TOK_FLOAT,
		TOK_COLON,
};

struct JSON_Token {
  JSON_Token(void) {;}
  ~JSON_Token(void);

  TokenType tok_type;
  union {
    const char *tok_string;
    long tok_int;
    double tok_double;
  } value;
};

JSON_Token::~JSON_Token(void) {
  if (tok_type == TOK_STRING) free((void *)value.tok_string);
}

JSON_Expression::~JSON_Expression(void) {
  if (string_val) free((void *) string_val);
  if (assignment_variable) free((void *)assignment_variable);
  if (assignment_expression) delete assignment_expression;

  for (auto x : seq_val) {
    delete x;
  }
}

void Tokenize(const char *byte_string, std::list<JSON_Token *> &output) {
  while(*byte_string) {
    while(isspace(*byte_string)) byte_string++;
    // first non-whitespace char determines type
    JSON_Token *t = new JSON_Token;
    if (*byte_string == '{') {
      t->tok_type = TOK_LEFTBRACKET;
    } else if (*byte_string == '}') {
      t->tok_type = TOK_RIGHTBRACKET;
    } else if (*byte_string == ',') {
      t->tok_type = TOK_COMMA;
    } else if (*byte_string == ':') {
      t->tok_type = TOK_COLON;
    } else if (*byte_string == '-' or
	       *byte_string == '+' or
	       isdigit(*byte_string)) {
      // is the next non-digit character a decimal point?
      const char *s = byte_string;
      s++;
      while(isdigit(*s)) s++;
      if (*s == '.') {
	// extract a float
	s++;
	while(isdigit(*s)) {
	  s++;
	}
	t->tok_type = TOK_FLOAT;
	t->value.tok_double = atof(byte_string);
	byte_string = (s-1);
      } else {
	t->tok_type = TOK_INT;
	t->value.tok_int = atol(byte_string);
	// extract an int
	byte_string = (s-1);
      }
    } else if (*byte_string == '"') {
      // extract a string
      const char *s = byte_string+1;
      while(*s and *s != '"') s++;
      const char len = (s - byte_string)-1;
      char *d = (char *) malloc(1+len);
      strncpy(d, byte_string+1, len);
      d[len] = 0;
      t->tok_type = TOK_STRING;
      t->value.tok_string = d;
      byte_string += (1+len);
    } else if (*byte_string == 0) break;
    else {
      fprintf(stderr, "Parse error in Tokenize: char == '%c'\n", *byte_string);
    }
    output.push_back(t);
    byte_string++;
  }
}

void PrintTokens(std::list<JSON_Token *> &input_list) {
  const char *p_tok_type = "";
  const char *p_tok_val = "";
  char val_buffer[80];
  
  for (auto token : input_list) {
    switch(token->tok_type) {
    case TOK_STRING:
      p_tok_type = "STRING";
      p_tok_val = token->value.tok_string;
      break;

    case TOK_LEFTBRACKET:
      p_tok_type = "{";
      p_tok_val = "";
      break;

    case TOK_RIGHTBRACKET:
      p_tok_type = "}";
      p_tok_val = "";
      break;

    case TOK_COMMA:
      p_tok_type = "COMMA";
      p_tok_val = "";
      break;

    case TOK_COLON:
      p_tok_type = "COLON";
      p_tok_val = ":";
      break;

    case TOK_INT:
      p_tok_type = "INT";
      sprintf(val_buffer, "%ld", token->value.tok_int);
      p_tok_val = val_buffer;
      break;

    case TOK_FLOAT:
      p_tok_type = "FLOAT";
      sprintf(val_buffer, "%lf", token->value.tok_double);
      p_tok_val = val_buffer;
      break;

    default:
      p_tok_type = "ERROR";
      p_tok_val = "";
      break;
    }

    if (*p_tok_val) {
      fprintf(stderr, " %s (%s)\n",
	      p_tok_type, p_tok_val);
    } else {
      fprintf(stderr, " %s \n",
	      p_tok_type);
    }
  }
}
  
JSON_Expression::JSON_Expression(const char *byte_string) {
  std::list<JSON_Token *> tokens;
  Tokenize(byte_string, tokens);
  //PrintTokens(tokens);

  // This is ugly. I know. Sorry.
  JSON_Expression expr(tokens);
  j_type = expr.j_type; // remember: might be JSON_EMPTY
  float_val = expr.float_val;
  string_val = expr.string_val;
  int_val = expr.int_val;
  assignment_variable = expr.assignment_variable;
  assignment_expression = expr.assignment_expression;
  seq_val = expr.seq_val;
  expr.assignment_expression = nullptr;
  expr.assignment_variable = nullptr;
  expr.string_val = nullptr;
  expr.seq_val.clear();

  for (auto t : tokens) {
    delete t;
  }
}

JSON_Expression::JSON_Expression(void) {
  j_type = JSON_EMPTY;
  float_val = 0.0;
  int_val = 0;
  string_val = nullptr;
  assignment_variable = nullptr;
  assignment_expression = nullptr;
}

// Remove tokens from "tokens" as they are needed. Leave trailing
// tokens untouched. 
JSON_Expression::JSON_Expression(std::list<JSON_Token *> &tokens) {
  // type of first token determines type of expression

  if (tokens.size() == 0) {
    j_type = JSON_EMPTY;
    return;
  }

  switch(tokens.front()->tok_type) {
  case TOK_LEFTBRACKET:
    // A sequence: { string : <expression> [, string : <expression>]* }
    j_type = JSON_SEQ;
    tokens.pop_front(); // remove '{'
    do {
      JSON_Expression *subexpr = new JSON_Expression;
      subexpr->j_type = JSON_ASSIGNMENT;
      if(tokens.front()->tok_type != TOK_STRING) {
	fprintf(stderr, "JSON_Expression: parse error(3) looking at:\n");
	PrintTokens(tokens);
	return;
      }

      subexpr->assignment_variable = strdup(tokens.front()->value.tok_string);
      // verify colon
      tokens.pop_front(); // remove STRING
      if (tokens.front()->tok_type != TOK_COLON) {
	fprintf(stderr, "JSON_Expression: parse error(1) looking at:\n");
	PrintTokens(tokens);
	return;
      }
      tokens.pop_front(); // remove COLON
      subexpr->assignment_expression = new JSON_Expression(tokens);
      seq_val.push_back(subexpr);
      
      // now have either comma or right bracket
      TokenType next_token = tokens.front()->tok_type;
      if (next_token != TOK_COMMA and next_token != TOK_RIGHTBRACKET) {
	fprintf(stderr, "JSON_Expression: parse error(4) looking at:\n");
	PrintTokens(tokens);
	return;
      }
      tokens.pop_front(); // remove '}' or COMMA
      if (next_token == TOK_RIGHTBRACKET) break;
    } while (1); // only way out is break in preceeding line
    break;  

  case TOK_STRING:
    // easy, just a string expression
    j_type = JSON_STRING;
    string_val = strdup(tokens.front()->value.tok_string);
    tokens.pop_front();
    break;

  case TOK_INT:
    // easy, just an integer expression
    j_type = JSON_INT;
    int_val = tokens.front()->value.tok_int;
    tokens.pop_front();
    break;

  case TOK_FLOAT:
    // easy, just a float (double) expression
    j_type = JSON_FLOAT;
    float_val = tokens.front()->value.tok_double;
    tokens.pop_front();
    break;
    
  default:
    fprintf(stderr, "JSON_Expression: parse error looking at:\n");
    PrintTokens(tokens);
  }
}

const JSON_Expression *JSON_Expression::GetValue(const char *dot_string) const {

  const char *dot_location = nullptr;

  const char *s = dot_string;
  while(*s and *s != '.') s++;
  int len = s-dot_string;
  char name[1+len];
  strncpy(name, dot_string, len);
  name[len] = 0;

  if (*s == '.') {
    dot_location = s;
  }

  //fprintf(stderr, "GetValue: working with %s\n", name);

  // does an assignment in this expression match name?
  if (j_type == JSON_ASSIGNMENT) {
    if (strcmp(assignment_variable, name) != 0) {
      fprintf(stderr, "JSON: GetValue: name failure: %s vs %s\n",
	      assignment_variable, name);
    } else {
      fprintf(stderr, "GetValue: returning from JSON_ASSIGNMENT.\n");
      if (dot_location) {
	return assignment_expression->GetValue(dot_location+1);
      } else {
	return assignment_expression;
      }
    }
  } else if (j_type == JSON_SEQ) {
    for (auto expr : seq_val) {
      if (strcmp(expr->assignment_variable, name) == 0) {
	if (dot_location) {
	  return expr->assignment_expression->GetValue(dot_location+1);
	} else {
	  return expr->assignment_expression;
	}
      }
    }
    fprintf(stderr, "JSON: GetValue: couldn't find %s\n", name);
  }
  return nullptr;
}

  
long
JSON_Expression::Value_int(void) const {
  if (j_type == JSON_SEQ and seq_val.size() == 1) return seq_val.front()->Value_int();
  
  if (j_type != JSON_INT) {
    fprintf(stderr, "JSON::Value_int() type mismatch\n");
    Print(stderr);
    return 0;
  } else {
    return int_val;
  }
}

double
JSON_Expression::Value_double(void) const {
  if (j_type == JSON_SEQ and seq_val.size() == 1) return seq_val.front()->Value_double();
  
  if (j_type != JSON_FLOAT) {
    fprintf(stderr, "JSON::Value_double() type mismatch\n");
    Print(stderr);
    return 0.0;
  } else {
    return float_val;
  }
}

const char *
JSON_Expression::Value_char(void) const {
  if (j_type == JSON_SEQ and seq_val.size() == 1) return seq_val.front()->Value_char();
  
  if (j_type != JSON_STRING) {
    fprintf(stderr, "JSON::Value_char() type mismatch\n");
    Print(stderr);
    return nullptr;
  } else {
    return string_val;
  }
}

void
JSON_Expression::Print(FILE *fp, int indent) const {
  const char *type = "invalid";
  char value[80];
  const char *val_ptr = "";
  char message[256] {0};
  
  switch(j_type) {
  case JSON_STRING:
    type = "STRING";
    val_ptr = string_val;
    break;

  case JSON_FLOAT:
    type = "FLOAT";
    val_ptr = value;
    sprintf(value, "%lf", float_val);
    break;

  case JSON_INT:
    type = "INT";
    val_ptr = value;
    sprintf(value, "%ld", int_val);
    break;

  case JSON_ASSIGNMENT:
    type = "ASSIGN";
    sprintf(message, "%s = <expr>\n", assignment_variable);
    val_ptr = message;
    break;

  case JSON_SEQ:
    type = "SEQ";
    sprintf(message, "%lu entries\n", seq_val.size());
    val_ptr = message;
    break;

  }

  for(int i=0; i<indent; i++) fprintf(fp, " ");
  fprintf(fp, "%s --> %s\n", type, val_ptr);

  if (j_type == JSON_ASSIGNMENT) {
    assignment_expression->Print(fp, indent+5);
  }
  if (j_type == JSON_SEQ) {
    for (auto s : seq_val) {
      s->Print(fp, indent+5);
    }
  }
}
