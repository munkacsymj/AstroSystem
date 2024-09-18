/*  dark.h -- Program to manage darks for a session
 *
 *  Copyright (C) 2024 Mark J. Munkacsy
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

#include <list>
#include <iostream>
#include <assert.h>

#include "dark.h"
#include "Image.h"
#include <gendefs.h>
#include <camera_api.h>
#include <scope_api.h>

/*
 *    Format of dark.info file:
 *
 * qty temp time filename
 *
 * qty = <integer> providing number of exposures contributing to this dark
 * time = <%.1f>   time in seconds (to nearest tenth) of this dark
 * temp = <%.1f>   temp in degree C
 * filename = /xx/xxx...xx.fits full pathname of this file
 *
 */

const char *dark_info_name = "dark.info";

//****************************************************************
//        normalize(filename)
// This routine will take a filename and will remove any double
// slashes that are found in the filename. This fixes problems
// by IRAF's inability to handle /home/IMAGES/10-30-2015//dark30.fits,
// for example.
//****************************************************************
void normalize(char *filename) {
  char *s = filename;
  char *d = filename;
  while (*s) {
    if (*s == '/' && *(s+1) == '/') s++;
    *d++ = *s++;
  }
  *d = 0;
}

struct dark_info_item {
  int quantity;			// number of exposures
  int is_composite;		// 1=if this is processed from raw darks
  double exposure_time;		// exposure time in seconds
  double temp;			// dark camera temperature
  char *filename;		// filename of this file (full path)
  int needs_to_be_written;	// 1 means in-memory entry not on disk
};

static list<dark_info_item *> darklist;

void read_info_file(const char *dark_name) {
  for (auto d : darklist) {
    delete d;
  }
  darklist.clear();

  FILE *fp = fopen(dark_name, "r");

  if(fp == 0) return;

  char buffer[256];

  while(fgets(buffer, sizeof(buffer), fp)) {
    char filename_str[256];
    int  quantity_str;
    double exposure_time_str;
    int is_composite;
    double temp_str;

    int num_fields = sscanf(buffer, "%d %d %lf %lf %s",
			    &quantity_str,
			    &is_composite,
			    &temp_str,
			    &exposure_time_str,
			    filename_str);

    if(num_fields != 5) {
      fprintf(stderr, "dark_manager: wrong # fields in line (%d): %s\n",
	      num_fields, buffer);
      continue;
    }

    dark_info_item *new_item = new dark_info_item;
    if(!new_item) {
      perror("dark_manager: cannot allocation new_item");
      exit(-2);
    }

    new_item->needs_to_be_written = 0;
    normalize(filename_str); // eliminate double '/'
    new_item->filename      = strdup(filename_str);
    new_item->temp          = temp_str;
    new_item->is_composite  = is_composite;
    new_item->exposure_time = exposure_time_str;
    new_item->quantity      = quantity_str;

    darklist.push_back(new_item);
  }
  fclose(fp);
}


void write_info_file(const char *dark_info_name) {
  FILE *fp = fopen(dark_info_name, "a");

  if(!fp) {
    fprintf(stderr, "dark_manager: cannot open dark.info for updating\n");
    exit(-2);
  }

  for (dark_info_item *item : darklist) {
    if(item->needs_to_be_written == 0) continue;

    fprintf(fp, "%d %d %.1f %.1f %s\n",
	    item->quantity,
	    item->is_composite,
	    item->temp,
	    item->exposure_time,
	    item->filename);

    item->needs_to_be_written = 0;
  }

  fclose(fp);
}

//****************************************************************
//        Main entry point: GetDark()
//****************************************************************
const char *GetDark(double exposure_time,
		    int quantity,
		    exposure_flags *flags,
		    const char *image_dir) {
  if (flags == nullptr) {
    flags = new exposure_flags("dark");
  }

  if (image_dir == nullptr) {
    image_dir = DateToDirname();
  }

  if (exposure_time < 0.001) {
    std::cerr << "dark_manager: exposure_time invalid\n";
    return nullptr;
  }

  assert(image_dir[0] == '/');	// image_dir must be absolute path

  assert(quantity >= 1 and quantity <= 1000);

  // construct an absolute pathname
  char full_dark_info[strlen(image_dir) +
		      strlen(dark_info_name) + 10];
  sprintf(full_dark_info, "%s/%s", image_dir, dark_info_name);
  normalize(full_dark_info);

  // read the dark info in the directory
  read_info_file(full_dark_info);

  // see if we already have what we need

  // in case we need to make some more exposures, here we count the
  // number we currently have available
  int number_to_use = 0;

  for (dark_info_item *item : darklist) {
    // if we cared about temperate, we'd do in in the following "if"
    if(fabs(item->exposure_time - exposure_time) < 0.001) {
      if(item->quantity >= quantity and item->is_composite) {
	// Yes! We don't need to do anything!
	return item->filename;
      } else if(item->quantity == 1 && item->is_composite == 0) {
	number_to_use++;
      }
    }
  }

  // Okay, we need to do some stuff. Start by grabbing any needed exposures.
  
  connect_to_camera();
  connect_to_scope();
  flags->SetShutterShut();	// dark

  while(number_to_use < quantity) {
    char *dark_filename = expose_image_next(exposure_time,
				       *flags, "DARK");
    fprintf(stderr, "%s DARK for %.3f seconds\n",
	    dark_filename, exposure_time);

    dark_info_item *new_dark    = new dark_info_item;
    new_dark->is_composite      = 0;
    new_dark->exposure_time     = exposure_time;
    new_dark->filename          = strdup(dark_filename);
    new_dark->quantity          = 1;
    new_dark->temp              = 0.0;
    new_dark->needs_to_be_written = 1;

    number_to_use++;
    darklist.push_back(new_dark);
  }

  const int dark_count = quantity;
  char command[dark_count*(15+strlen(image_dir)) + 90];
  char new_darkname[128];

  int int_exp_time = (int) (exposure_time + 0.5);
  if (fabs(exposure_time - (double) int_exp_time) > 0.001) {
    // dealing with a fractional exposure time
    int exp_time_msec = (int) ((exposure_time+0.0005)*1000);
    sprintf(new_darkname, "%s/dark%d_%03d.fits",
	    image_dir,
	    exp_time_msec/1000,
	    exp_time_msec - 1000*(exp_time_msec/1000));
  } else {
    // even number of seconds
    sprintf(new_darkname, "%s/dark%d.fits", image_dir, int_exp_time);
  }
    
  normalize(new_darkname); // eliminate any double '/'

  if (number_to_use < 4) {
    sprintf(command, COMMAND_DIR "/average -o %s ", new_darkname);
  } else {
    sprintf(command, COMMAND_DIR "/medianaverage -o %s ", new_darkname);
  }

  for (dark_info_item *item : darklist) {
    if(fabs(item->exposure_time - exposure_time) < 0.0005 &&
       item->quantity == 1 && item->is_composite == 0) {
      strcat(command, item->filename);
      strcat(command, " ");
    }
  }
    
  fprintf(stderr, "Averaging %d darks.\n", dark_count);
  if(system(command) == -1) {		// execute the average
    perror("dark_manager: unable to execute shell command:");
  }

  dark_info_item *new_dark    = new dark_info_item;
  new_dark->exposure_time     = exposure_time;
  new_dark->filename          = new_darkname;
  new_dark->is_composite      = 1;
  new_dark->quantity          = quantity;
  new_dark->temp              = 0.0;
  new_dark->needs_to_be_written = 1;

  darklist.push_back(new_dark);
  
  write_info_file(full_dark_info);

  return strdup(new_darkname);
}
  
