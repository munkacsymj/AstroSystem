/*  validation.cc -- services to validate star info against the AAVSO
 *  validation file 
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
#include <stdio.h>
#include <ctype.h>		// for isalpha()
#include <stdlib.h>		// for malloc(), exit()
#include <string.h>		// for strcat()
#include <unistd.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "validation.h"

struct valid_star {
  char *vs_desig;
  char *vs_name;
} *vs_array;

static int number_of_valid_stars = 0;
static char *validation_file_data; // holds entire contents of
				   // validation file

// Read the entire validation file and create a "valid_star" structure
// for every star in the AAVSO validation file.

void initialize_validation_file(const char *validation_directory) {
  const char *validation_name = "/valid.des";
  char *full_name = new char[strlen(validation_directory) +
			     strlen(validation_name) + 8];
  strcpy(full_name, validation_directory);
  strcat(full_name, validation_name);
  
  int fd = open(full_name, O_RDONLY);

  if(fd < 0) {
    fprintf(stderr, "validation: unable to open validation file %s\n",
	    full_name);
    exit(2);
  }

  /* the entire file will be read in to a single character array for */
  /* speed and simplicity. */
  struct stat buf;		/* status buffer */
  if(fstat(fd, &buf) < 0) {
    fprintf(stderr, "validation: unable to get status of validation file %s\n",
	    full_name);
    exit(2);
  }

  validation_file_data = (char *) malloc(buf.st_size);
  if(!validation_file_data) {
    fprintf(stderr,
	    "validation: cannot allocate memory to read validation file.\n");
    exit(2);
  }
  if(read(fd, validation_file_data, buf.st_size) != buf.st_size) {
    fprintf(stderr,
	    "validation: premature end-of-file reading validation file\n");
    exit(2);
  }

  {				// count the number of entries in the
				// file.  We'll build an array big
				// enough to hold info on all the
				// stars. 
    for(char *s = validation_file_data; *s; s++) {
      if(*s == '\n') number_of_valid_stars++;
    }
  }

  // build an array of "valid_star" structures, one for each AAVSO
  // star. 
  vs_array = new valid_star[number_of_valid_stars];

  { /* build the entries in vs_array */
    struct valid_star *vs = vs_array;
    char *s = validation_file_data;

    while(*s) {
      // every line of the validation file starts with the designation
      // string, followed by at least one space, followed by the star
      // name (which may contain spaces), followed by zero or more
      // spaces, followed by a newline.
      vs->vs_desig = s;
      // skip over the designation, then turn the first space that\'s
      // found into a null.
      while(*s && *s != ' ') s++;
      *s = '\0';			// terminate the designation
      // skip over any remaining spaces in the blank space between the
      // designation and the "name"
      s++;
      while(*s && *s == ' ') s++;
      // now looking at the start of the star name
      vs->vs_name = s;
      // jump ahead to the newline (then we\'ll back up)
      while(*s && *s != '\n') s++;
      char *end_of_line = s;
      // now back up
      do {
	s--;
      } while(*s == ' ');
      // now looking at last non-space character of the name
      s++;
      *s = '\0';
      s = end_of_line + 1;
      vs++;
    }
  }
}

/* validate_star() returns zero if the validation was successful; */
/* non-zero is returned if the validation failed.  If it fails and */
/* suppress_messages is zero, then a diagnostic message will be put */
/* onto stderr. */

static void cleanup_to_upper(const char *s, char *d) {
  while(*s) {
    if(isalpha(*s)) {
      *d = toupper(*s);
    } else {
      *d = *s;
    }
    d++;
    s++;
  }
  *d = '\0';
}

static void cleanup_no_hyphens(const char *s, char *d) {
  while(*s) {
    if(isalpha(*s)) {
      *d++ = toupper(*s);
    } else if(*s == ' ' || *s == '\t' || *s == '-') {
      ;
    } else {
      *d++ = *s;
    }
    s++;
  }
  *d = '\0';
}

int validate_star(const char *designation,
		  const char *full_name,
		  int suppress_messages) {
  if(strlen(designation) > 24 ||
     strlen(full_name) > 24) {
    fprintf(stderr, "\nvalidation: strings too long for '%s' '%s'\n",
	    designation, full_name);
    return 1;
  }
  char proper_desig[32];	// proper desig and name hold the name
  char proper_name[32];		// we\'ve been asked to validate, but
				// put in uppercase and with spaces
				// pulled out.
  cleanup_to_upper(designation, proper_desig);
  cleanup_no_hyphens(full_name, proper_name);

  struct valid_star *vs = vs_array;
  int count = number_of_valid_stars;
  while(count--) {
    if(vs->vs_name && vs->vs_desig &&
       (strcmp(vs->vs_desig, proper_desig) == 0)) {
      // MATCH!!!
      char match_name[32];
      cleanup_no_hyphens(vs->vs_name, match_name);
      if(strcmp(match_name, proper_name) == 0 ||
	 strcmp(vs->vs_name, proper_name) == 0) {
	// EVERYTHING MATCHED!!!
	return 0; // good return code
      } else {
	if(suppress_messages == 0) {
	  fprintf(stderr, "\nvalidation failed for %s %s\n",
		  designation, full_name);
	  fprintf(stderr, "Does not match %s %s\n",
		  vs->vs_desig, vs->vs_name);
	}
	return 1;		// failure return code
      }
    }
    vs++;
  }
  // if we arrived here, means that the star\'s designation didn\'t
  // match anything in the database.
  if(suppress_messages == 0) {
    fprintf(stderr, "\nvalidation failed for %s %s\n",
	    designation, full_name);
    fprintf(stderr, "No match found for that designation.\n");
  }
  return 1;
}

// Close the validation file and release all the memory used to hold
// the entries for each star.
void validation_finished(void) {
  free(validation_file_data);
  delete [] vs_array;
}

