/* This may look like C code, but it is really -*-c++-*- */
/*  groups.cc -- manage group number in extended AAVSO format
 *
 *  Copyright (C) 2016 Mark J. Munkacsy
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

#include "groups.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *group_filename = "/home/ASTRO/CURRENT_DATA/report_groups.txt";

GroupData::GroupData(void) { // initializes from file in /home/ASTRO/CURRENT_DATA
  dirty = false;
  group_list = 0;
  max_group_number = 0;
  FlushAndRefresh();
}

GroupData::~GroupData(void) {
  FreeGroupList();
}

int
GroupData::GroupNumber(const char *aavso_starname) {
  for (int i=0; i<max_group_number; i++) {
    if (strcmp(group_list[i], aavso_starname) == 0) {
      return i;
    }
  }
  const int result = AddNewGroup(aavso_starname);
  FlushAndRefresh();
  return result;
}

void
GroupData::FlushAndRefresh(void) {
  if (dirty) {
    dirty = false;
    FreeGroupList();
  } // end if data was dirty

    // read file twice: once to count and once to read
  FILE *fp = fopen(group_filename, "r");
  max_group_number = 0;
  if (!fp) {
    fprintf(stderr, "Warning: no group data file found.\n");
    group_list = 0;
  } else {
    char buffer[64];
    while(fgets(buffer, sizeof(buffer), fp)) {
      if (buffer[0] != '\n' && buffer[0] != 0) {
	max_group_number++;
      } 
    }
    fseek(fp, 0, SEEK_SET); // rewind the file
    group_list = (const char **) malloc(max_group_number * sizeof(char *));
    int group_number = 0;
    while(fgets(buffer, sizeof(buffer), fp)) {
      if (buffer[0] != '\n' && buffer[0] != 0) {
	char *field0 = 0; // the name
	char *field1 = 0; // the group number
	// Find a comma
	for (char *s = buffer; *s; s++) {
	  if (*s == ',') {
	    field0 = buffer;
	    *s = 0;
	    field1 = s+1;
	    break;
	  }
	}
	if (field0 == 0) {
	  fprintf(stderr, "ERROR: corrupt group file: %s\n", buffer);
	} else {
	  int line_number;
	  group_list[group_number] = strdup(buffer);
	  sscanf(field1, "%d", &line_number);
	  if (line_number != group_number) {
	    fprintf(stderr, "ERROR: corrupt group file line %d\n",
		    line_number);
	  }
	  group_number++;
	}
      } // end if line wasn't empty
    } // end loop over all lines
    fclose(fp);
  } // end if open() was successful
}
      
int
GroupData::AddNewGroup(const char *aavso_starname) {
  dirty = true;
  FILE *fp = fopen(group_filename, "a");
  if (!fp) {
    perror("Error opening group_file to update:");
  } else {
    fprintf(fp, "%s, %d\n", aavso_starname, max_group_number);
  }
  fclose(fp);
  return max_group_number;
}

void
GroupData::FreeGroupList(void) {
  for (int i=0; i<max_group_number; i++) {
    free((char *) group_list[i]);
  }
  free((char **) group_list);
}

