/*  make_master_bias.cc -- Program to create bias image for a session
 *
 *  Copyright (C) 2018 Mark J. Munkacsy
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
#include <camera_api.h>
#include <scope_api.h>
#include <Image.h>
#include <gendefs.h>
#include <vector>

/*
 *    INVOCATION:
 * make_master_bias -n qty -d dark_directory 
 *
 */

void usage(void) {
  fprintf(stderr, "Usage: make_master_bias -n qty -d dark_directory\n");
  exit(-2);
}

std::vector<const char *> all_bias_images;

int main(int argc, char **argv) {
  int ch;			// option character
  char *dark_dir = 0;
  int quantity_val = 0;

  while((ch = getopt(argc, argv, "n:t:d:")) != -1) {
    switch(ch) {
    case 'd':			// darkfile name
      dark_dir = optarg;
      break;

    case 'n':
      quantity_val = atoi(optarg);
      break;

    case '?':
    default:
      usage();
      return 2;		// error return
    }
  }

  if(dark_dir == 0 || dark_dir[0] != '/') {
    fprintf(stderr, "dark_manager: directory name must be absolute path\n");
    usage();
  }

  if(quantity_val < 1 || quantity_val > 1000) {
    fprintf(stderr, "dark_manager: # exposures invalid\n");
    usage();
  }

  int sum_filenames = 0; // need to know how many chars in all filenames

  connect_to_camera();
  connect_to_scope();
  for (int i=0; i<quantity_val; i++) {
    exposure_flags flags;
    flags.SetShutterShut();	// dark
    char *dark_filename = expose_image_next(0.01 /*seconds*/, flags, "BIAS");

    char *dark_saved = strdup(dark_filename);
    all_bias_images.push_back(dark_saved);
    sum_filenames += strlen(dark_saved);
  }

  char *command = (char *) malloc(sum_filenames + 128 + quantity_val);
  if (!command) {
    fprintf(stderr, "make_master_bias: cannot allocate memory!?\n");
    exit(2);
  }

  sprintf(command, "medianaverage -o %s/bias.fits ", dark_dir);
  for (int i=0; i<quantity_val; i++) {
    strcat(command, all_bias_images[i]);
    strcat(command, " ");
  }

  if(system(command)) {
    fprintf(stderr, "medianaverage did not complete successfully.\n");
  }
}
  
    
