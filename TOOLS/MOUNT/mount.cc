/*  mount.cc -- GM2000-specific mount commands
 *
 *  Copyright (C) 2017 Mark J. Munkacsy

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
#include <stdio.h>
#include <string.h>
#include "scope_api.h"

static void usage(void) {
  fprintf(stderr,
	  "usage: mount [-p pressure] [-t temp] [-l {start|stop|dump}]\n");
  fprintf(stderr, "pressure: either inHg or hPa (inferred from value)\n");
  fprintf(stderr,
	  "temperature: either deg-F or deg-C (must have trailing letter)\n");
  exit(-2);
}

void dump_init_data(void) {
  char response[64];
  ScopeResponseStatus Status;

  //********************************
  //        DATE
  //********************************
  if(scope_message(":GC#",	// get right ascension
		   RunFast,
		   StringResponse,
		   response,
		   0,
		   &Status)) {
    fprintf(stderr, "Cannot communicate with scope.\n");
    return;
  }
  printf("Date: %s\n", response);

  //********************************
  //        UTC Offset
  //********************************
  if(scope_message(":GG#",	// get right ascension
		   RunFast,
		   StringResponse,
		   response,
		   0,
		   &Status)) {
    fprintf(stderr, "Cannot communicate with scope.\n");
    return;
  }
  printf("UTC Offset (should be +4 or +5): %s\n", response);

  //********************************
  //        UTC Offset
  //********************************
  if(scope_message(":GG#",	// get right ascension
		   RunFast,
		   StringResponse,
		   response,
		   0,
		   &Status)) {
    fprintf(stderr, "Cannot communicate with scope.\n");
    return;
  }
  printf("UTC Offset (should be +4 or +5): %s\n", response);

  //********************************
  //        Local Time
  //********************************
  if(scope_message(":GL#",	// get right ascension
		   RunFast,
		   StringResponse,
		   response,
		   0,
		   &Status)) {
    fprintf(stderr, "Cannot communicate with scope.\n");
    return;
  }
  printf("Local time: %s\n", response);

  //********************************
  //        UTC date/time
  //********************************
  if(scope_message(":GUDT#",	// get right ascension
		   RunFast,
		   StringResponse,
		   response,
		   0,
		   &Status)) {
    fprintf(stderr, "Cannot communicate with scope.\n");
    return;
  }
  printf("UTC date/time: %s\n", response);

  //********************************
  //        Latitude
  //********************************
  if(scope_message(":Gt#",	// get right ascension
		   RunFast,
		   StringResponse,
		   response,
		   0,
		   &Status)) {
    fprintf(stderr, "Cannot communicate with scope.\n");
    return;
  }
  printf("Latitude: %s\n", response);

  //********************************
  //        Longitude
  //********************************
  if(scope_message(":Gg#",	// get right ascension
		   RunFast,
		   StringResponse,
		   response,
		   0,
		   &Status)) {
    fprintf(stderr, "Cannot communicate with scope.\n");
    return;
  }
  printf("Longitude: %s\n", response);

  //********************************
  //        Elevation
  //********************************
  if(scope_message(":Gev#",	// get right ascension
		   RunFast,
		   StringResponse,
		   response,
		   0,
		   &Status)) {
    fprintf(stderr, "Cannot communicate with scope.\n");
    return;
  }
  printf("Site elevation (meters): %s\n", response);
}

void set_mount_latlon(void) {
  char response[64];
  ScopeResponseStatus Status;

  //********************************
  //        Set Latitude
  //********************************
  if(scope_message(":St+41*34:08#",
		   RunFast,
		   FixedLength,
		   response,
		   1,
		   &Status)) {
    fprintf(stderr, "Cannot communicate with scope.\n");
    return;
  } else {
    if (response[0] == '1') {
      fprintf(stderr, "Latitude set okay.\n");
    } else if (response[0] == '0') {
      fprintf(stderr, "Mount rejected Latitude\n");
    } else {
      fprintf(stderr, "Funny response from mount: %c\n", response[0]);
    }
  }
  //********************************
  //        Set Longitude
  //********************************
  if(scope_message(":Sg+071*14:17.9#",
		   RunFast,
		   FixedLength,
		   response,
		   1,
		   &Status)) {
    fprintf(stderr, "Cannot communicate with scope.\n");
    return;
  } else {
    if (response[0] == '1') {
      fprintf(stderr, "Longitude set okay.\n");
    } else if (response[0] == '0') {
      fprintf(stderr, "Mount rejected Longitude\n");
    } else {
      fprintf(stderr, "Funny response from mount: %c\n", response[0]);
    }
  }
  dump_init_data();
}

void set_backlash(double backlash_arcmin) {
  int bl_degrees = (int) (backlash_arcmin/60.0);
  int bl_arcmin = (int) (backlash_arcmin - 60*bl_degrees);
  int bl_arcsec = (int) (backlash_arcmin*60.0 - 3600*bl_degrees - 60*bl_arcmin);

  char msg_buffer[64];
  sprintf(msg_buffer, ":Bd%02d*%02d.%02d#",
	  bl_degrees, bl_arcmin, bl_arcsec);
  fprintf(stderr, "Setting backlash to %02d:%02d:%02d\n",
	  bl_degrees, bl_arcmin, bl_arcsec);
  char response[64];
  ScopeResponseStatus Status;

  //********************************
  //        Set BACKLASH (dec)
  //********************************
  if(scope_message(msg_buffer,
		   RunFast,
		   FixedLength,
		   response,
		   1,
		   &Status)) {
    fprintf(stderr, "Cannot communicate with scope.\n");
    return;
  } else {
    if (response[0] == '1') {
      fprintf(stderr, "Backlash set okay.\n");
      dump_init_data();
    } else if (response[0] == '0') {
      fprintf(stderr, "Mount rejected backlash of %s\n",
	      msg_buffer);
    } else {
      fprintf(stderr, "Funny response from mount: %c\n", response[0]);
    }
  }
}
  

void set_mount_datetime(void) {
  struct tm *d_info;

  time_t now = time(0);
  d_info = gmtime(&now);

  char msg_buffer[64];
  sprintf(msg_buffer, ":SUDT%d-%02d-%02d,%02d:%02d:%02d#",
	  d_info->tm_year+1900,
	  d_info->tm_mon+1,
	  d_info->tm_mday,
	  d_info->tm_hour,
	  d_info->tm_min,
	  d_info->tm_sec);
  
  char response[64];
  ScopeResponseStatus Status;

  //********************************
  //        Set DATE
  //********************************
  if(scope_message(msg_buffer,
		   RunFast,
		   FixedLength,
		   response,
		   1,
		   &Status)) {
    fprintf(stderr, "Cannot communicate with scope.\n");
    return;
  } else {
    if (response[0] == '1') {
      fprintf(stderr, "Date set okay.\n");
      dump_init_data();
    } else if (response[0] == '0') {
      fprintf(stderr, "Mount rejected date/time of %s\n",
	      msg_buffer);
    } else {
      fprintf(stderr, "Funny response from mount: %c\n", response[0]);
    }
  }
}
    
  

int main(int argc, char **argv) {
  int option_char;
  double pressure;
  double temperature;
  double backlash;
  char   temp_letter;
  bool err_encountered = false;
  bool dump_setup = false; // -x option
  bool set_datetime = false; // -d option
  bool set_latlon = false; // -g (geo) option

  connect_to_scope();

  while((option_char = getopt(argc, argv, "gdxp:t:l:b:")) > 0) {
    switch (option_char) {
    case 'd':
      set_datetime = true;
      break;

    case 'g':
      set_latlon = true;
      break;

    case 'x':
      dump_setup = true;
      break;

    case 'b':
      if(sscanf(optarg, "%lf", &backlash) != 1) {
	fprintf(stderr, "-b not followed by backlash in arcmin\n");
	err_encountered = true;
      } else {
	if (backlash > 10.0 || backlash < 0.0) {
	  fprintf(stderr, "-b backlash must be +arcmin\n");
	  usage();
	}
	set_backlash(backlash);
      }
      break;
      
    case 'p':
      if(sscanf(optarg, "%lf", &pressure) != 1) {
	fprintf(stderr, "-p not followed by numeric pressure\n");
	err_encountered = true;
      } else {
	if (pressure < 100.0) {
	  // must be inHg
	  pressure = pressure/0.02953;
	}
	MountSetPressure(pressure);
      }
      break;
      
    case 't':
      if (sscanf(optarg, "%lf%c", &temperature, &temp_letter) != 2) {
	fprintf(stderr, "-t not followed by temp with trailing letter\n");
	err_encountered = true;
      } else {
	if (temp_letter == 'C') {
	  ; // no conversion needed
	} else if (temp_letter == 'F') {
	  temperature = (temperature-32.0)*5.0/9.0;
	} else {
	  fprintf(stderr, "Invalid temperature scale: %c\n", temp_letter);
	  err_encountered = true;
	}
	if (!err_encountered) {
	  MountSetTemperature(temperature);
	}
      }
      break;

    case 'l':
      if (strcmp(optarg, "start") == 0) {
	;//MountStartLogging();
      } else if (strcmp(optarg, "stop") == 0) {
	;//MountStopLogging();
      } else if (strcmp(optarg, "dump") == 0) {
	;//MountDumpLog(stdout);
      } else {
	fprintf(stderr, "Invalid logging command: %s\n", optarg);
	err_encountered = true;
	usage();
      }
      break;

    case '?':			// invalid argument
    default:
      fprintf(stderr, "Invalid argument.\n");
      exit(2);
    }
  }

  if (set_datetime) set_mount_datetime();
  if (set_latlon) set_mount_latlon();
  if (dump_setup) dump_init_data();
}
  
