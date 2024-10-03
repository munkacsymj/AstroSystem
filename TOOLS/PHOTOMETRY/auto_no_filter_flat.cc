/*  auto-flat.cc -- Main program that takes charge of the entire process
 *  of creating a flat file.
 *
 *  Copyright (C) 2007, 2017, 2024 Mark J. Munkacsy
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
#include <stdio.h>
#include <unistd.h> 		// for getopt()
#include <stdlib.h>		// for atof(), atoi()
#include <Image.h>		// get Image
#include <camera_api.h>
#include <scope_api.h>
#include <gendefs.h>

// Command-line options:
//
// -o calibration_flat.fits     // filename of output file
//
//

static void Terminate(void) {
  disconnect_camera();
  disconnect_scope();
  exit(-2);
}

double saturation = -1.0;

struct FlatInfo {
  const char *filter_name;
  double first_exposure_guess;
  double exposure_time;
  char *dark_name;
  char *final_flat_name;
  bool do_this_color;
  double min_exp_time;
  double max_exp_time;
  double test_exposure_time;
  double test_median;
  std::list<char *>raw_flat_names;
} flat_data[] = {
  { "None", 1.0, 0.0, 0, 0, true }
};
const int num_filters = (sizeof(flat_data)/sizeof(flat_data[0]));
  
  const int num_bias_exposures = 20;
  const int num_dark_exposures = 5;
  const int num_flat_exposures = 5;

  const char *bias_name = TMP_IMAGE_DIR "/bias0.fits";

  const int shutter_open = 1;
  const int shutter_shut = 0;

#define MAX_ADU (saturation*0.6)
#define ADU_LOW_LIMIT (saturation*0.4)
#define ADU_TARGET (saturation*0.5)

void select_exposure_times(void) {
  // First, use test exposure to set min/max exposure times for each filter
  // Target is 40,000 to 60,000 counts median
  for (int f = 0; f < num_filters; f++) {
    FlatInfo &i = flat_data[f];
    i.exposure_time = 0.0; // indicates that time hasn't been set yet
    if (i.do_this_color) {
      i.min_exp_time = (ADU_LOW_LIMIT/i.test_median)*i.test_exposure_time;
      i.max_exp_time = (MAX_ADU/i.test_median)*i.test_exposure_time;
      fprintf(stderr, "Filter %s: min = %.2lf sec, max = %.2lf sec\n",
	      i.filter_name, i.min_exp_time, i.max_exp_time);
    } else {
      fprintf(stderr, "Filter %s: Do not do this color.\n",
	      i.filter_name);
    }
  }

  int loop_counter = 8;
 repeat:
  if (loop_counter-- == 0) {
    fprintf(stderr, "Failsafe engaged. Quitting.\n");
    return;
  }
  {
    // We look for the first filter that hasn't already been given an exposure time
    int first_needed = -1;
    double working_min, working_max;
    
    for (int f = 0; f < num_filters; f++) {
      FlatInfo &i = flat_data[f];
      if (i.exposure_time == 0.0) {
	first_needed = f;
	working_min = i.min_exp_time;
	working_max = i.max_exp_time;
	break;
      }
    }
    if (first_needed != -1) {
      for (int f = first_needed; f < num_filters; f++) {
	FlatInfo &i = flat_data[f];
	fprintf(stderr, "Working %s\n", i.filter_name);
	if (i.do_this_color && i.exposure_time == 0.0) {
	  if (i.min_exp_time < working_max && i.max_exp_time > working_min) {
	    // Yes! add to the interval
	    working_min = fmax(working_min, i.min_exp_time);
	    working_max = fmin(working_max, i.max_exp_time);
	    fprintf(stderr, "  adding to interval, now = [%.2lf to %.2lf]\n",
		    working_min, working_max);
	  }
	}
      }
      // Now assign something in this range
      int w_int = (int) (working_min + 0.999);
      double working_exp = w_int;
      double middle_exp = (working_min + working_max)/2.0;
      if (w_int > working_max) {
	// integer value won't work
	working_exp = middle_exp;
      } else {
	// integer value will work
	int low_int = w_int;
	int high_int = (int) working_max;
	int mid_int = (int) ((low_int + high_int)/2.0);
	// which is closes to midpoint?
	if (fabs(low_int - middle_exp) < fabs(mid_int - middle_exp)) {
	  working_exp = low_int;
	} else {
	  if (fabs(mid_int - middle_exp) < fabs(high_int - middle_exp)) {
	    working_exp = mid_int;
	  } else {
	    working_exp = high_int;
	  }
	}
      }
      fprintf(stderr, "Selected working exposure = %.2lf\n", working_exp);
      
      for (int f = first_needed; f < num_filters; f++) {
	FlatInfo &i = flat_data[f];
	if (i.do_this_color && i.exposure_time == 0.0) {
	  if (i.min_exp_time < working_exp && i.max_exp_time > working_exp) {
	    // Yes! add to the interval
	    i.exposure_time = working_exp;
	    fprintf(stderr, "Setting %s to %.2lf\n",
		    i.filter_name, working_exp);
	  }
	}
      }
      goto repeat;
    }
  }

  fprintf(stderr, "Exposure times: \n");
  for (int f = 0; f < num_filters; f++) {
    FlatInfo &i = flat_data[f];
    if (!i.do_this_color) {
      fprintf(stderr, "%s: skipped.\n", i.filter_name);
    } else {
      fprintf(stderr, "%s: %.1lf\n", i.filter_name, i.exposure_time);
    }
  }
}
      
void wait_for_twilight(void) {
  exposure_flags b_flag;

				// Now get a 2-second image and
				// compare medians  against the bias
  double exposure_time = 2.0;	// 2-second initial check
  double bias_rough;

  do {
    fprintf(stderr, "Waiting 2 minutes.\n");
    sleep(120);
    b_flag.SetShutterOpen();
    char *rough_name = expose_image_next(exposure_time, b_flag, "FLAT");
    Image *rough = new Image(rough_name);
    bias_rough = rough->statistics()->MedianPixel;
    delete rough;
    fprintf(stderr, "    %.1f sec median = %.0f\n", exposure_time, bias_rough);
  } while(bias_rough > MAX_ADU);
}

int build_sequence(double      exposure_time,
		   int         number_exposures,
		   int         shutter,
		   Filter      *filter,
		   std::list<char *> &raw_file_names,
		   const char *purpose) {
  int exposure_count;

  exposure_flags flags("flat");
  flags.SetFilter(*filter);
  if(shutter == shutter_open) {
    flags.SetShutterOpen();
  } else {
    flags.SetShutterShut();
  }

  for(exposure_count = 0;
      exposure_count < number_exposures;
      exposure_count++) {
    fprintf(stderr, "    starting exposure %d of %d\n",
	    exposure_count+1,
	    number_exposures);

    char *image_name = expose_image_next(exposure_time, flags, purpose);
    if(image_name == 0) return -1;

    raw_file_names.push_back(strdup(image_name));
  }

  return 0;
}
				    
void usage(void) {
  fprintf(stderr, "auto_all_filter_flat -o /home/IMAGES/date/\n");
  Terminate();
}

int main(int argc, char **argv) {
  int ch;			// option character
  char *output_dirname = 0;
  char *bias0_name = 0;
  //bool bias_available = false;

  while((ch = getopt(argc, argv, "b:o:")) != -1) {
    switch(ch) {
    case 'o':
      output_dirname = optarg;
      break;

    case 'b':
      bias0_name = optarg;
      bias_name = optarg;
      //bias_available = true;
      break;

    case '?':
    default:
      usage();
    }
  }

  if(output_dirname == 0) usage();

  connect_to_camera();
  connect_to_scope();

  exposure_flags b_flag("flat");
  Filter *filter = 0;

  b_flag.SetShutterShut();

  if (bias0_name == 0) {
    fprintf(stderr, "auto_flat: getting rough bias frame.\n");
    bias0_name = expose_image_next(0.0, b_flag, "BIAS");
  }
  Image bias0(bias0_name);	// 0.01 second bias frame
  double bias_median = bias0.statistics()->MedianPixel;

  for (int f = 0; f < num_filters; f++) {
    filter = new Filter(flat_data[f].filter_name);
    b_flag.SetFilter(*filter);
    fprintf(stderr, "Using %s filter.\n", filter->NameOf());

    {
      const int filename_len = strlen(output_dirname) + 64;
      flat_data[f].dark_name = (char *) malloc(filename_len);
      flat_data[f].final_flat_name = (char *) malloc(filename_len);
      sprintf(flat_data[f].dark_name,
	      "%s/dark_flat_%s.fits",
	      output_dirname,
	      filter->NameOf());
      sprintf(flat_data[f].final_flat_name,
	      "%s/flat_%s.fits",
	      output_dirname,
	      filter->NameOf());
    }
    
    // Now get a 2-second image and
    // compare medians  against the bias
    double exposure_time = flat_data[f].first_exposure_guess;

    double flat_rough;

    do {
      b_flag.SetShutterOpen();
      char *rough_name = expose_image_next(exposure_time, b_flag, "FLAT");
      Image rough(rough_name);
      flat_rough = rough.statistics()->MedianPixel;

      fprintf(stderr, "At %.2f secs, median is %.0f\n",
	      exposure_time, flat_rough);

      // Establish saturation point
      if (saturation < 0.0) {
	ImageInfo *info = rough.GetImageInfo();
	if (!info) {
	  fprintf(stderr, "auto_all_filter_flat: ERROR. Missing ImageInfo.\n");
	  Terminate();
	} else if (not info->DatamaxValid()) {
	  saturation = 65530.0;
	} else {
	  saturation = info->GetDatamax();
	}
      }

      // is it good??
      if(flat_rough > (ADU_LOW_LIMIT) && flat_rough <  MAX_ADU) {
	flat_data[f].test_exposure_time = exposure_time;
	flat_data[f].test_median = flat_rough;
	break;
      }

      // nope.  What was the problem?
      if(flat_rough >= MAX_ADU) {
	// need to cut the exposure time
	exposure_time /= 2.0;

	if(exposure_time < 0.0005) {
	  flat_data[f].do_this_color = false;
	  fprintf(stderr, "Skipping %s because too bright.\n",
		  filter->NameOf());
	}
      } else if(flat_rough <= ADU_LOW_LIMIT) {
	double counts_per_sec = (flat_rough -
				 bias_median)/exposure_time;
	double new_exposure_time = (ADU_TARGET -
				    bias_median)/counts_per_sec;
	if(new_exposure_time < exposure_time) {
	  fprintf(stderr, "auto_flat: logic error: %.2f < %.2f\n",
		  new_exposure_time, exposure_time);
	  Terminate();
	}

	// limit changes to a factor of 4x to stay in control
	if(new_exposure_time/exposure_time > 4.0) {
	  new_exposure_time = exposure_time * 4.0;
	}

	exposure_time = new_exposure_time;
      }
    } while(flat_rough <= ADU_LOW_LIMIT || flat_rough >=  MAX_ADU);

  }

  select_exposure_times();

  for (int f=0; f<num_filters; f++) {
    if (flat_data[f].do_this_color) {
      Filter filter(flat_data[f].filter_name);
      fprintf(stderr, "Starting flat exposure run of %d images at %.1f for %s\n",
	      num_flat_exposures,
	      flat_data[f].exposure_time,
	      flat_data[f].filter_name);
      if(build_sequence(flat_data[f].exposure_time,
			num_flat_exposures,
			shutter_open,
			&filter,
			flat_data[f].raw_flat_names,
			"FLAT") < 0) {
	Terminate();
      }
    }
  }

#if 0
  if (system("flatlight -d -w")) {
    fprintf(stderr, "Error: flatlight -d command failed.\n");
  }
  if (!bias_available) {
    fprintf(stderr, "Starting bias exposure run of %d images at %.3f\n",
	    num_bias_exposures, 0.0);
    if(build_sequence(0.0, num_bias_exposures, shutter_shut, filter, bias_name, "BIAS") < 0) {
      Terminate();
    }
  }
#endif

  for (int f=0; f<num_filters; f++) {
    // Two different ways of doing darks, depending on whether the exposure time is an integer
    double this_exp_time = flat_data[f].exposure_time;
    if (this_exp_time >= 10.0 && fabs((int(this_exp_time+0.5))- this_exp_time) < 0.01) {
      // yes, integer
      int exposure_int = (int) (this_exp_time+0.5);
      char buffer[132];
      sprintf(buffer, "dark_manager -n %d -t %d -d %s -m 1 -g 56 -z 5 ",
	      num_dark_exposures, exposure_int, output_dirname);
      fprintf(stderr, "Executing: %s\n", buffer);
      if(system(buffer)) {
	fprintf(stderr, "dark_manager returned error code.\n");
      }
      sprintf(buffer, "%s/dark%d.fits", output_dirname, exposure_int);
      flat_data[f].dark_name = strdup(buffer);
    } else {
      // not an integer
      fprintf(stderr, "Starting dark exposure run of %d images at %.1f sec for %s\n",
	      num_dark_exposures, flat_data[f].exposure_time,
	      flat_data[f].filter_name);
      std::list<char *>dark_filenames;
      if(build_sequence(this_exp_time,
			num_dark_exposures, shutter_shut, filter,
			dark_filenames, "DARK") < 0) {
	Terminate();
      }
      // make a dark
      char *cmd_buffer = (char *) malloc(256 + 256*num_dark_exposures);
      if (!cmd_buffer) {
	perror("Cannot allocate command memory:");
	Terminate();
      }
      sprintf(cmd_buffer, "medianaverage -o %s  ", flat_data[f].dark_name);
      for (auto f : dark_filenames) {
	strcat(cmd_buffer, f);
	strcat(cmd_buffer, " ");
      }
      if(system(cmd_buffer)) {
	fprintf(stderr, "combine darks returned error code.\n");
	Terminate();
      }
    }
    
    char command_string[256+256*num_flat_exposures];
    sprintf(command_string, COMMAND_DIR "/make_flat -d %s -o %s ",
	    flat_data[f].dark_name,
	    flat_data[f].final_flat_name);
    for (auto f : flat_data[f].raw_flat_names) {
      strcat(command_string, f);
      strcat(command_string, " ");
    }
    if(system(command_string)) {
      fprintf(stderr, "make_flat returned error code.\n");
    }

    fprintf(stderr, "auto_flat: flat file put into %s\n",
	    flat_data[f].final_flat_name);
  }
  disconnect_camera();
  disconnect_scope();
  return 0;
}
