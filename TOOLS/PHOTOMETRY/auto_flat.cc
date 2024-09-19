/*  auto-flat.cc -- Main program that takes charge of the entire process
 *  of creating a flat file.
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

  const int num_bias_exposures = 20;
  const int num_dark_exposures = 10;
  const int num_flat_exposures = 10;

  const char *bias_name = TMP_IMAGE_DIR "/bias0.fits";
  const char *dark_name = TMP_IMAGE_DIR "/dark0.fits";
  const char *flat_name = TMP_IMAGE_DIR "/flat0.fits";

  const int shutter_open = 1;
  const int shutter_shut = 0;

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
  } while(bias_rough > 60000.0);
}

int build_sequence(double      exposure_time,
		   int         number_exposures,
		   int         shutter,
		   Filter      *filter,
		   const char *output_fits_name,
		   const char *purpose) {
  int exposure_count;

  exposure_flags flags;
  if(shutter == shutter_open) {
    flags.SetShutterOpen();
  } else {
    flags.SetShutterShut();
  }

  char *command_name = (char *) malloc(256 + number_exposures*256);
  if(!command_name) {
    fprintf(stderr, "auto_flat: cannot allocate mem for auto command name\n");
    return -3;
  }

  sprintf(command_name, COMMAND_DIR "/medianaverage -o %s ",
	  output_fits_name);

  for(exposure_count = 0;
      exposure_count < number_exposures;
      exposure_count++) {
    fprintf(stderr, "    starting exposure %d of %d\n",
	    exposure_count+1,
	    number_exposures);

    char *image_name = expose_image_next(exposure_time, flags, purpose);
    if(image_name == 0) return -1;

    strcat(command_name, image_name);
    strcat(command_name, " ");
  }

  if(system(command_name)) {
    fprintf(stderr, "medianaverage returned error code.\n");
  }
  return 0;
}
				    
void usage(void) {
  fprintf(stderr, "auto_flat -f filter -o output.fits\n");
  exit(-2);
}

int main(int argc, char **argv) {
  int ch;			// option character
  char *filtername = 0;
  char *output_filename = 0;

  while((ch = getopt(argc, argv, "f:o:")) != -1) {
    switch(ch) {
    case 'o':
      output_filename = optarg;
      break;

    case 'f':
      filtername = optarg;
      break;

    case '?':
    default:
      usage();
    }
  }

  if(output_filename == 0) usage();

  connect_to_camera();
  connect_to_scope();

  exposure_flags b_flag;
  Filter *filter = 0;

  if (filtername) {
    filter = new Filter(filtername);
    b_flag.SetFilter(*filter);
    fprintf(stderr, "Using %s filter.\n", filter->NameOf());
  }

  fprintf(stderr, "auto_flat: getting rough bias frame.\n");

  b_flag.SetShutterShut();

  char *bias0_name = expose_image_next(0.01, b_flag, "BIAS");
  Image bias0(bias0_name);	// 0.01 second bias frame
  double bias_median = bias0.statistics()->MedianPixel;

				// Now get a 2-second image and
				// compare medians  against the bias
  double exposure_time = 2.0;	// 2-second initial check

  double bias_rough;

  do {
    b_flag.SetShutterOpen();
    char *rough_name = expose_image_next(exposure_time, b_flag, "FLAT");
    Image *rough = new Image(rough_name);
    bias_rough = rough->statistics()->MedianPixel;
    delete rough;

    fprintf(stderr, "At %.2f secs, median is %.0f\n",
	    exposure_time, bias_rough);

    // is it good??
    if(bias_rough > 40000.0 && bias_rough <  60000.0) break;

    // nope.  What was the problem?
    if(bias_rough >= 60000.0) {
      // need to cut the exposure time
      exposure_time /= 2.0;

      if(exposure_time < 0.5) {
	wait_for_twilight();
	exposure_time = 2.0;
      }
    } else if(bias_rough <= 40000) {
      double counts_per_sec = (bias_rough -
			       bias_median)/exposure_time;
      double new_exposure_time = (50000.0 -
				  bias_median)/counts_per_sec;
      if(new_exposure_time < exposure_time) {
	fprintf(stderr, "auto_flat: logic error: %.2f < %.2f\n",
		new_exposure_time, exposure_time);
	exit(-2);
      }

      // limit changes to a factor of 4x to stay in control
      if(new_exposure_time/exposure_time > 4.0) {
	new_exposure_time = exposure_time * 4.0;
      }

      exposure_time = new_exposure_time;
    }
  } while(bias_rough <= 40000.0 || bias_rough >=  60000.0);

  fprintf(stderr, "auto_flat: using exposure time of %.0f\n", exposure_time);


  fprintf(stderr, "Starting flat exposure run of %d images at %.1f\n",
	  num_flat_exposures, exposure_time);
  if(build_sequence(exposure_time, num_flat_exposures, shutter_open, filter,
		    flat_name, "FLAT") < 0) {
    exit(-2);
  }

  fprintf(stderr, "Starting bias exposure run of %d images at %.3f\n",
	  num_bias_exposures, 0.01);
  if(build_sequence(0.01, num_bias_exposures, shutter_shut, filter, bias_name, "BIAS") < 0) {
    exit(-2);
  }

  fprintf(stderr, "Starting dark exposure run of %d images at %.1f\n",
	  num_dark_exposures, exposure_time);
  if(build_sequence(exposure_time, num_dark_exposures, shutter_shut, filter,
		    dark_name, "DARK") < 0) {
    exit(-2);
  }

  char command_string[256];
  sprintf(command_string, COMMAND_DIR "/make_flat -b %s -i %s -d %s -o %s",
	  bias_name, flat_name, dark_name, output_filename);
  if(system(command_string)) {
    fprintf(stderr, "make_flat returned error code.\n");
  }

  fprintf(stderr, "auto_flat: flat file put into %s\n",
	  output_filename);
}
