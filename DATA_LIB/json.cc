#include <list>
#include <ctype.h>
#include <stdlib.h>		// atol(), malloc()
#include <fcntl.h>
#include <stdio.h>
#include <string.h>		// strdup()
#include <signal.h>
#include <math.h>		// exp10()
#include <sys/types.h>
#include <sys/stat.h>		// stat()
#include <sys/time.h>		// gettimeofday()
#include <sys/file.h>		// flock()
#include <unistd.h>		// read()
#include "json.h"

enum TokenType {
		TOK_LEFTBRACKET,
		TOK_RIGHTBRACKET,
		TOK_LEFTSQUARE,
		TOK_RIGHTSQUARE,
		TOK_STRING,
		TOK_INT,
		TOK_BOOL,
		TOK_COMMA,
		TOK_FLOAT,
		TOK_NONE,
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

void
JSON_Expression::Kill(void) {
#if 0
  fprintf(stderr, "Killer invoked for:\n");
  this->Print(stderr, 0);
  
  if (string_val) free((void *) string_val);
  if (assignment_variable) free((void *)assignment_variable);
  if (assignment_expression) delete assignment_expression;

  for (auto x : seq_val) {
    x->Kill();
  }
#endif
  ;
}

void
JSON_Expression::SyncWithFile(const char *pathname, int mode) {
  if (file_is_active) return;
  
  if (json_fd != -1) {
    fprintf(stderr, "JSON_Expression::SyncWithFile(): ERROR: already associated with file.\n");
    kill(0, SIGABRT);
    /*NOTREACHED*/
  }

  int flags = O_RDWR | O_CREAT;
  if (mode == JSON_READONLY) flags = O_RDONLY;
  
  file_pathname = strdup(pathname);
  json_fd = open(pathname, flags, 0666);
  if (json_fd < 0) {
    char message[128];
    sprintf(message, "JSON_Expression unable to create/open file %s\n", pathname); 
    perror(message);
    if (mode == JSON_READONLY) {
      fprintf(stderr, "JSON will not create file with READONLY mode.\n");
    }
    kill(0, SIGABRT);
  }

  if (flock(json_fd, LOCK_EX)) {
    perror("JSON_Expression: unable to lock file: ");
    kill(0, SIGABRT);
  }

  struct stat statbuf;
  if (stat(pathname, &statbuf)) {
    fprintf(stderr, "Unable to stat() json file %s\n", pathname);
    kill(0, SIGABRT);
  }
  char *contents = (char *) malloc(statbuf.st_size+1);
  if (read(json_fd, contents, statbuf.st_size) != statbuf.st_size) {
    fprintf(stderr, "Error reading json file from %s\n",
	    pathname);
    kill(0, SIGABRT);
  }

  contents[statbuf.st_size] = 0;
  JSON_Expression *expr = new JSON_Expression(contents);
  expr->Validate();
  //  bool destroy = false;
  
  j_type = expr->j_type;
  switch(j_type) {
  case JSON_SEQ:
    seq_val = expr->seq_val;
    break;

  case JSON_STRING:
    string_val = expr->string_val;
    break;

  case JSON_LIST:
    seq_val = expr->seq_val;
    break;

  case JSON_FLOAT:
    float_val = expr->float_val;
    // destroy = true;
    break;
    
  case JSON_INT:
  case JSON_BOOL:
    int_val = expr->int_val;
    //destroy = true;
    break;
    
  case JSON_ASSIGNMENT:
    assignment_variable = expr->assignment_variable;
    assignment_expression = expr->assignment_expression;
    break;
    
  case JSON_EMPTY:
  case JSON_NONE:
    //destroy = true;
    break;
  }
  //if (destroy) delete expr;

  sync_flags = mode;

  Validate();
  file_is_active = true;
}

void JSON_Abort(const char *message, const JSON_Expression *expr = nullptr) {
  fprintf(stderr, "JSON_Abort: %s\n", message);
  if (expr) expr->Print(stderr);
  kill(0, SIGABRT);
  /*NOTREACHED*/
}

void
JSON_Expression::Validate(void) const {
  switch(j_type) {
  case JSON_SEQ:
    if (string_val or
	assignment_variable or
	assignment_expression) {
      JSON_Abort("Seq expression has wrong sub-content", this);
    } else {
      for (auto x : seq_val) {
	if (x->j_type != JSON_ASSIGNMENT) {
	  JSON_Abort("Seq has non-assignment child", this);
	}
	x->Validate();
      }
      return;
    }
    break;

  case JSON_NONE:
    if (assignment_variable or
	assignment_expression or
	seq_val.size()) {
      JSON_Abort("Null/None node has sub-content", this);
    } else {
      return;
    }
    break;

  case JSON_STRING:
    if (string_val == nullptr) {
      JSON_Abort("String node has <nil> string", this);
    } else if (assignment_variable or
	       assignment_expression or
	       seq_val.size()) {
      JSON_Abort("String node has sub-content", this);
    } else {
      return;
    }
    break;

  case JSON_LIST:
    // you can have a list of anything except EMPTY and ASSIGNMENTS
    if (string_val or
	assignment_variable or
	assignment_expression) {
      JSON_Abort("List expression has wrong sub-content", this);
    } else {
      for (auto x : seq_val) {
	if (x->j_type == JSON_ASSIGNMENT or
	    x->j_type == JSON_EMPTY) {
	  JSON_Abort("List has invalid child", this);
	}
	x->Validate();
      }
      return;
    }
    
    break;

  case JSON_FLOAT:
    if (string_val or
	assignment_variable or
	assignment_expression or
	seq_val.size()) {
      JSON_Abort("Float expression has sub-content", this);
    }
    break;
    
  case JSON_BOOL:
  case JSON_INT:
    if (string_val or
	assignment_variable or
	assignment_expression or
	seq_val.size()) {
      JSON_Abort("Integer expression has sub-content", this);
    }
    break;
    
  case JSON_ASSIGNMENT:
    if (assignment_variable == nullptr) {
      JSON_Abort("Assignment has no assignment variable", this);
    } else if (assignment_expression == nullptr) {
      JSON_Abort("Assignment has no value expression", this);
    } else if (assignment_expression->j_type == JSON_EMPTY or
	       assignment_expression->j_type == JSON_ASSIGNMENT) {
      JSON_Abort("Assignment has illegal value type", this);
    } else {
      return assignment_expression->Validate();
    }
    break;
    
  case JSON_EMPTY:
    if (string_val or
	assignment_variable or
	assignment_expression or
	seq_val.size()) {
      JSON_Abort("Empty expression has sub-content", this);
    }
    break;
  }
  return;
}

void Tokenize(const char *byte_string, std::list<JSON_Token *> &output) {
  const char *last_token = byte_string;
  const char *token_start = byte_string;
  do {
    last_token = token_start;
    token_start = byte_string;
    while(isspace(*byte_string)) byte_string++;
    // first non-whitespace char determines type
    JSON_Token *t = new JSON_Token;
    if (*byte_string == '{') {
      t->tok_type = TOK_LEFTBRACKET;
    } else if (*byte_string == '}') {
      t->tok_type = TOK_RIGHTBRACKET;
    } else if (*byte_string == '[') {
      t->tok_type = TOK_LEFTSQUARE;
    } else if (*byte_string == ']') {
      t->tok_type = TOK_RIGHTSQUARE;
    } else if (*byte_string == ',') {
      t->tok_type = TOK_COMMA;
    } else if (*byte_string == ':') {
      t->tok_type = TOK_COLON;
    } else if (*byte_string == '-' or
	       *byte_string == '+' or
	       isdigit(*byte_string)) {
      // is the next non-digit character a decimal point?
      //double exponent_factor = 1.0;
      const char *s = byte_string;
      s++;
      while(isdigit(*s)) s++;
      if (*s == '.' || *s == 'e') {
	// extract a float
	if (*s == '.') {
	  s++;
	  while(isdigit(*s)) {
	    s++;
	  }
	}
	if (*s == 'e') {
	  //int exponent = 0;
	  //int exp_sign = 1;
	  ++s;
	  if (*s == '+') {
	    ++s;
	  } else if (*s == '-') {
	    ++s;
	    //exp_sign = -1;
	  }
	  while(isdigit(*s)) {
	    //exponent *= 10;
	    //exponent += (*s - '0');
	    s++;
	  }
	  //exponent_factor = exp10(exp_sign*exponent);
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
      const int len = (s - byte_string)-1;
      char *d = (char *) malloc(1+len);
      strncpy(d, byte_string+1, len);
      d[len] = 0;
      t->tok_type = TOK_STRING;
      t->value.tok_string = d;
      byte_string += (1+len);
    } else if (byte_string[0] == 'f' and
	       byte_string[1] == 'a' and
	       byte_string[2] == 'l' and
	       byte_string[3] == 's' and
	       byte_string[4] == 'e') {
      t->tok_type = TOK_BOOL;
      t->value.tok_int = 0;
      byte_string += 4;
    } else if (byte_string[0] == 't' and
	       byte_string[1] == 'r' and
	       byte_string[2] == 'u' and
	       byte_string[3] == 'e') {
      t->tok_type = TOK_BOOL;
      t->value.tok_int = 1;
      byte_string += 3;
    } else if (byte_string[0] == 'n' and
	       byte_string[1] == 'u' and
	       byte_string[2] == 'l' and
	       byte_string[3] == 'l') {
      t->tok_type = TOK_NONE;
      byte_string += 3;
    } else if (*byte_string == 0) break;
    else {
      fprintf(stderr, "Parse error in Tokenize: char == '%.24s'\n", byte_string);
      fprintf(stderr, "Prior token was: '%.24s'\n", last_token);
    }
    output.push_back(t);
    byte_string++;
  } while(1); // only exit is with break;
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

    case TOK_NONE:
      p_tok_type = "None";
      p_tok_val = "";
      break;

    case TOK_LEFTBRACKET:
      p_tok_type = "{";
      p_tok_val = "";
      break;

    case TOK_RIGHTBRACKET:
      p_tok_type = "}";
      p_tok_val = "";
      break;

    case TOK_LEFTSQUARE:
      p_tok_type = "[";
      p_tok_val = "";
      break;

    case TOK_RIGHTSQUARE:
      p_tok_type = "]";
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

    case TOK_BOOL:
      p_tok_type = "BOOL";
      p_tok_val = (token->value.tok_int ? "true" : "false");
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

  InitializeFromTokens(tokens);
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
void
JSON_Expression::InitializeFromTokens(std::list<JSON_Token *> &tokens) {
  // type of first token determines type of expression

  // As a special case, if there are no tokens, just turn this into an
  // empty expression
  if (tokens.size() == 0) {
    j_type = JSON_EMPTY;
    return;
  }

  // A second special case recognizes a "naked" assignment statement
  if (tokens.front()->tok_type == TOK_STRING and
      tokens.size() > 2 and
      (*++(tokens.begin()))->tok_type == TOK_COLON) {
    // simple assignment
    this->j_type = JSON_ASSIGNMENT;
    this->assignment_variable = strdup(tokens.front()->value.tok_string);
    tokens.pop_front(); // remove string
    tokens.pop_front(); // remove colon
    assignment_expression = new JSON_Expression(tokens);
    return;
  }

  switch(tokens.front()->tok_type) {
  case TOK_LEFTBRACKET:
    // A sequence: { string : <expression> [, string : <expression>]* }
    j_type = JSON_SEQ;
    tokens.pop_front(); // remove '{'
    do {
      if (tokens.front()->tok_type == TOK_RIGHTBRACKET) break;
      
      JSON_Expression *subexpr = new JSON_Expression;
      subexpr->j_type = JSON_ASSIGNMENT;
      if(tokens.front()->tok_type != TOK_STRING) {
	fprintf(stderr, "JSON_Expression: parse error(3) looking at:\n");
	PrintTokens(tokens);
	exit(-2);
      }

      subexpr->assignment_variable = strdup(tokens.front()->value.tok_string);
      // verify colon
      tokens.pop_front(); // remove STRING
      if (tokens.front()->tok_type != TOK_COLON) {
	fprintf(stderr, "JSON_Expression: parse error(1) looking at:\n");
	PrintTokens(tokens);
	exit(-2);
      }
      tokens.pop_front(); // remove COLON
      subexpr->assignment_expression = new JSON_Expression(tokens);
      seq_val.push_back(subexpr);
      
      // now have either comma or right bracket or comma followed by right bracket
      TokenType next_token = tokens.front()->tok_type;
      // if there's a comma, get rid of it.
      if (next_token == TOK_COMMA) {
	tokens.pop_front();
	next_token = tokens.front()->tok_type;
      } else {
	// no comma, so only valid next token is right bracket
	if (next_token != TOK_RIGHTBRACKET) {
	  fprintf(stderr, "JSON_Expression: parse error(4) looking at:\n");
	  PrintTokens(tokens);
	  exit(-2);
	}
      }

      if (next_token == TOK_RIGHTBRACKET) break;
    } while (1); // only way out is break in preceeding line
    tokens.pop_front(); // pop the '}'
    break;

  case TOK_LEFTSQUARE:
    // A list: [ expr, expr, expr ]
    j_type = JSON_LIST;
    tokens.pop_front(); // remove '['
    while (tokens.front()->tok_type != TOK_RIGHTSQUARE) {
      JSON_Expression *listexpr = new JSON_Expression(tokens);
      seq_val.push_back(listexpr);
      if (tokens.front()->tok_type == TOK_COMMA) tokens.pop_front();
    }
    tokens.pop_front(); // remove ']'
    break;

  case TOK_STRING:
    // easy, just a string expression
    j_type = JSON_STRING;
    string_val = strdup(tokens.front()->value.tok_string);
    tokens.pop_front();
    break;

  case TOK_BOOL:
    // easy, just a boolean expression
    j_type = JSON_BOOL;
    int_val = tokens.front()->value.tok_int;
    tokens.pop_front();
    break;

  case TOK_NONE:
    // A Python "None" value
    j_type = JSON_NONE;
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
    exit(-2);
  }
}

JSON_Expression *JSON_Expression::GetValue(const char *dot_string) const {

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
    //fprintf(stderr, "JSON: GetValue: couldn't find %s\n", name);
  }
  return nullptr;
}

JSON_Expression *
JSON_Expression::Value(const char *keyword) const {
  // We are pointing to an expression sequence. Find this keyword in the sequence
  if (not IsSeq()) {
    fprintf(stderr, "JSON::Value() type mismatch\n");
    Print(stderr);
    return nullptr;
  }

  for (auto x : seq_val) {
    if (strcmp(x->assignment_variable, keyword) == 0) {
      // match found
      return x->assignment_expression;
    }
  }
  return nullptr;
}

std::list<JSON_Expression *> &
JSON_Expression::Value_list(void) {
  if (not IsList()) {
    Print(stderr);
    JSON_Abort("JSON::Value_list() type mismatch");
    /*NOTREACHED*/
  }

  return seq_val;
}

const char *
JSON_Expression::Assignment_variable(void) const {
  if (not IsAssignment()) {
    Print(stderr);
    JSON_Abort("JSON::Assignment_variable() type mismatch");
    /*NOTREACHED*/
  }

  return assignment_variable;
}

JSON_Expression &
JSON_Expression::GetAssignment(void) const {
  if (not IsAssignment()) {
    Print(stderr);
    JSON_Abort("JSON::GetAssignment() type mismatch");
    /*NOTREACHED*/
  }

  return *assignment_expression;
}

std::list<JSON_Expression *> &
JSON_Expression::Value_seq(void) {
  if (not IsSeq()) {
    Print(stderr);
    JSON_Abort("JSON::Value_seq() type mismatch");
    /*NOTREACHED*/
  }

  return seq_val;
}

const char *
JSON_Expression::Value_char(void) const {
  if (j_type == JSON_SEQ and seq_val.size() == 1) return seq_val.front()->Value_char();

  if (j_type != JSON_STRING) {
    fprintf(stderr, "JSON::Value_string() type mismatch\n");
    Print(stderr);
    return nullptr;
  }
  return string_val;
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

bool
JSON_Expression::Value_bool(void) const {
  if (j_type == JSON_SEQ and seq_val.size() == 1) return seq_val.front()->Value_bool();
  
  if (j_type != JSON_BOOL) {
    fprintf(stderr, "JSON::Value_bool() type mismatch\n");
    Print(stderr);
    return 0;
  } else {
    return int_val;
  }
}

std::string
JSON_Expression::Value_string(void) const {
  if (j_type == JSON_SEQ and seq_val.size() == 1) return seq_val.front()->Value_string();
  
  if (j_type != JSON_STRING) {
    fprintf(stderr, "JSON::Value_string() type mismatch\n");
    Print(stderr);
    return std::string("");
  } else {
    return std::string(string_val);
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

  case JSON_BOOL:
    type = "BOOL";
    val_ptr = value;
    sprintf(value, "%s", (int_val ? "true" : "false"));
    break;

  case JSON_ASSIGNMENT:
    type = "ASSIGN";
    sprintf(message, "%s =", assignment_variable);
    val_ptr = message;
    break;

  case JSON_SEQ:
    type = "SEQ";
    sprintf(message, "%ld entries", seq_val.size());
    val_ptr = message;
    break;

  case JSON_LIST:
    type = "LIST";
    sprintf(message, "%ld entries", seq_val.size());
    val_ptr = message;
    break;

  case JSON_NONE:
    type = "NONE";
    val_ptr = "<None>";
    break;
    
  case JSON_EMPTY:
    type = "EMPTY";
    val_ptr = "<nil>";
    break;
  }

  for(int i=0; i<indent; i++) fprintf(fp, " ");
  fprintf(fp, "%s --> %s\n", type, val_ptr);

  if (j_type == JSON_ASSIGNMENT) {
    assignment_expression->Print(fp, indent+5);
  }
  if (j_type == JSON_SEQ or j_type == JSON_LIST) {
    for (auto s : seq_val) {
      s->Print(fp, indent+5);
    }
  }
}

static void safe_write(int fd, const char *buffer, int count) {
  int written = 0;

  do {
    int res = write(fd, buffer, count);
    if (res >= 0) {
      written += res;
      count -= res;
      if (count == 0) return;
    } else {
      if (errno == EINTR) {
	; // do nothing and retry
      } else {
	// deep error
	perror("attempting to write to JSON file");
	return;
      }
    }
  } while(1); // only exit is via "return"
  /*NOTREACHED*/
}

void JSON_Expression::WriteJSON(int file_fd) const {
  switch(j_type) {
  case JSON_STRING:
    safe_write(file_fd, "\"", 1);
    safe_write(file_fd, string_val, strlen(string_val));
    safe_write(file_fd, "\"", 1);
    break;

  case JSON_SEQ:
    {
      safe_write(file_fd, "{\n", 2);
      bool first = true;
      for (auto x : seq_val) {
	if (not first) {
	  safe_write(file_fd, ",\n", 2);
	} else {
	  first = false;
	}
	x->WriteJSON(file_fd);
      }
      safe_write(file_fd, "\n}\n", 3);
    }
    break;

  case JSON_LIST:
    {
      safe_write(file_fd, "[\n", 2);
      bool first = true;
      for (auto x : seq_val) {
	if (not first) {
	  safe_write(file_fd, ",\n", 2);
	} else {
	  first = false;
	}
	x->WriteJSON(file_fd);
      }
      safe_write(file_fd, "]\n", 2);
    }
    break;

  case JSON_FLOAT:
    {
      char buffer[32];
      sprintf(buffer, "%lf", float_val);
      safe_write(file_fd, buffer, strlen(buffer));
    }
    break;

  case JSON_BOOL:
    {
      char buffer[32];
      sprintf(buffer, "%s", (int_val ? "true" : "false"));
      safe_write(file_fd, buffer, strlen(buffer));
    }
    break;

  case JSON_NONE:
    safe_write(file_fd, "null", 4);
    break;

  case JSON_INT:
    {
      char buffer[32];
      sprintf(buffer, "%ld", int_val);
      safe_write(file_fd, buffer, strlen(buffer));
    }
    break;

  case JSON_ASSIGNMENT:
    safe_write(file_fd, "\"", 1);
    safe_write(file_fd, assignment_variable, strlen(assignment_variable));
    safe_write(file_fd, "\" : ", 4);
    assignment_expression->WriteJSON(file_fd);
    break;

  case JSON_EMPTY:
    safe_write(file_fd, "\n", 1);
    break;
  }
}
  

void
JSON_Expression::WriteAndReleaseFileSync(void) {
  Validate();
  
  if (json_fd < 0 or
      sync_flags & JSON_READONLY) {
    JSON_Abort("WriteAndRelease: cannot write to JSON file", nullptr);
  }

  // I didn't know this before... ftruncate() doesn't change the
  // current location pointer associated with the file descriptor
  if (ftruncate(json_fd, 0)) {
    JSON_Abort("WriteAndRelease: ftruncate() failed", nullptr);
  }
  if (lseek(json_fd, 0, SEEK_SET)) {
    JSON_Abort("WriteAndRelease: lseek() failed", nullptr);
  }

  this->WriteJSON(json_fd);

  // remember when; later, we can check file modify time to see if any
  // modifications happened after we released it
  if(gettimeofday(&this->time_of_release, nullptr)) {
    perror("gettimeofday() failed in json.cc");
    JSON_Abort("gettimeofday() failed");
    /*NOTREACHED*/
  }

  // This releases any lock we held
  if(close(json_fd)) {
    fprintf(stderr, "Unable to close JSON file after writing\n");
  }
  is_dirty = false;
  json_fd = -1;
  file_is_active = false;
}

void
JSON_Expression::ReSyncWithFile(int mode, bool *anything_changed) {
  // check initial conditions
  if (file_is_active) {
    if(anything_changed) *anything_changed = false;
    return;
  }

  if (json_fd != -1 or
      file_pathname == nullptr) {
    JSON_Abort("JSON_Expression:ReSyncWithFile(): invalid fd/path");
    /*NOTREACHED*/
  }

  struct stat statbuf;
  if (stat(file_pathname, &statbuf)) {
    fprintf(stderr, "Unable to stat() prior json file %s\n", file_pathname);
    kill(0, SIGABRT);
  }

  struct timeval last_mod;
  TIMESPEC_TO_TIMEVAL(&last_mod, &statbuf.st_mtim);
  if (timercmp(&last_mod, &this->time_of_release, >)) {
    // Old data is invalid
    this->Kill();
    fprintf(stderr, "JSON_Expression::ReSyncWithFile() is reloading file.\n");
    if(anything_changed) *anything_changed = true;
    SyncWithFile(file_pathname, sync_flags);
  } else {
    if(anything_changed) *anything_changed = false;
    int flags = O_RDWR;
    if (sync_flags == JSON_READONLY) flags = O_RDONLY;
  
    json_fd = open(file_pathname, flags, 0666);
    if (json_fd < 0) {
      char message[128];
      sprintf(message, "ReSyncWithFile() unable to open file %s\n", file_pathname); 
      perror(message);
      if (sync_flags == JSON_READONLY) {
	fprintf(stderr, "JSON will not create file with READONLY mode.\n");
      }
      kill(0, SIGABRT);
    }

    if (flock(json_fd, LOCK_EX)) {
      perror("JSON_Expression: unable to lock file: ");
      kill(0, SIGABRT);
    }
  }
  file_is_active = true;
}
    
JSON_Expression *
JSON_Expression::CreateBlankTopLevelSeq(void) {
  // only valid to invoke this if "this" is a single, empty node
  if (not this->IsEmpty()) {
    JSON_Abort("CreateBlankTopLevelSeq(): initial exp is not EMPTY.", this);
  }

  j_type = JSON_SEQ;

  this->Validate();
  return this;
}

void
JSON_Expression::InsertAssignmentIntoSeq(JSON_Expression *assignment) {
  this->Validate();
  assignment->Validate();
  this->seq_val.push_back(assignment);
  this->Validate();
}

void
JSON_Expression::AddToArrayEnd(JSON_Expression *to_add) { // "this" is
							  // the list
  this->Validate();
  to_add->Validate();
  if (not this->IsList()) {
    JSON_Abort("AddToArrayEnd(): 'this' isn't array.");
  }
  this->seq_val.push_back(to_add);
}

void
JSON_Expression::DeleteFromArray(JSON_Expression *item_to_delete) {
  this->Validate();
  if (not(this->IsList() or this->IsSeq())) {
    JSON_Abort("DeleteFromArray(): initial exp not list or seq", this);
  }

  this->seq_val.remove(item_to_delete);
}

void
JSON_Expression::ReplaceAssignment(JSON_Expression *new_value) {
  if (not(this->IsAssignment())) {
    JSON_Abort("ReplaceAssignment(value): initial exp not assignment", this);
  } else {
    //delete assignment_expression;
    assignment_expression = new_value;
  }
}
void
JSON_Expression::ReplaceAssignment(const char *key, JSON_Expression *new_value) {
  this->Validate();
  if (not(this->IsSeq())) {
    JSON_Abort("ReplaceAssignment(key): initial exp not seq", this);
  } else {
    for (auto k : this->seq_val) {
      if (strcmp(k->assignment_variable, key) == 0) {
	if (new_value == nullptr) {
	  // delete the whole assignment
	  this->seq_val.remove(k);
	  return;
	} else {
	  return k->ReplaceAssignment(new_value);
	}
      }
    }
    JSON_Abort("ReplaceAssignment(key): assignment key not found", this);
  }
}

JSON_Expression::JSON_Expression(JSON_TYPE type) {
  j_type = type;
  if (j_type == JSON_EMPTY or
      j_type == JSON_SEQ or
      j_type == JSON_LIST) {
    ; // good situation; nothing further to do
  } else {
    JSON_Abort("JSON_Expression::JSON_Expression: invalid type");
  }
}
    
JSON_Expression::JSON_Expression(JSON_TYPE type, long value) { // int
  if (type != JSON_INT and type != JSON_BOOL) {
    JSON_Abort("JSON_Expression::JSON_Expression() type must be JSON_INT or JSON_BOOL");
  }
  j_type = type;
  int_val = value;
}

JSON_Expression::JSON_Expression(JSON_TYPE type, double value) { // float
  if (type != JSON_FLOAT) {
    JSON_Abort("JSON_Expression::JSON_Expression() type must be JSON_FLOAT");
  }
  j_type = JSON_FLOAT;
  float_val = value;
}

JSON_Expression::JSON_Expression(JSON_TYPE type, const char *s, JSON_Expression *value) { // assignment
  if (type != JSON_ASSIGNMENT) {
    JSON_Abort("JSON_Expression::JSON_Expression() type must be JSON_ASSIGNMENT");
  }
  j_type = JSON_ASSIGNMENT;
  assignment_variable = s;
  assignment_expression = value;
}

JSON_Expression::JSON_Expression(JSON_TYPE type, const char *s, const char *value) { // assignment
  if (type != JSON_ASSIGNMENT) {
    JSON_Abort("JSON_Expression::JSON_Expression() type must be JSON_ASSIGNMENT");
  }
  j_type = JSON_ASSIGNMENT;
  assignment_variable = s;
  assignment_expression = new JSON_Expression(JSON_STRING, value);
}

JSON_Expression::JSON_Expression(JSON_TYPE type, const char *s, long value) { // assignment
  if (type != JSON_ASSIGNMENT) {
    JSON_Abort("JSON_Expression::JSON_Expression() type must be JSON_ASSIGNMENT");
  }
  j_type = JSON_ASSIGNMENT;
  assignment_variable = s;
  assignment_expression = new JSON_Expression(JSON_INT, value);
}
  
JSON_Expression::JSON_Expression(JSON_TYPE type, const char *s, double value) { // assignment
  if (type != JSON_ASSIGNMENT) {
    JSON_Abort("JSON_Expression::JSON_Expression() type must be JSON_ASSIGNMENT");
  }
  j_type = JSON_ASSIGNMENT;
  assignment_variable = s;
  assignment_expression = new JSON_Expression(JSON_FLOAT, value);
}

JSON_Expression::JSON_Expression(JSON_TYPE type, const char *value) { // string
  if (type != JSON_STRING) {
    JSON_Abort("JSON_Expression::JSON_Expression() type must be JSON_STRING");
  }
  j_type = JSON_STRING;
  string_val = value;
}

void
JSON_Expression::InsertUpdateTSTAMPInSeq(void) {
  if (j_type != JSON_SEQ) {
    JSON_Abort("JSON::InsertTSTAMP() type must be JSON_SEQ");
  }
  time_t now = time(0);
  JSON_Expression *assignment = FindAssignment("tstamp");
  if (assignment) {
    assignment->ReplaceAssignment(new JSON_Expression(JSON_INT, (long) now));
  } else {
    // no existing tstamp assignment
    InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT, "tstamp", (long) now));
  }
}

JSON_Expression::JSON_Expression(JSON_TYPE type, std::list<long> &input) {
  if (type != JSON_LIST) {
    JSON_Abort("JSON::Expression(juid_list): must be built upon JSON_LIST.");
  }
  j_type = JSON_LIST;
  for (long i : input) {
    JSON_Expression *exp = new JSON_Expression(JSON_INT, i);
    this->AddToArrayEnd(exp);
  }
}

JSON_Expression::JSON_Expression(JSON_TYPE type, const char *s, std::list<std::string> &input) {
  if (type != JSON_ASSIGNMENT) {
    JSON_Abort("JSON::Expression(std::string_list): must be built upon JSON_ASSIGNMENT.");
  }
  j_type = JSON_ASSIGNMENT;
  assignment_variable = s;
  assignment_expression = new JSON_Expression(JSON_LIST);
  for (std::string &s : input) {
    JSON_Expression *exp = new JSON_Expression(JSON_STRING, strdup(s.c_str()));
    assignment_expression->AddToArrayEnd(exp);
  }
}


JSON_Expression *
JSON_Expression::FindAssignment(const char *key) {
  if (j_type != JSON_SEQ) {
    JSON_Abort("JSON::FindAssignment() type must be JSON_SEQ");
  } else {
    for (auto assignment : this->seq_val) {
      if (strcmp(assignment->assignment_variable, key) == 0) {
	return assignment;
      }
    }
  }
  return nullptr;
}
