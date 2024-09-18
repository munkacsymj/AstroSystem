/*  execute_script.cc -- Program that executes a script from a
 *  strategy and writes the resulting output 
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
#include <unistd.h>		// getopt()
#include <string.h>		// strdup()
#include <ctype.h>		// toupper(), isspace()
#include <stdlib.h>		// system()
#include <sys/types.h>
#include "execute_script.h"

void usage(void) {
  fprintf(stderr, "usage: execute_script -n star -i image.fits -d dark.fits -e script -o out.txt\n");
  exit(-2);
}

char *image_name = 0;
char *dark_name = 0;
char *starname = 0;

int main(int argc, char **argv) {
  int option_char;
  char *output_name = 0;
  char *script_name = 0;

  while((option_char = getopt(argc, argv, "n:i:d:e:o:")) > 0) {
    switch (option_char) {
    case 'i':
      image_name = optarg;
      break;

    case 'n':
      starname = optarg;
      break;

    case 'o':
      output_name = optarg;
      break;

    case 'd':
      dark_name = optarg;
      break;

    case 'e':
      script_name = optarg;
      break;
      
    case '?':			// invalid argument
    default:
      fprintf(stderr, "Invalid argument '%c'.\n", option_char);
      exit(2);
    }
  }

  if(image_name  == 0 ||
     dark_name   == 0 ||
     output_name == 0 ||
     starname    == 0) usage();

  FILE *fp=fopen(script_name, "r");

  if(!fp) {
    fprintf(stderr, "execute_script: cannot open script file %s.\n",
	    script_name);
    return 0;
  }

  // read expressions until an EOF token is returned
  Token *t;
  t = eval_exp_seq(fp, 1, 0);
  if (t->TokenType != TOK_EOF) {
    fprintf(stderr, "Syntax error at top level.\n");
  }

  {
    Script_Output output(output_name, 1);
    dump_vars_to_output(&output);
  }
  return 0;
}
    
