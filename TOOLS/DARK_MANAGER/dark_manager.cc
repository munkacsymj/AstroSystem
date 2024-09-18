/*  dark_manager.cc -- Program to manage darks for a session
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
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dark.h>

/*
 *    INVOCATION:
 * dark_manager -n qty -t exp_time -d dark_directory 
 *
 */

void usage(void) {
  fprintf(stderr, "Usage: dark_manager [-l] -n qty -t exp_time -d dark_directory\n");
  fprintf(stderr, "    -l     perform image linearity correction\n");
  exit(-2);
}

int main(int argc, char **argv) {
  int ch;			// option character
  double exposure_time_val = 0.0;
  char *dark_dir = 0;
  const char *profile = "dark";	// default profile
  const char *format_string = nullptr;
  int offset = -1;
  int readoutmode = -1;
  int gain = -1;
  bool linearize = false;
  int binning = -1;
  int quantity_val = 0;

  while((ch = getopt(argc, argv, "P:lB:z:F:m:g:n:t:d:")) != -1) {
    switch(ch) {
    case 'F':
      format_string = optarg;
      break;

    case 'P':
      profile = optarg;
      break;

    case 'l':
      linearize = true;
      break;
      
    case 'z':
      offset = atoi(optarg);
      break;

    case 'B':
      binning = atoi(optarg);
      break;

    case 'm':
      readoutmode = atoi(optarg);
      break;

    case 'g':
      gain = atoi(optarg);
      break;
      
    case 't':			// exposure time
      exposure_time_val = atof(optarg);
      break;

    case 'd':			// darkfile name
      dark_dir = optarg;
      break;

    case 'n':
      quantity_val = atoi(optarg);
      break;

    case '?':
    default:
	fprintf(stderr,
		"usage: %s -g gain -m mode -z offset -B binning -t xx.xx -l logfile -d dark.fits -c nn -f mmm\n",
		argv[0]);
	return 2;		// error return
    }
  }

  if(exposure_time_val < 0.1 || exposure_time_val > 3600.0) {
    fprintf(stderr, "dark_manager: exposure_time invalid\n");
    usage();
  }

  if(dark_dir[0] != '/') {
    fprintf(stderr, "dark_manager: directory name must be absolute path\n");
    usage();
  }
 
  if(quantity_val < 1 || quantity_val > 1000) {
    fprintf(stderr, "dark_manager: # exposures invalid\n");
    usage();
  }

  bool fatal_error = false;
  exposure_flags flags(profile);

  if (profile and gain < 0) {
    ; // do nothing; use the value from the profile
  } else {
    if (profile == nullptr and gain < 0) gain = 0;
    if (gain >= 0 and gain <= 100) {
      flags.SetGain(gain);
    } else {
      fprintf(stderr, "Invalid gain setting: %d (valid: 0..100)\n",
	      gain);
      fatal_error = true;
    }
  }

  if (profile and readoutmode < 0) {
    ; // do nothing; use the value from profile
  } else {
    if (profile == nullptr and readoutmode < 0) readoutmode = 0;
    if (readoutmode >= 0 and readoutmode <= 3) {
      flags.SetReadoutMode(readoutmode);
    } else {
      fprintf(stderr, "Invalid readoutmode setting: %d (valid: 0..3)\n",
	      readoutmode);
      fatal_error = true;
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
      fatal_error = true;
    }
  }

  int usb_traffic = -1; // no existing command-line arg???
  if (profile and usb_traffic < 0) {
    ; // do nothing; use the value from profile
  } else {
    if (profile == nullptr and usb_traffic < 0) usb_traffic = 0;
    if (usb_traffic >= 0 and usb_traffic <= 60) {
      flags.SetUSBTraffic(usb_traffic);
    } else {
      fprintf(stderr, "Invalid USB Traffic: %d (valid: 0..60)\n", usb_traffic);
      fatal_error = true;
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
      fatal_error = true;
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
	fatal_error = true;
      }
    }
  }

  if(fatal_error) exit(-2);

  const char *new_darkname = GetDark(exposure_time_val,
				     quantity_val,
				     &flags,
				     dark_dir);
  
  printf("%s", new_darkname);
  fflush(stdout);
  return 0;
}
    
  

  
