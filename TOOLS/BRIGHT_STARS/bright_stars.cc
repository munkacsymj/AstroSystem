/*  bright_stars.cc -- Program that lists currently-visible alignment
 *  stars for the Gemini east and west of the meridian
 *
 *  Copyright (C) 2007 Mark J. Munkacsy

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
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <visibility.h>
#include <named_stars.h>
#include <stdlib.h>
#include <alt_az.h>
#include <dec_ra.h>
#include <time.h>
#include <gendefs.h>

#define MAX_STARS 100
#define BSL_FILENAME BRIGHT_STAR_DIR "/bright_star_list.txt"

struct b_star {
  char fullname[80];
  char common_name[80];
  DEC_RA position;
  ALT_AZ *alt_az_pos;

  int east_of_meridian;
  int excluded;
};

b_star star_array[MAX_STARS];
int num_stars;			// number of valid entries in star_array

int main(int argc, char **argv) {
  FILE *bsl_fp = fopen(BSL_FILENAME, "r");

  if(!bsl_fp) {
    fprintf(stderr, "bright_stars: cannot open %s: ", BSL_FILENAME);
    exit(-2);
  }

  char buffer[80];
  char common_name[80];
  num_stars = 0;

  while(fgets(buffer, sizeof(buffer), bsl_fp)) {
    sscanf(buffer, "%s", common_name);

    struct b_star *p = star_array+num_stars;

    strcpy(p->fullname, buffer);
    p->fullname[strlen(buffer)-1]=0;
    {
      char *s = common_name;
      char *d = p->common_name;

      while(*s) *d++ = tolower(*s++);
      *d = 0;
    }

    NamedStar this_star(p->common_name);
    if(!this_star.IsKnown()) {
      fprintf(stderr, "Cannot find star named %s\n", p->common_name);
    } else {
      p->position = this_star.Location();
      p->excluded = 0; // starts as valid

      num_stars++;
    }
  }

  int n;
  JULIAN now(time(0));
  for(n=0; n<num_stars; n++) {
    
    struct b_star *p = star_array+n;
    p->alt_az_pos = new ALT_AZ(p->position, now);
    if(!IsVisible(*(p->alt_az_pos), now)) {
      p->excluded = 1;
    } else {
      p->east_of_meridian = (p->alt_az_pos->azimuth_of() < 0.0);
    }
  }

  fprintf(stderr, "\nStars East of Medidian:\n");
  for(n=0; n<num_stars; n++) {
    struct b_star *p = star_array+n;

    if(p->excluded == 0 &&
       p->east_of_meridian) {
      fprintf(stderr, "%s\n", p->fullname);
    }
  }

  fprintf(stderr, "\nStars West of Medidian:\n");
  for(n=0; n<num_stars; n++) {
    struct b_star *p = star_array+n;

    if(p->excluded == 0 &&
       !p->east_of_meridian) {
      fprintf(stderr, "%s\n", p->fullname);
    }
  }
}
