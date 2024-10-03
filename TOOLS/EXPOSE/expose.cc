/*  expose.cc -- Program to perform a camera exposure
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
#include <stdio.h>
#include <string.h>
#include "camera_api.h"
#include "scope_api.h"
#include <Image.h>
#include <unistd.h> 		// for getopt()
#include <stdlib.h>		// for atof()

void usage(void) {
  fprintf(stderr, "usage: expose -t n.n [-d] [-o filename] [-p purpose] [-f filter]\n");
  fprintf(stderr, "       [-b nnn -u nnn -l nnn -r nnn] [-g nn] [-m nn] [-c]\n");
  fprintf(stderr, "       [-B n] [-F xx] [-z nn] [-P profile]\n");
  fprintf(stderr, "  d: darkimage\n  g: gain [0..100]\n  m: mode [0,1,2,3]\n  c: compress\n");
  fprintf(stderr, "  B: binning\n  F: 16|32|float\n  z: offset[0..255]\n  U: USB Traffic [0..60]\n");
  exit(-2);
}

int main(int argc, char **argv) {
  int ch;			// option character
  int t_flag = 0;		// counts # times -t option seen
  int o_flag = 0;		// counts # times -o option seen
  int d_flag = 0;		// dark? (shutter kept shut)
  int box_upper = -1;
  int box_bottom = -1;
  int box_right = -1;
  int box_left = -1;
  const char *purpose = nullptr;
  const char *profile = "default";
  const char *filter = "";
  char output_file[256];
  double exposure_time_val = 0.0;
  bool do_compress = true;
  const char *format_string = nullptr;
  int gain {-1};
  int mode {-1};
  int binning {-1};
  int offset {-1};
  double usb_traffic {-1};
  
  // Command line options:
  // -t xx.xx 		Exposure time in seconds
  // -o filename.fits   Output filename
  // -u nn              Box upper bound
  // -b nn              Box bottom bound
  // -r nn              Box right bound
  // -l nn              Box left bound
  // -d                 Dark image; keep shutter shut
  // -c                 Do not compress
  // -f filtername      Use the specified filter instead of the default
  // -p purpose         Specify PURPOSE keyword for FITS header
  // -P profile         Profile name for exposure flags
  // -U traffic         USB traffic setting
  // -z offset          Camera offset

  while((ch = getopt(argc, argv, "U:p:cdu:b:r:l:t:o:f:g:P:m:B:F:z:")) != -1) {
    switch(ch) {
    case 'c':
      do_compress = false;
      break;

    case 'U':
      usb_traffic = atoi(optarg);
      break;

    case 'z':
      offset = atoi(optarg);
      break;

    case 'd':
      d_flag++;
      break;

    case 'p':
      purpose = optarg;
      break;

    case 'P':
      profile = optarg;
      break;

    case 't':			// exposure time
      t_flag++;
      exposure_time_val = atof(optarg);
      break;

    case 'o':
      o_flag++;
      if (strlen(optarg) > 200) {
	fprintf(stderr, "output filename too long.\n");
	output_file[0] = 0;
      } else {
	strcpy(output_file, optarg);
      }
      break;

    case 'u':
      box_upper = atoi(optarg);
      break;

    case 'b':
      box_bottom = atoi(optarg);
      break;

    case 'r':
      box_right = atoi(optarg);
      break;

    case 'l':
      box_left = atoi(optarg);
      break;

    case 'f':
      filter = optarg;
      break;

    case 'g':
      gain = atoi(optarg);
      break;

    case 'm':
      mode = atoi(optarg);
      break;

    case 'B':
      binning = atoi(optarg);
      break;

    case 'F':
      format_string = optarg;
      break;

    case '?':
    default:
      usage();
    }
  }

  if(t_flag == 0) {
    fprintf(stderr, "%s: no exposure time specified with -t\n", argv[0]);
    usage();
  }

  if(t_flag > 1) {
    fprintf(stderr, "%s: -t specified more than once.\n", argv[0]);
    usage();
  }

  if(box_upper != -1 || box_bottom != -1|| box_right != -1|| box_left != -1) {
    if(box_bottom >= box_upper ||
       box_bottom < 0 ||
       box_left >= box_right ||
       box_left < 0) {
      fprintf(stderr, "expose: must have bottom<upper & left<right\n");
      return 2;
    }
  }

  connect_to_camera();
  connect_to_scope();

  if(!o_flag) {
    strcpy(output_file,  NextValidImageFilename());
    fprintf(stdout, "%s", output_file);
  }

  const char *image_filename = output_file;

  exposure_flags flags(profile);
  if(d_flag) {
    flags.SetShutterShut();
  } else {
    flags.SetShutterOpen();
  }
  flags.SetDoNotTrack();
  flags.SetCompression(do_compress);

  Filter this_filter;
  if (filter && *filter) {
    this_filter = Filter(filter);	// convert string to Filter
    flags.SetFilter(this_filter);
  } else {
    if (!GetDefaultFilter(this_filter)) {
      // Unable to find the default filter
      fprintf(stderr, "Warning: no default filter information available.\n");
    } else {
      flags.SetFilter(this_filter);
    }
  }

  if (profile and gain < 0) {
    ; // do nothing; use the value from the profile
  } else {
    if (profile == nullptr and gain < 0) gain = 0;
    if (gain >= 0 and gain <= 100) {
      flags.SetGain(gain);
    } else {
      fprintf(stderr, "Invalid gain setting: %d (valid: 0..100)\n",
	      gain);
      return -2;
    }
  }

  if (profile and mode < 0) {
    ; // do nothing; use the value from profile
  } else {
    if (profile == nullptr and mode < 0) mode = 0;
    if (mode >= 0 and mode <= 3) {
      flags.SetReadoutMode(mode);
    } else {
      fprintf(stderr, "Invalid mode setting: %d (valid: 0..3)\n",
	      mode);
      return -2;
    }
  }

  if (profile and binning < 0) {
    ; // do nothing; use the value from profile
  } else {
    if (profile == nullptr and binning < 0) binning = 1;
    if (binning > 0 and binning < 10) {
      flags.SetBinning(binning);
    } else {
      fprintf(stderr, "Invalid binning: %d (valid: 1..9)\n", binning);
      return -2;
    }
  }

  if (profile and usb_traffic < 0) {
    ; // do nothing; use the value from profile
  } else {
    if (profile == nullptr and usb_traffic < 0) usb_traffic = 0;
    if (usb_traffic >= 0 and usb_traffic <= 60) {
      flags.SetUSBTraffic(usb_traffic);
    } else {
      fprintf(stderr, "Invalid USB Traffic: %.0lf (valid: 0..60)\n", usb_traffic);
      return -2;
    }
  }

  if (profile and offset < 0) {
    ; // do nothing; use the value from profile
  } else {
    if (profile == nullptr and offset < 0) offset = 5;
    if (offset >= 0 and offset < 256) {
      flags.SetOffset(offset);
    } else {
      fprintf(stderr, "Invalid offset: %d (valid: 0..255)\n", offset);
    }
  }

  if (profile and format_string == nullptr) {
    ; // do nothing; use the value from profile
  } else {
    if (profile == nullptr and format_string == nullptr) format_string="32";
    if (format_string and *format_string) {
      if (strcmp(format_string, "16") == 0) {
	flags.SetOutputFormat(exposure_flags::E_uint16);
      } else if (strcmp(format_string, "32") == 0) {
	flags.SetOutputFormat(exposure_flags::E_uint32);
      } else if (strcmp(format_string, "float") == 0) {
	flags.SetOutputFormat(exposure_flags::E_float);
      } else {
	fprintf(stderr, "Invalid file format: %s (valid: 16, 32, float)\n",
		format_string);
	return -2;
      }
    }
  }

  if (profile and (box_upper < 0 or
		   box_bottom < 0 or
		   box_right < 0 or
		   box_left < 0)) {
    ; // do nothing; use the values from profile
  } else {
    if (profile == nullptr and (box_upper < 0 or
				box_bottom < 0 or
				box_right < 0 or
				box_left < 0)) {
      flags.subframe.box_left =
	flags.subframe.box_right =
	flags.subframe.box_bottom =
	flags.subframe.box_top = 0;
    }
    if (box_upper >= 0 and box_bottom >= 0 and
	box_left >= 0 and box_right >= 0) {
      flags.subframe.box_left = box_left;
      flags.subframe.box_right = box_right;
      flags.subframe.box_bottom = box_bottom;
      flags.subframe.box_top = box_upper;
    }
  }

  // now do the dirty deed.  Make an exposure
  fprintf(stderr, "Starting exposure of %f seconds.\n", exposure_time_val);

  do_expose_image(exposure_time_val,
		  flags,
		  image_filename,
		  purpose);

  fflush(stdout);
  fprintf(stderr, "\n");
  disconnect_camera();
  disconnect_scope();
  return 0;
}
