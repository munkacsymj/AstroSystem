/*  set_focus.cc -- Main program to manually move the focus motor
 *
 *  Copyright (C) 2015, 2017 Mark J. Munkacsy
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
#include <string.h>
#include <stdio.h>
#include "scope_api.h"
#include <system_config.h>

SystemConfig system_config;

void scope_error(char *response, ScopeResponseStatus Status) {
  const char *type = "";

  if(Status == Okay) type = "Okay";
  if(Status == TimeOut) type = "TimeOut";
  if(Status == Aborted) type = "Aborted";

  fprintf(stderr, "ERROR: %s, string = '%s'\n", type, response);
}

void usage(void) {
  fprintf (stderr, "usage: set_focus [-F C|F] [-h | -t [+-]nnn] | -a [+=]nnn\n");
  fprintf (stderr, "     (nnn in msec)\n");
  disconnect_focuser();
  exit(-2);
}

int main(int argc, char **argv) {
  int option_char;
  long running_time = 0;
  bool go_halfway = false;
  const char *focuser_name = nullptr;
  FocuserName selected_focuser = FOCUSER_FINE;
  bool move_absolute = false;

  connect_to_focuser();

  while((option_char = getopt(argc, argv, "a:hF:t:")) > 0) {
    switch (option_char) {
    case 'h':
      go_halfway = true;
      break;

    case 'F':
      focuser_name = optarg;
      break;

    case 'a':
      running_time = atol(optarg);
      move_absolute = true;
      break;

    case 't':			// running time in msec
      running_time = atol(optarg);
      move_absolute = false;
      break;

    case '?':			// invalid argument
    default:
      fprintf(stderr, "Invalid argument.\n");
      usage();
      exit(2);
    }
  }

  if (focuser_name) {
    if (strcmp(focuser_name, "C") == 0 or strcmp(focuser_name, "c") == 0) {
      selected_focuser = FOCUSER_COARSE;
    } else if (strcmp(focuser_name, "F") == 0 or strcmp(focuser_name, "f") == 0) {
      selected_focuser = FOCUSER_FINE;
    } else {
      fprintf(stderr, "set_focus: ERROR: focuser name %s isn't C (coarse) or F (fine)\n",
	      focuser_name);
      disconnect_focuser();
      exit(-2);
    }
  }

  if(go_halfway) {
    running_time = 439000/2;
    move_absolute = true;
  }

  if (move_absolute) {
    fprintf(stderr, "moving focuser to %ld.\n", running_time);
    (void) scope_focus(running_time,
		       FOCUSER_MOVE_ABSOLUTE,
		       selected_focuser);
  } else {
    fprintf(stderr, "running focus motor for %ld msec...", running_time);
    (void) scope_focus(running_time,
		       FOCUSER_MOVE_RELATIVE,
		       selected_focuser);
  }
    
  if (system_config.NumFocusers() == 1) {
    fprintf(stderr, "Focuser position = %ld\n",
	    CumFocusPosition(FOCUSER_DEFAULT));
  } else {
    fprintf(stderr, "Focuser position = %ld (coarse), %ld (fine)\n",
	  CumFocusPosition(FOCUSER_COARSE),
	  CumFocusPosition(FOCUSER_FINE));
  }

  fprintf(stderr, "Focuser limit (system_config) is %.0lf\n",
	  system_config.FocuserMax(FOCUSER_FINE));
  disconnect_focuser();
  return 0;
}
