/*  obs_spreadsheet.cc -- maintains the aavso.csv spreadsheet that
 *  lists all observations made during the session
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
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <unistd.h>
#include <libgen.h>		// for basename
#include "obs_spreadsheet.h"

static char *SpreadsheetName = 0;

/****************************************************************/
/*        Class Spreadsheet_Filelist				*/
/****************************************************************/
SpreadSheet_Filelist::SpreadSheet_Filelist(void) {
  current_allocation_size = current_size = 0;
  NumberList = 0;
}

SpreadSheet_Filelist::~SpreadSheet_Filelist(void) {
  if(NumberList) free(NumberList);
}

void
SpreadSheet_Filelist::Add_Filename(const char *filename) {
  int num;
  char *local_name = strdup(filename);

  char *base = basename(local_name);

  while(*base && !isdigit(*base)) base++;
  if(isdigit(*base)) {
    sscanf(base, "%d", &num);
  } else {
    fprintf(stderr, "obs_spreadsheet: cannot parse image number from %s\n",
	    filename);
    return;
  }

  // at this point we have the image number
  if(current_allocation_size == current_size) {
    current_allocation_size += 30;
    NumberList = (int *) realloc(NumberList,
				 sizeof(int) * current_allocation_size);
    if(!NumberList) {
      fprintf(stderr, "obs_spreadsheet: realloc failed\n");
      return;
    }
  }
  NumberList[current_size++] = num;
}

char *
SpreadSheet_Filelist::GetImageList(void) {
  int biggest;
  int smallest;

  biggest = smallest = NumberList[0];

  for(int i = 0; i<current_size; i++) {
    if(NumberList[i] < smallest) smallest = NumberList[i];
    if(NumberList[i] > biggest)  biggest  = NumberList[i];
  }

  if(smallest + current_size - 1 == biggest) {
    // complete set!
    char *spread = (char *) malloc(16);
    sprintf(spread, "%d - %d", smallest, biggest);
    return spread;
  } else {
    return strdup("various");
  }
}
    

/****************************************************************/
/*        Initialize_Spreadsheet				*/
/****************************************************************/

void
Initialize_Spreadsheet(const char * spreadsheet_name) {
  if(!spreadsheet_name) {
    fprintf(stderr, "obs_spreadsheet: error: <nil> spreadsheet_name\n");
    return;
  }

  // if Initialize is called more than once, just quietly forget the
  // prior name we were given.
  if(SpreadsheetName) {
    free((void *)SpreadsheetName);
  }
  // Save the name for later use.
  SpreadsheetName = strdup(spreadsheet_name);
  if(!SpreadsheetName) {
    perror("Cannot allocate space for spreadsheet name");
    return;
  }
  
  int fd = open(spreadsheet_name, O_WRONLY|O_APPEND|O_CREAT , 0777);
  if(fd < 0) {
    perror("Cannot create spreadsheet file");
  } else {
    close(fd);
  }
}

/***************************************************************
  StandardFormOfStarname(char *starname)
 ***************************************************************/
void
CapitalizeConstellation(char *name) {
  char *p = name;
  while(*p) {
    *p = tolower(*p);
    p++;
  }
  *name = toupper(*name);
  if(strcmp(name, "Uma") == 0 ||
     strcmp(name, "Umi") == 0 ||
     strcmp(name, "Cma") == 0 ||
     strcmp(name, "Cmi") == 0 ||
     strcmp(name, "Cvn") == 0 ||
     strcmp(name, "Lmi") == 0) {
    name[1] = toupper(name[1]);
  }

  if(strcmp(name, "Cra") == 0 ||
     strcmp(name, "Crb") == 0 ||
     strcmp(name, "Psa") == 0 ||
     strcmp(name, "Tra") == 0) {
    name[2] = toupper(name[2]);
  }
}

char *
StandardFormOfStarname(const char *input_name) {
  static char answer[80];

  strcpy(answer, input_name);
  char *s;

  /* States:
     0 = star "letter" (e.g., "RR")
     1 = in constellation name
   */

  int state = 0;
  for(s=answer; *s; s++) {
    if(*s == '-') {
      state = 1;
      *s = ' ';
      CapitalizeConstellation(s+1);
      break;
    }
    if(state == 0) {
      if(isalpha(*s)) *s = toupper(*s);
    }
  }

  return answer;
}

/****************************************************************/
/*        Add_Spreadsheet_Entry					*/
/****************************************************************/
void
Add_Spreadsheet_Entry(const char * star_name,
		      const char * star_designation,
		      SpreadSheet_Filelist * filelist,
		      JULIAN       obs_time) {
  if(SpreadsheetName == 0) return;

  char buffer[256];

  if(star_name == 0 ||
     star_designation == 0 ||
     filelist == 0) {
    fprintf(stderr,
	    "obs_spreadsheet: Add_Spreadsheet_Entry: Nil argument value\n");
    return;
  }

  char *filename_info = filelist->GetImageList();
  sprintf(buffer, "%s,%s,%s,%s,,,,\n",
	  star_name,
	  //StandardFormOfStarname(star_name),
	  star_designation,
	  filename_info,
	  obs_time.sprint(1));

  FILE *fp = fopen(SpreadsheetName, "a");
  if(!fp) {
    fprintf(stderr, "obs_spreadsheet: cannot reopen spreadsheet %s\n",
	    SpreadsheetName);
  } else {
    fputs(buffer, fp);
    fclose(fp);
  }

  free(filename_info);
  // caller responsibility to destroy filelist
}
