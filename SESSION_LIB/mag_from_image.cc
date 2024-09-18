/*  mag_from_image.cc -- Extract rough star brightness from finder image
 *  strategy and writes the resulting output 
 *
 *  Copyright (C) 2017 Mark J. Munkacsy
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

#include <unistd.h>		// getopt()
#include <string.h>		// strdup()
#include <ctype.h>		// toupper(), isspace()
#include <stdio.h>
#include <math.h>		// NAN, isnormal()
#include <stdlib.h>		// system()
#include <sys/types.h>
#include "mag_from_image.h"
#include <string>

static int starname_check(const char *s1, const char *s2) {
  do {
    if(*s1 == 0 && *s2 == 0) return 1; // success

    if(toupper(*s1) != toupper(*s2)) return 0; // failure

    s1++;
    s2++;
  } while (1);
  /*NOTREACHED*/
}

static char *canonical_starname(const char *s) {
  char *result = strdup(s);
  char *p = result;
  while (*p) {
    *p = tolower(*p);
    p++;
  }
  return result;
}

// returns NAN if could not get a valid brightness from the image
double magnitude_from_image(const char *image_filename,
			    const char *dark_filename,
			    const char *query_star_name,
			    const char *strategy_star_name) {
  // Do we already have an analysis file? If so, will be the same as
  // the image_filename but with .fits replaced with .analyze
  char analysis_filename[400];
  {
    strcpy(analysis_filename, image_filename);
    int last_char = strlen(image_filename);
    if (strcmp(analysis_filename+last_char-5, ".fits")) {
      fprintf(stderr, "mag_from_image: ERROR: bad filename ending: %s\n",
	      analysis_filename);
      sprintf(analysis_filename,
	      "/tmp/script_analyze%d.out", (int) getpid());
      (void) unlink(analysis_filename);
    } else {
      strcpy(analysis_filename+last_char-5, ".analyze");
    }
  }

  FILE *fp = fopen(analysis_filename, "r");
  if (!fp) {
    std::string simple_starname(canonical_starname(query_star_name));
    std::string strategy_name(canonical_starname(strategy_star_name));
    fprintf(stderr, "Looking in image for magnitude of star %s using catalog for %s\n",
	    simple_starname.c_str(), strategy_name.c_str());

    char buffer[512];

    sprintf(buffer, "analyze -d %s -n %s -o %s %s",
	    dark_filename,
	    strategy_name.c_str(),
	    analysis_filename,
	    image_filename);
    if(system(buffer) == -1) {
      perror("mag_from_image: error executing analyze shell command:");
    }

    fp = fopen(analysis_filename, "r");
  }
  
  double this_magnitude = NAN;

  if(!fp) {
    fprintf(stderr, "mag_from_image: no analysis output file\n");
    return NAN;
  }

  char buffer[512];
  while(fgets(buffer, sizeof(buffer), fp)) {
    // ignore comment lines
    if(buffer[0] == '#') continue;

    char temp_starname[128];
    char *d = temp_starname;
    char *s = buffer;

    // Move the starname from the first field of the record into the
    // temp_starname field. Then we null-terminate it and compare it
    // against the starname we are looking for.
    while(!isspace(*s)) {
      *d++ = *s++;
    }
    *d = 0;

    if(starname_check(temp_starname, query_star_name)) {
      if(sscanf(buffer+37, "%lf", &this_magnitude) != 1) {
	this_magnitude = NAN;
      }

      break;
    }
  } // end loop over all stars in the photometry file

  fclose(fp);
  fprintf(stderr, "Returning magnitude %.1lf\n", this_magnitude);
  return this_magnitude;
}
  
