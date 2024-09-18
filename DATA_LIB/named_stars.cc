/*
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
#include <stdio.h>		// for fgets()
#include <gendefs.h>
#include "named_stars.h"
#include <list>
#include <string.h>

const static char StarListFilename[] = REF_DATA_DIR "/star_list.txt";

struct OneStar {
  DEC_RA location;
  const char *starname;
};
std::list<OneStar *> all_named_stars;

void BuildNamedStarList(void) {
  if (all_named_stars.size() > 0) return;

  FILE *fp = fopen(StarListFilename, "r");

  int status = !STATUS_OK;		// haven't found the star yet

  if(!fp) {
    perror("Unable to open StarListFile");
    return;
  }

  char buffer[132];
  while(fgets(buffer, sizeof(buffer), fp)) {
    char *s;
    char starname[64];
    char dec_string[64];
    char ra_string[64];

    // terminate string at beginning of any comments
    for(s=buffer; *s; s++) {
      if(*s == '#') {
	*s = 0;
	break;
      }
    }

    // skip over leading whitespace
    s = buffer;
    while(*s == ' ' || *s == '\t') s++;

    if (*s == 0 || *s == '\n') continue;

    if (sscanf(s, "%s %s %s", starname, dec_string, ra_string) != 3) {
      fprintf(stderr, "named_stars: invalid line? '%s'\n", buffer);
      continue;
    }

    OneStar *star = new OneStar;
    star->starname = strdup(starname);
    star->location = DEC_RA(dec_string, ra_string, status);

    if (status == STATUS_OK) {
      all_named_stars.push_back(star);
    } else {
      fprintf(stderr, "Bad dec/RA for '%s'\n",
	      buffer);
    }
  }
  fclose(fp);
  return; 
}

NamedStar::NamedStar(const char *starname) {
  BuildNamedStarList();
  for (auto star : all_named_stars) {
    if (strcmp(star->starname, starname) == 0) {
      status = STATUS_OK;
      location = star->location;
      strcpy(name, star->starname);
      return;
    }
  }
  status = !STATUS_OK;
}

NamedStar::NamedStar(const DEC_RA &tgt_location) {
  BuildNamedStarList();
  for (auto star : all_named_stars) {
    const double delta_ra_rad = tgt_location.ra_radians() - star->location.ra_radians();
    const double delta_dec_rad = tgt_location.dec() - star->location.dec();
    // Use three arcminute threshold for match
    if (fabs(delta_dec_rad) < 3*(1/60.0)*M_PI/180.0 &&
	fabs(delta_ra_rad)*cos(location.dec()) < 3*(1/60.0)*M_PI/180.0) {
      status = STATUS_OK;
      location = star->location;
      strcpy(name, star->starname);
      return;
    }
  }
  status = !STATUS_OK;
}
