/*  lex.cc -- lexical analyzer of script text for script in a strategy
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
#include <string.h>
#include "lex.h"
#include <stdlib.h>		// malloc()
#include <ctype.h>		// isspace()

// compares a string from the script against a keyword; comparison is
// case-insensitive.
int keyword_check(const char *s1, const char *s2) {
  do {
    if(*s1 == 0 && *s2 == 0) return 1; // success

    if(toupper(*s1) != toupper(*s2)) return 0; // failure

    s1++;
    s2++;
  } while (1);
  /*NOTREACHED*/
}

static Token *lookahead_token = 0;
Token *Fetch_Next_Token(FILE *fp);

Token *LookAhead_Token(FILE *fp) {
  if (!lookahead_token) {
    lookahead_token = Fetch_Next_Token(fp);
  }
  return lookahead_token;
}
  
Token *Get_Next_Token(FILE *fp) {
  if (lookahead_token) {
    Token *ret_val = lookahead_token;
    lookahead_token = 0;
    return ret_val;
  }
  return Fetch_Next_Token(fp);
}

Token *Fetch_Next_Token(FILE *fp) {
  int c;
  Token *return_value = new Token;

 restart_fetch:
  // dump whitespace
  do {
    c = fgetc(fp);
    if(c == EOF) {
      return_value->TokenType = TOK_EOF;
      return return_value;
    }
  } while(isspace(c));
  if (c == '#') { // comment start
    // delete to EOL
    do {
      c = fgetc(fp);
      if (c == EOF) {
	return_value->TokenType = TOK_EOF;
	return return_value;
      }
    } while (c != '\n');
    goto restart_fetch;
  }
    
  // c now holds first non-whitespace character
  if(isdigit(c) || (c == '-') || (c == '+') || (c == '.')){
    // NUMBER
    char buffer[64];
    char *d = buffer;
    int is_real = 0;		// gets set if we see a '.'

    if(c == '-' || c == '+') {
      *d++ = c;
      c = fgetc(fp);
    }

    while(isdigit(c) || (c == '.')) {
      *d++ = c;
      if(c == '.') {
	is_real++;
      }
      c = fgetc(fp);
    }
    *d = 0;

    ungetc(c, fp);

    if(is_real) {
      sscanf(buffer, "%lf", &return_value->token_value.value_double);
      return_value->token_value.value_type = VAL_DOUBLE;
      return_value->TokenType = TOK_DOUBLE;
      return return_value;
    } else {
      sscanf(buffer, "%d", &return_value->token_value.value_int);
      return_value->token_value.value_type = VAL_INT;
      return_value->TokenType = TOK_INTEGER;
      return return_value;
    }
    
    // end of NUMBER
  } else if(isalpha(c)) {
    // STRING
    
    char buffer[64];
    char *d = buffer;

    while(isalnum(c) ||
	  (c == '+') ||
	  (c == '_') ||
	  (c == '-')) {
      *d++ = c;
      c = fgetc(fp);
    }
    *d = 0;
    ungetc(c, fp);

    // check for keywords
    //        IF
    if(keyword_check(buffer, "if")) {
      return_value->TokenType = TOK_IF;
      return return_value;
    }
    
    //        SET
    if(keyword_check(buffer, "set")) {
      return_value->TokenType = TOK_SET;
      return return_value;
    }
    
    //        DEFINE
    if(keyword_check(buffer, "define")) {
      return_value->TokenType = TOK_DEFINE;
      return return_value;
    }
    
    //        BRIGHTER
    if(keyword_check(buffer, "brighter")) {
      return_value->TokenType = TOK_BRIGHTER;
      return return_value;
    }
    
    //        MAG
    if(keyword_check(buffer, "mag")) {
      return_value->TokenType = TOK_MAG;
      return return_value;
    }
    
    //        VARIABLE or STRING
    return_value->TokenType = TOK_STRING;
    return_value->token_value.value_type = VAL_STRING;
    return_value->token_value.value_string = strdup(buffer);
    return return_value;

  } else if(c == '(') {
    return_value->TokenType = TOK_OPEN_PAREN;
    return return_value;

  } else if(c == ')') {
    return_value->TokenType = TOK_CLOSE_PAREN;
    return return_value;

  } else if (c == '[') {
    return_value->TokenType = TOK_OPEN_BRACKET;
    return return_value;
  
  } else if (c == ']') {
    return_value->TokenType = TOK_CLOSE_BRACKET;
    return return_value;

  } else {
    //        Trouble in paradise
    fprintf(stderr, "Invalid character: '%c'\n", c);
    return_value->TokenType = TOK_EOF; // TOK_ERR would be better??
    return return_value;
  }
}
  
const char *
Token::StringType(void) {
  switch(TokenType) {
  case TOK_IF:
    return "IF";
    break;

  case TOK_SET:
    return "SET";
    break;

  case TOK_BRIGHTER:
    return "BRIGHTER";
    break;

  case TOK_MAG:
    return "MAG";
    break;

  case TOK_INTEGER:
    return "INTEGER";
    break;

  case TOK_STRING:
    return "STRING";
    break;

  case TOK_DEFINE:
    return "DEFINE";
    break;

  case TOK_DOUBLE:
    return "DOUBLE";
    break;

  case TOK_EOF:
    return "EOF";
    break;

  case TOK_OPEN_PAREN:
    return "OPEN_PAREN";
    break;

  case TOK_CLOSE_PAREN:
    return "CLOSE_PAREN";
    break;

  case TOK_OPEN_BRACKET:
    return "OPEN_BRACKET";
    break;

  case TOK_CLOSE_BRACKET:
    return "CLOSE_BRACKET";
    break;

  case TOK_LIST:
    return "LIST";
    break;

  case TOK_VARIABLE:
    return "VARIABLE";
    break;

  default:
    return "<unknown>";
    break;
  }
}
