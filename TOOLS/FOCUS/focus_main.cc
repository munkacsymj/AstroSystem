/*  focus_main.cc -- Main program to perform auto-focus
 *
 *  Copyright (C) 2007, 2017 Mark J. Munkacsy
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
#include <stdio.h>
#include <string.h>
#include "camera_api.h"
#include "scope_api.h"
#include <unistd.h> 		// for getopt()
#include <stdlib.h>		// for atof(), atoi()
#include <Image.h>		// get Image
#include "focus.h"
#include "focus_star.h"

// Command-line options:
//
// -d dark_frame.fits		// filename of dark frame
// -t xx.xx			// exposure time in seconds
// -f filter		        // name of filter to use
// -l logfile.txt		// name of logfile
// -s nnn                       // best guess encoder position
// -z                           // conduct special test (development only)
// -p                           // inhibit auto-plotting
// -a                           // auto-select focus star
// -n                           // no auto-find, trust initial position
// -D session_dir               // directory for session files
// -x UP | DOWN                 // specify prefered direction
// -F C | F                     // specify the focuser to use (coarse, fine)

int inhibit_plotting = 0;
int preferred_direction = DIRECTION_POSITIVE;
FocuserName focuser_to_use = FOCUSER_DEFAULT;

FILE *logfile = stdout;

void usage(void) {
  fprintf(stderr, "usage: focus <options>\n");
  fprintf(stderr, "    -d dark_frame.fits\n");
  fprintf(stderr, "    -t xx.xx [required]\n");
  fprintf(stderr, "    -f filter\n");
  fprintf(stderr, "    -l logfile.txt\n");
  fprintf(stderr, "    -s nnn [best guess]\n");
  fprintf(stderr, "    -z     [special test, don't use]\n");
  fprintf(stderr, "    -p     [inhibit plotting]\n");
  fprintf(stderr, "    -a     [auto-select focus star]\n");
  fprintf(stderr, "    -n     [no auto-find, trust initial position]\n");
  fprintf(stderr, "    -D session_dir\n");
  fprintf(stderr, "    -x UP | DOWN    [preferred direction]\n");
  fprintf(stderr, "    -F C | F [focuser to use]\n");
  exit(-2);
}

int main(int argc, char **argv) {
  int ch;			// option character
  Image *dark_image = 0;
  double exposure_time_val = 0.0;
  long best_guess = 0;
  int auto_focus_star_select = 0;
  bool no_auto_find = false;	// false == perform auto-find
  int focus_ticks = 0;
  int special_test = 0;
  Filter filter("Vc");		// default filter is the V filter
  const char *session_dir = 0;

  while((ch = getopt(argc, argv, "F:D:apx:nzs:d:t:f:l:")) != -1) {
    switch(ch) {
    case 'F':
      if (strcmp(optarg, "C") == 0 or strcmp(optarg, "c") == 0) {
	focuser_to_use = FOCUSER_COARSE;
      } else if (strcmp(optarg, "F") == 0 or strcmp(optarg, "f") == 0) {
	focuser_to_use = FOCUSER_FINE;
      } else {
	fprintf(stderr, "Illegal focuser name: %s isn't C or F.\n",
		optarg);
	usage();
      }
      break;
      
    case 'z':
      special_test = 1;
      break;

    case 'D':
      session_dir = (const char *) strdup(optarg);
      break;

    case 'n':
      no_auto_find = true;
      break;

    case 'p':
      inhibit_plotting = 1;
      break;

    case 'a':
      auto_focus_star_select = 1;
      break;

    case 'x':
      if (strcmp(optarg, "UP") == 0) {
	preferred_direction = DIRECTION_POSITIVE;
      } else if (strcmp(optarg, "DOWN") == 0) {
	preferred_direction = DIRECTION_NEGATIVE;
      } else {
	fprintf(stderr, "focus: -x option requires either UP or DOWN\n");
      }
      break;
      
    case 't':			// exposure time
      exposure_time_val = atof(optarg);
      break;

    case 's':
      best_guess = atol(optarg);
      break;

    case 'l':			// logfile name
      logfile = fopen(optarg, "w");
      if(!logfile) {
	perror("Unable to open logfile");
	logfile = stdout;
      }
      break;

    case 'd':			// darkfile name
      dark_image = new Image(optarg); // create image from dark file
      break;

    case 'f':			// name of filter to use
      filter = Filter(optarg);
      break;

    case '?':
    default:
      usage();
    }
  }

  if (special_test) {
    do_special_test();
    exit(0);
  }

  if(exposure_time_val <= 0.0 || logfile == 0) {
    usage();
  }

  connect_to_camera();
  connect_to_scope();

  if (best_guess == 0) {
    best_guess = scope_focus(0, FOCUSER_MOVE_RELATIVE, focuser_to_use);
    fprintf(stderr, "No [-s best_guess] option, so using current focuser position: %ld\n",
	    best_guess);
  } else {
    fprintf(stderr, "Using initial best guess of %ld\n", best_guess);
  }

  Image *initial_image = 0;

  if (auto_focus_star_select) {
    if (session_dir == 0 || *session_dir == 0) {
      fprintf(stderr, "focus: -a requires [-D session_dir]\n");
      exit(-2);
    }
    initial_image = find_focus_star(no_auto_find, logfile, 30.0 /*seconds*/, session_dir);
    initial_image = 0; // go ahead and allow another exposure
  }
  
  fprintf(logfile,
	  "# %s\n# %f sec exposure\n# %d encoder ticks focus travel\n",
	  DateTimeString(),
	  exposure_time_val,
	  focus_ticks);

  focus(initial_image,
	exposure_time_val, best_guess, focus_ticks, dark_image, filter);
  fflush(logfile);


}
