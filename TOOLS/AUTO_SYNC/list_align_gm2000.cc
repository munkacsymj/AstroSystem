/*  list_align_gm2000.cc -- List alignment stars in the mount's model
 *
 *  Copyright (C) 2017, 2020 Mark J. Munkacsy

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
#include <unistd.h>		// getopt()
#include <string.h>		// strcmp
#include <ctype.h>		// isspace()
#include <stdio.h>
#include "bright_star.h"
#include "alt_az.h"
#include "visibility.h"
#include "scope_api.h"
#include "camera_api.h"
#include "mount_model.h"
#include "named_stars.h"
#include <list>
#include <stdlib.h>		// random()
    
const char *find_match(DEC_RA &location) {
  NamedStar star(location);
  if (star.IsKnown()) {
    return star.Name();
  }
  return 0;
}

//****************************************************************
//        main()
//****************************************************************
void usage(void) {
  fprintf(stderr, "usage: list_align_gm2000 \n");
  exit(-2);
}

void err_exit(void) {
  exit(-2);
}

int main(int argc, char **argv) {
  connect_to_scope();

  char message[64];
  char response[64];
  ScopeResponseStatus status;

  if (scope_message(":getalst#",
		    RunFast,
		    StringResponse,
		    response,
		    sizeof(response),
		    &status)) {
    fprintf(stderr, "getalst: command not accepted by mount.\n");
    err_exit();
  }

  int num_align_points = atoi(response);
  fprintf(stderr, "Total of %d alignment points.\n", num_align_points);

  for (int i=1; i<= num_align_points; i++) {
    sprintf(message, ":getalp%d#", i);
    if (scope_message(message,
		      RunFast,
		      StringResponse,
		      response,
		      sizeof(response),
		      &status)) {
      fprintf(stderr, "getalp: command not accept by mount for star %d\n", i);
    } else {
      int ha_hour, ha_min, dec_hour, dec_min;
      double ha_sec, dec_sec;
      double error;
      char dec_sign;
      int pa;
      sscanf(response, "%d:%d:%lf,%c%d*%d:%lf,%lf,%d",
	     &ha_hour, &ha_min, &ha_sec,
	     &dec_sign,
	     &dec_hour, &dec_min, &dec_sec,
	     &error, &pa);
      DEC_RA location((dec_sign == '-' ? -1 : 1)*(dec_hour + dec_min/60.0 + dec_sec/3600.0)*M_PI/180.0,
		      ha_hour + ha_min/60.0 + ha_sec/3600.0);
      const char *matching_object = find_match(location);
      
      if (matching_object == 0) {
	fprintf(stderr, "Star %2d: err: %.1lf arcsec at PA = %d deg [%s]\n",
		i, error, pa, response);
      } else {
	fprintf(stderr, "Star %2d (%s): err: %.1lf arcsec at PA = %d deg\n",
		i, matching_object, error, pa);
      }
    }
  }
  fprintf(stderr, "-----------------------------\n");

#if 0
  // This doesn't work because the scope response (almost 70
  // characters) is too long for the lx_scope_response message
  // type. Sending this message will crash the scope_server in
  // jellybean.   
  sprintf(message, ":getain#");
  if (scope_message(message,
		    RunFast,
		    StringResponse,
		    response,
		    sizeof(response),
		    &status)) {
    fprintf(stderr, "getain: command not accepted by mount.\n");
  } else {
    fprintf(stderr, "%s\n", response);
  }
#endif
}
