/*  gm2000_sync.cc -- Program to synchronize and add alignment stars
 *
 *  Copyright (C) 2017 Mark J. Munkacsy
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
#include <unistd.h>		// pick up sleep(), getopt()
#include <stdlib.h>		// pick up atof()
#include <stdio.h>
#include <string.h>
#include "scope_api.h"
#include <list>

void usage(void) {
  fprintf(stderr, "usage: gm2000_sync -f align_points.txt\n");
  exit(2);
}

int main(int argc, char **argv) {
  int option_char;
  const char *input_filename = 0;

  while((option_char = getopt(argc, argv, "f:")) > 0) {
    switch (option_char) {
    case 'f':
      input_filename = optarg;
      break;
      
    case '?':
    default:
      fprintf(stderr, "Invalid argument.\n");
      usage();
    }
  }

  if (input_filename == 0) usage();

  connect_to_scope();

  FILE *fp = fopen(input_filename, "r");
  if (!fp) {
    fprintf(stderr, "Cannot read alignment points from %s.\n",
	    input_filename);
  } else {
    char buffer[132];
    SyncPointList p;
    while(fgets(buffer, sizeof(buffer), fp)) {
      for (char *s = buffer; *s; s++) {
	if (*s == '\n') {
	  *s = 0;
	  break;
	}
      }
      if (buffer[0]) { // ignore blank lines
	p.push_back(strdup(buffer));
      }
    }
    fprintf(stderr, "Found %ld sync points.\n",
	    p.size());
    LoadSyncPoints(&p);
    fclose(fp);
  }
}
