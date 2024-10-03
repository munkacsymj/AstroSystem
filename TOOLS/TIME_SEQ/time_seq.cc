/*  time_seq_new.cc -- Obtain a time series of exposures of one particular object
 *
 *  Copyright (C) 2007,2018 Mark J. Munkacsy
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
#include <sys/types.h>
#include <sys/stat.h>
#include "camera_api.h"
#include "scope_api.h"
#include "drifter.h"
#include "proc_messages.h"
#include "running_focus.h"
#include "finder.h"
#include <Image.h>
#include <unistd.h> 		// for getopt()
#include <stdlib.h>		// for atof()
#include <gendefs.h>
#include <named_stars.h>
#include <dark.h>
#include <ctype.h>		// isdigit()
#include "system_config.h"

//#define USE_SIMULATOR

#ifdef USE_SIMULATOR
#include "focus_simulator.h"
#endif

#define STOP_TIME (4*60+55) // 04:45am
//#define STOP_TIME (2*60+18) // 02:18am
#define FLIP_TIME (7*60+1) // 12:01am; make bigger than STOP_TIME to
			   // prevent flip

Image *ProcessImage(const char *exposure_filename, Drifter *drift);

static void Terminate(void) {
  disconnect_camera();
  disconnect_scope();
  exit(-2);
}

long int char_to_time(const char *s) {
  // string must be in form of "hh:mm"
  if (isdigit(s[0]) &&
      isdigit(s[1]) &&
      s[2] == ':' &&
      isdigit(s[3]) &&
      isdigit(s[4]) &&
      s[5] == 0) {
    // perfect format:
    return (s[4]-'0') +
      (s[3] - '0')*10 +
      (s[1] - '0')*60 +
      (s[0] - '0')*600;
  } else {
    fprintf(stderr, "wrong time format (%s); must be hh:mm\n", s);
  }
  return -1;
}

const char *current_time_string(void) {
  time_t now = time(0);
  struct tm time_info;
  localtime_r(&now, &time_info);
  static char time_string[132];
  sprintf(time_string, "%02d:%02d:%02d",
	  time_info.tm_hour,
	  time_info.tm_min,
	  time_info.tm_sec);
  return time_string;
}

//****************************************************************
//        FetchOffsets(string, &north_offset, &east_offset)
//****************************************************************
void
adjust_offset(double offset, char direction,
	      double *north_offset, // these are in radians
	      double *east_offset) { // radians
  double north_delta = 0.0;
  double east_delta = 0.0;
  
  switch(direction) {
  case 'n':
  case 'N':
    north_delta = offset;
    break;

  case 's':
  case 'S':
    north_delta = -offset;
    break;

  case 'e':
  case 'E':
    east_delta = offset;
    break;

  case 'w':
  case 'W':
    east_delta = -offset;
    break;

  default:
    fprintf(stderr, "Offset must end with one of N, S, E, or W (%c)\n",
	    direction);
  }

  *north_offset += (north_delta * (2*M_PI)/360.0)/60.0;
  *east_offset +=  (east_delta * (2*M_PI)/360.0)/60.0;
}

void
FetchOffsets(const char *string,
	     double *north_offset,
	     double *east_offset) {
  *north_offset = 0.0;
  *east_offset = 0.0;
  
  if (strlen(string) > 32) {
    fprintf(stderr, "FetchOffsets: offset string too long: %s\n", string);
    *north_offset = 0;
    *east_offset = 0;
    return;
  }

  char first_substring[32];
  char second_substring[32];
  char first_direction = 0;
  char second_direction = 0;

  char *d = first_substring;
  while(isdigit(*string) || *string == '.') {
    *d++ = *string++;
  }
  *d = 0;
  d = second_substring;
  if (*string) {
    first_direction = *string++;
  } else {
    fprintf(stderr, "FetchOffsets: missing direction character.\n");
    return;
  }
  if (*string == 0) {
    // no second string
    second_substring[0] = 0;
    second_direction = 0;
  } else {
    while(isdigit(*string) || *string == '.') {
      *d++ = *string++;
    }
    *d = 0;
    second_direction = *string++;
    if (*string) {
      fprintf(stderr, "FetchOffsets: extra chars after offset string: %s\n",
	      string);
    }
  }

  double offset = atof(first_substring);
  adjust_offset(offset, first_direction, north_offset, east_offset);
  if (second_substring[0]) {
    offset = atof(second_substring);
    adjust_offset(offset, second_direction, north_offset, east_offset);
  }

  fprintf(stderr, "Using offset of %.2lf N, %.2lf E (arcmin)\n",
	  60.0*(180.0/M_PI)*(*north_offset), 60.0*(180.0/M_PI)*(*east_offset));
}

//********************************
//        Invoke dark manager
//********************************
const char *get_darkfilename(double how_long) {
  return GetDark(how_long, 1);
}

//********************************
//        usage() error
//********************************
void usage(void) {
  fprintf(stderr, "usage: time_seq [-d] [-P profile] -t xx.x -n starname [-q hh:mm] [-m hh:mm] [-f Vc] -l logfile.log\n");
  Terminate();
  /*NOTREACHED*/
}

//********************************
//    Global variables
//********************************
const char *darkfilename = 0;
const char *quickdarkname = 0;
const char *starname = 0;
char darkfilearg[256];
char quickdarkarg[256];
const char *current_dark_name = 0;
const char *profile_name = "time_seq"; // default

//********************************
//        main()
//********************************
int main(int argc, char **argv) {
  int ch;			// option character
  int t_flag = 0;		// counts # times -t option seen
  const char *logfilename = 0;
  double exposure_time_val = 0.0;
  const char *filtername = "Vc";
  bool flip_performed = false;
  bool flip_ordered = false;
  bool finished = false;
  long int stop_time_min = -1;
  long int flip_time_min = -1;
  const char *offset_string = 0;
  bool alternate_colors = false;
  bool use_drift_guider = true;
  bool use_running_focus = true;

  // Command line options:
  // -t xx.xx 		Exposure time in seconds
  // -n starname
  // -l logfile.log
  // -f Vc              filter name
  // -f filtername
  // -q quit_time       quit_time is integer = hh*60+mm
  // -m flip_time       flip_time is integer = hh*60+mm
  // -o n.nWn.nS        offset from object location
  // -P profile         profile name for exposure profile
  // -a                 alternate colors (V and R)
  // -d                 inhibit use of the drift guider
  // -r                 inhibit use of running_focus

  while((ch = getopt(argc, argv, "rdaP:o:q:m:f:t:n:l:")) != -1) {
    switch(ch) {
    case 'r':
      use_running_focus = false;
      break;
      
    case 'P':
      profile_name = optarg;
      break;
      
    case 'd':			// inhibit use of the drift guider
      use_drift_guider = false;
      break;
      
    case 't':			// exposure time
      t_flag++;
      exposure_time_val = atof(optarg);
      break;

    case 'a':
      alternate_colors = true;
      fprintf(stderr, "Alternating filters V and R.\n");
      break;

    case 'o':
      offset_string = optarg;
      break;

    case 'q':
      stop_time_min = char_to_time(optarg);
      break;

    case 'm':
      flip_time_min = char_to_time(optarg);
      flip_ordered = true;
      break;

    case 'f':
      filtername = optarg;
      break;

    case 'n':
      starname = optarg;
      break;

    case 'l':
      logfilename = optarg;
      break;

    case '?':
    default:
      usage();
      /*NOTREACHED*/
    }
  }

  if(t_flag == 0) {
    fprintf(stderr, "%s: no exposure time specified with -t\n", argv[0]);
    usage();
    /*NOTREACHED*/
  }

  FILE *logfile = 0;
  if (logfilename) {
    logfile = fopen(logfilename, "w");
    if (!logfile) {
      fprintf(stderr, "time_seq_new: unable to create logfile '%s'\n",
	      logfilename);
      usage();
      /*NOTREACHED*/
    }
  }

  if (starname == 0) {
    fprintf(stderr, "%s: no starname provided with -n\n", argv[0]);
    usage();
    /*NOTREACHED*/
  }

  time_t starting_time = time(0);
  struct tm *starting_tm = localtime(&starting_time);
  // midnight is taken as "0 minutes". Earlier than midnight is
  // "negative minutes" and later than midnight is "positive
  // minutes".
  int starting_minutes = (starting_tm->tm_hour < 12 ?
			  starting_tm->tm_hour*60 :
			  (starting_tm->tm_hour-24)*60) + starting_tm->tm_min;
  int flipping_minutes = (flip_time_min < 12*60 ? flip_time_min :
			  (flip_time_min - 24*60));
  int quitting_minutes = (stop_time_min < 12*60 ? stop_time_min :
			  (stop_time_min - 24*60));

  if (flip_ordered && flipping_minutes < quitting_minutes) {
    fprintf(stderr, "meridian flip in %d minutes.\n",
	    flipping_minutes-starting_minutes);
  } else {
    fprintf(stderr, "no meridian flip.\n");
  }
  fprintf(stderr, "quitting in %d minutes.\n",
	  quitting_minutes-starting_minutes);

  //*********************************
  //        Connect to camera & mount
  //*********************************
#ifndef USE_SIMULATOR
  connect_to_camera();
  connect_to_scope();

  //fprintf(stderr, "Disabling mount dual-axis tracking.\n");
  //SetDualAxisTracking(true);

  SystemConfig config;
  //const bool camera_ST9 = config.IsST9();

  //********************************
  //        Set up for darks
  //********************************
  darkfilename = get_darkfilename(exposure_time_val);
  if (darkfilename) {
    sprintf(darkfilearg, " -d %s ", darkfilename);
  } else {
    strcpy(darkfilearg, "  ");
  }
  quickdarkname = get_darkfilename(20.0);
  if (quickdarkname) {
    sprintf(quickdarkarg, " -d %s ", quickdarkname);
  } else {
    strcpy(quickdarkarg, "  ");
  }
#else
  InitializeSimulator("/tmp/simulator.log");
#endif
  
  //********************************
  //        Finder
  //********************************
  bool use_alternate_color = false; // when true, use filter R
  Filter filter(filtername);
  Filter alt_filter("Rc");

  NamedStar target(starname);
  if (!target.IsKnown()) {
    fprintf(stderr, "Don't know of object named %s\n", starname);
    Terminate();
  }
  DEC_RA target_loc = target.Location();
  
  if (offset_string) {
    double north_delta = 0.0;
    double east_delta = 0.0;

    FetchOffsets(offset_string, &north_delta, &east_delta);
    fprintf(stderr, "Offsetting location by (%.12lf, %.12lf) [dec, ra: radians]\n",
	    north_delta, east_delta);
    target_loc.increment(north_delta, east_delta);
  }
  MoveTo(&target_loc); // goto the target
  WaitForGoToDone();
  Finder(starname, target_loc, (1.0/60.0)*M_PI/180.0, filter); // default tolerance = 1 arcmin

  //********************************
  //        Take first image &
  //        Initialize the Drifter
  //********************************

  char filename[256];
  strcpy(filename, DateToDirname());
  strcat(filename, "/running_focus.log");
  RunningFocus focus(filename);
  if (use_running_focus) {
    focus.SetInitialImagesToIgnore(3);
  }
  
  Drifter *drift = nullptr;
  FILE *drifter_fp = nullptr;

  if (use_drift_guider) {
    strcpy(filename, DateToDirname());
    strcat(filename, "/drifter.log");
    drifter_fp = fopen(filename, "w");
    if (!drifter_fp) {
      fprintf(stderr, "Error trying to open %s as logfile\n",
	      filename);
      Terminate();
    }
    drift = new Drifter(drifter_fp);
  }

  exposure_flags flags(profile_name);
  flags.SetFilter(filter);
  flags.SetDoNotTrack();

  const double initialize_exposure_time = (exposure_time_val < 30 ? exposure_time_val : 20);
  const bool do_quick_init = (exposure_time_val >= 30);

  // There are two nested while() loops. The outer loop is traversed
  // at most two times; it is used for handling the restart required
  // after a meridian flip.
  while (!finished) {
    if (do_quick_init) {
      current_dark_name = quickdarkarg;
    } else {
      current_dark_name = darkfilearg;
    }

#ifndef USE_SIMULATOR
    
    const char *exposure_filename = expose_image_next(initialize_exposure_time, flags,
						      "DRIFT_SETUP");

    if (!exposure_filename) {
      fprintf(stderr, "time_seq_new: setup exposure failed.\n");
      return -1;
    }
    Image *first_image = new Image(exposure_filename);
    if (drift) {
      drift->SetNorthUp(first_image->GetImageInfo()->NorthIsUp());
    }
    delete first_image;
    first_image = ProcessImage(exposure_filename, drift);
    // Short exposures used for quick_init tend to confuse the focus
    // manager, because they are sharper just because they are
    // shorter. Only let drift guider initialization images be used
    // for focus if they're the right exposure length.
    if (use_running_focus and !do_quick_init) {
      focus.AddImage(first_image);
    }
    delete first_image;
  
#else
    // SIMULATOR
    double sim_now = exposure_time_val/2;
    SetSimulatorTime(sim_now);
    focus.AddImage((Image *) 0); // fake AddImage will fetch from simulator
    sim_now += exposure_time_val/2;
#endif

    if (do_quick_init) {
      fprintf(stderr, "Starting initialization with short exposures.\n");
      for (unsigned int i=0; i<7; i++) {
	int message_id = 0;
    
	// see if we've been "notify" of quit
	if (ReceiveMessage("time_seq_new", &message_id)) {
	  fprintf(stderr, "time_seq_new: received notify message. Quitting.\n");
	  Terminate();
	}
	exposure_filename = expose_image(initialize_exposure_time, flags, "DRIFT_SETUP",
					 drift);
	fprintf(logfile, "%s: %s\n",
		current_time_string(), exposure_filename);
	Image *image = ProcessImage(exposure_filename, drift);
	if (use_running_focus) {
	  focus.AddImage(image);
	}
	delete image;
      }
      // finished quick init
      fprintf(stderr, "Finished initialization with short exposures.\n");
    }

    current_dark_name = darkfilearg;
    focus.PerformFocusDither();

    // This is the inner while() loop. It is traversed hundreds of
    // times and is used for most images.
    do {
      int message_id = 0;
    
      // see if we've been "notify" of quit
      if (ReceiveMessage("time_seq", &message_id)) {
	fprintf(stderr, "time_seq: received notify message. Quitting.\n");
	break;
      }

      if (use_running_focus) {
	focus.UpdateFocus(); // adjust focus if needed
      }

#ifndef USE_SIMULATOR
      bool focus_this_image = true;
      if (alternate_colors) {
	use_alternate_color = !use_alternate_color;
	flags.SetFilter(use_alternate_color ? alt_filter : filter);
	focus_this_image = !use_alternate_color;
      }
	
      exposure_filename = expose_image(exposure_time_val, flags, "PHOTOMETRY",
				       drift);
      fprintf(logfile, "%s: %s (%s)\n",
	      current_time_string(), exposure_filename,
	      flags.FilterRequested().NameOf());
      Image *image = ProcessImage(exposure_filename, drift);
      if (use_running_focus and focus_this_image) {
	focus.AddImage(image);
      }
      delete image;
#else
      sim_now += (15 + exposure_time_val/2);
      SetSimulatorTime(sim_now);
      focus.AddImage((Image *) 0);
      sim_now += exposure_time_val/2;
#endif
      if (drift) {
	drift->print(logfile);
      }
      fflush(logfile);
#ifndef USE_SIMULATOR

      if ((!flip_performed) && flip_ordered &&
	  (time(0) - starting_time) > (flipping_minutes - starting_minutes)*60) {
	// time to perform a flip
	fprintf(stderr, "Time to perform meridian flip.\n");
	if(system("~/ASTRO/CURRENT/TOOLS/MOUNT/flip")) {
	  fprintf(stderr, "flip command did not execute okay.\n");
	} else {
	  // flip succeeded
	  flip_performed = true;
	  Finder(starname, target_loc, (1.0/60.0)*M_PI/180.0, filter);
	  // reset the drifter
	  if (use_drift_guider) {
	    drift = new Drifter(drifter_fp);
	  }
	  fprintf(stderr, "Restarting running focus.\n");
	  focus.Restart();
	  sleep(60); // pause to let the mount stabilize
	  if (use_drift_guider) {
	    break; // force out of the photometry loop to repeat the
	    // drift initialization cycle
	  }
	}
      } // end if time to do a flip
      if ((time(0) - starting_time) > (quitting_minutes - starting_minutes)*60) {
	finished = true;
	break;
      }
    } while (1); // end of photometry loop
  } // end of entire session
#else
} while (sim_now < quit_time_minutes);
#endif
  // otherwise, we're done.
  fprintf(stderr, "time_seq_new: time is up.\n");
  disconnect_camera();
  disconnect_scope();
  return 0;
}

Image *ProcessImage(const char *exposure_filename, Drifter *drift) {
  fprintf(stderr, "ProcessImage(): starting.\n");
  char command[512];
  
  sprintf(command, "find_stars  %s -i %s;star_match -e -f -b -h -n %s -i %s",
	  current_dark_name, exposure_filename, starname, exposure_filename);
  if (system(command) == -1) {
    fprintf(stderr, "time_seq_new: cannot invoke find_stars/star_match.\n");
    Terminate();
  }

  Image *image = new Image(exposure_filename); // pick up new starlist
  if (drift) {
    int status = 0;
    DEC_RA image_center = image->ImageCenter(status);
    ImageInfo *info = image->GetImageInfo();
    if (status == STATUS_OK) {
      drift->AcceptCenter(image_center, info->GetExposureMidpoint());
    }
  }
  fprintf(stderr, "ProcessImage(): finished.\n");
  return image; // this is raw (not dark-subtracted)
 }
