/*  measure_linearity.cc -- Program to characterize CCD linearity
 *
 *  Copyright (C) 2019 Mark J. Munkacsy
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
#include <string.h>		// for strcat()
#include <stdio.h>
#include <unistd.h> 		// for getopt()
#include <stdlib.h>		// for atof()
#include <fitsio.h>
#include <Image.h>
#include <gendefs.h>
#include <list>
#include <camera_api.h>
#include <scope_api.h>
#include <random>



enum Purpose {
  P_CONTROL,
  P_LIGHT,
  P_DARK,
  P_BIAS,
  P_SETEXPOSURE
};

struct LStats {
  double median;
  double average;
  unsigned int num_saturated_pixels;
  unsigned int num_pixels;
};

struct LImageInfo {
  const char *filename;
  Purpose purpose;
  double exposure_time;
  Image *image;
  LStats whole_image_stats;
  LStats select_stats;
};

void BuildSeqEven(double min_t,
		  double max_t,
		  int num_exposures_each_dir,
		  std::list<double> &sequence);
void BuildSeqRandom(double min_t,
		    double max_t,
		    int num_exposures,
		    bool perform_sort,
		    std::list<double> &sequence);

FILE *logfile = 0;
std::list<LImageInfo *> all_exposures;

LImageInfo *LExpose(double exposure_time,
		    Purpose p,
		    exposure_flags *flags) {
  LImageInfo *i = new LImageInfo;
  i->filename = expose_image(exposure_time, *flags);
  i->purpose = p;
  i->exposure_time = exposure_time;
  i->image = new Image(i->filename);
  Statistics *s = i->image->statistics();
  i->whole_image_stats.median = s->MedianPixel;
  i->whole_image_stats.average = s->AveragePixel;
  i->whole_image_stats.num_pixels = (i->image->width * i->image->height);
  i->whole_image_stats.num_saturated_pixels = s->num_saturated_pixels;

  all_exposures.push_back(i);

  // add PURPOSE to FITS file
  {
    int status = 0;
    fitsfile *fptr;
    if(fits_open_file(&fptr, i->filename, READWRITE, &status)) {
      fprintf(stderr, "fits_open_file(): something went wrong.\n");
    } else {
      ImageInfo info(fptr);
      if (p == P_CONTROL) {
	info.SetPurpose("LINCONTROL");
      } else if (p == P_LIGHT) {
	info.SetPurpose("LINSEQ");
      } else if (p == P_SETEXPOSURE) {
	info.SetPurpose("LINSETUP");
      }
      info.WriteFITS(fptr);
      if(fits_close_file(fptr, &status)) {
	fprintf(stderr, "fits_close_file(): something went wrong.\n");
      }
    }
  }
  return i;
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

int main(int argc, char **argv) {
  int ch;			// option character
  char *logname = 0;
  const char *filtername = "Bc"; // default filter name
  long int stop_time_min = -1;

  // Command line options:
  // -l logfile_name       
  // -f filtername
  // -q quit_time hh:mm

  while((ch = getopt(argc, argv, "q:l:f:")) != -1) {
    switch(ch) {
    case 'l':			// name of logfile
      logname = optarg;
      break;

    case 'q':
      stop_time_min = char_to_time(optarg);
      fprintf(stderr, "Quitting time = %s\n", optarg);
      break;

    case 'f':
      filtername = optarg;
      break;

    case '?':
    default:
      fprintf(stderr,
	      "usage: %s -l logfile [-f filtername]\n", argv[0]);
      return 2;			// error return
    }
  }

  time_t starting_time = time(0);
  struct tm *starting_tm = localtime(&starting_time);
  int quitting_minutes = (stop_time_min < 12*60 ? stop_time_min :
			  (stop_time_min - 24*60));
  int starting_minutes = (starting_tm->tm_hour < 12 ?
			  starting_tm->tm_hour*60 :
			  (starting_tm->tm_hour-24)*60) + starting_tm->tm_min;
  
  fprintf(stderr, "Quitting in %d minutes.\n", quitting_minutes-starting_minutes);
  
  connect_to_camera();
  connect_to_scope();

  if (logname) {
    logfile = fopen(logname, "w");
  }

  
  if(logfile == 0) {
    fprintf(stderr, "usage: %s -l logfile [-f filtername]\n", argv[0]);
    return 2;			// error return
  }
  
  Filter filter(filtername);
  Filter invalid_filter("Invalid");
  if (filter == invalid_filter) {
    fprintf(stderr, "measure_linearity: invalid filter name: %s\n",
	    filtername);
    return 2;
  }

  exposure_flags flags;
  flags.SetFilter(filter);

  // perform initial exposure time calc.
  fprintf(stderr, "Getting exposure time info...(20 sec exposure).\n");
  LImageInfo *initial_exposure = LExpose(20.0, P_SETEXPOSURE, &flags);

  double target_saturation_time = (65535.0/initial_exposure->whole_image_stats.median)*20.0;
  //double target_saturation_time = 240;
  if (target_saturation_time <= 25.0) {
    fprintf(stderr, "Quitting. Initial 20sec exposure too close to saturation.\n");
    return 2;
  }

  // Three styles of exposure sequences:
  // 0. Evenly-spaced exposure times, run sequentially short to long
  // and back to short
  // 1. Random set of exposure times in a random order
  // 2. Random set of exposure times, run in an order from short to long
  // and back to short

  std::list<double> exp_sequence;

  int style = 0; // selects from the three available styles

  do {
    if (style == 0) {
      fprintf(stderr, "Starting sequence style 0: even spacing.\n");
      BuildSeqEven(0.0,		// min time
		   target_saturation_time + 5.0, // max time
		   10,		// # steps each direction
		   exp_sequence);

    } else if(style == 1) {
      fprintf(stderr, "Starting sequence style 1: unsorted random.\n");
      BuildSeqRandom(0.0,	// min time
		     target_saturation_time + 5.0, // max time
		     10,	// # steps total
		     false,     // sorted?
		     exp_sequence);
    } else {
      fprintf(stderr, "Starting sequence style 2: unsorted random short.\n");
      BuildSeqRandom(0.0,	// min time
		     10.0,      // max time
		     40,	// # steps total
		     false,     // sorted?
		     exp_sequence);
    }
  
    const double control_time = 5.0; // 5 seconds

    for(double t : exp_sequence) {
      fprintf(stderr, "Making exposure for %.1lf seconds\n", t);
      (void) LExpose(t, P_LIGHT, &flags);
      fprintf(stderr, "Making control exposure (%.1lf seconds)\n",
	      control_time);
      (void) LExpose(control_time, P_CONTROL, &flags);
    }

    style = (style+1)%3;

    if ((time(0) - starting_time) > (quitting_minutes - starting_minutes)*60) {
      //finished = true;
      break;
    }
    

  }  while(1);
  fprintf(stderr, "Finished.\n");
  return 0;
}

// count up from min_t to max_t evenly, then count down from max_t to
// min_t evenly
void BuildSeqEven(double min_t,
		  double max_t,
		  int num_exposures_each_dir,
		  std::list<double> &sequence) {
  sequence.clear();

  const double interval = (max_t - min_t)/(num_exposures_each_dir-1);
  double t = min_t;

  for (int i=0; i<num_exposures_each_dir; i++) {
    sequence.push_back(t);
    t += interval;
  }
  t = max_t;
  for (int i=0; i<num_exposures_each_dir; i++) {
    sequence.push_back(t);
    t -= interval;
  }
}

std::default_random_engine generator;

void BuildSeqRandom(double min_t,
		    double max_t,
		    int num_exposures,
		    bool perform_sort,
		    std::list<double> &sequence) {
  std::uniform_real_distribution<double> distribution(min_t, max_t);

  sequence.clear();
  for(int i=0; i<num_exposures; i++) {
    sequence.push_back(distribution(generator));
  }

  if (perform_sort) {
    sequence.sort();
  }
}

