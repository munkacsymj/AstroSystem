/*  move.cc -- Main program to move the mount a few arcmins
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
#include <unistd.h>		// pick up sleep()
#include <stdlib.h>		// pick up atof()
#include <stdio.h>
#include "scope_api.h"

void scope_error(char *response, ScopeResponseStatus Status) {
  const char *type;

  if(Status == Okay) type = "Okay";
  if(Status == TimeOut) type = "TimeOut";
  if(Status == Aborted) type = "Aborted";

  fprintf(stderr, "ERROR: %s, string = '%s'\n", type, response);
}

int main(int argc, char **argv) {
  double north_delta = 0.0;
  double east_delta  = 0.0;
  int perform_goto = 0;		// set with -g option

  if(argc == 1) exit(0);	// nothing specified.
  if(argc >= 5) {
    fprintf(stderr, "usage: move [-g] xxx.xN xxx.xE\n");
    exit(2);
  }

  while(--argc) {
    if(strcmp(argv[argc], "-g") == 0) {
      perform_goto = 1;
    } else {
      char last_letter;
      const int len = strlen(argv[argc]);
      char *last_letter_ptr = argv[argc] + (len-1);

      last_letter = *last_letter_ptr;
      *last_letter_ptr = 0;

      double converted_value = atof(argv[argc]);

      switch(last_letter) {
      case 'n':
      case 'N':
	north_delta = converted_value;
	break;

      case 's':
      case 'S':
	north_delta = -converted_value;
	break;

      case 'e':
      case 'E':
	east_delta = converted_value;
	break;

      case 'w':
      case 'W':
	east_delta = -converted_value;
	break;

      default:
	fprintf(stderr, "Motion must end with one of N, S, E, or W\n");
	exit(2);
      }
    }
  }

  connect_to_scope();

  DEC_RA initial_pos = ScopePointsAt();

  printf("Initial scope position:\nRA= %s\nDEC= %s (J2000)\n",
	 initial_pos.string_ra_of(),
	 initial_pos.string_dec_of());

  if (perform_goto) {
    north_delta *= ((2.0 * M_PI)/360.0)/60.0;
    east_delta  *= ((2.0 * M_PI)/360.0)/60.0;
    initial_pos.increment(north_delta, east_delta);
    MoveTo(&initial_pos);
  } else {
    int result = SmallMove(east_delta/cos(initial_pos.dec()), north_delta);
    if (result) {
      fprintf(stderr, "Move: SmallMove() returned error code %d\n", result);
    }
  }
      
  WaitForGoToDone();

  DEC_RA final_pos = ScopePointsAt();
  printf("Final scope position:\nRA= %s\nDEC= %s\n",
	 final_pos.string_ra_of(),
	 final_pos.string_dec_of());
  return 0;
}
  
