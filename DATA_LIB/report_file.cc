/*  report_file.cc -- Manage the extended format AAVSO report 
 *
 *  Copyright (C) 2008 Mark J. Munkacsy
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
#include "report_file.h"
#include <stdio.h>
#include <string.h>		// strcpy()
#include <stdlib.h>		// malloc(), atof()

#define SKELETON_CHARACTER '~'

ReportFileLine::ReportFileLine(void) {
  report_name[0] = 0;
  jd = -1.0;
  magnitude = -999.0;
  error_estimate = 0.0;
  filter[0] = 0;
  transformed = 0;
  mtype = MTYPE_ABS;
  comp_name[0] = 0;
  comp_magnitude = -999.0;
  check_name[0] = 0;
  check_magnitude = -999.0;
  airmass = -1;
  group = -1;
  chart[0] = 0;
  notes[0] = 0;
}

// returns status
int
ReportFileLine::BuildReportFileLine(int word_count,
				    char **words,
				    int is_skeleton) {
  if(word_count != 15 && !is_skeleton) {
    fprintf(stderr, "report_file: wrong # fields: %d\n", word_count);
    return -1;
  }

  // STAR NAME
  strcpy(report_name, words[0]);

  // JULIAN DAY
  if(word_count > 1) {
    jd = atof(words[1]);
    if(*words[1] == 0 && is_skeleton) {
      jd = 0.0;
    } else if((jd != 0.0) && (jd < 2000000.0 || jd > 3000000.0)) {
      fprintf(stderr, "report_file: illogical julian date: %s\n",
	      words[1]);
      return -1;
    }
  } else {
    jd = 0.0;
  }

  // MAGNITUDE
  if(word_count > 2) {
    magnitude = atof(words[2]);
    if(*words[2] == 0 && is_skeleton) {
      magnitude = -999.0;
    } else if(magnitude < -1000.0 ||
	      (magnitude > -998.0 && (magnitude < -2.0 || magnitude > 25.0))) {
      fprintf(stderr, "report_file: illogical magnitude: %s\n",
	      words[2]);
      return -1;
    }
  } else {
    magnitude = -999.0;
  }

  // ERROR
  if(word_count > 3) {
    error_estimate = atof(words[3]);
    if(*words[3] == 0 && is_skeleton) {
      error_estimate = 0.0;
    } else if(error_estimate > 1.0 || error_estimate < 0.0) {
      fprintf(stderr, "report_file: illogical error estimate: %s\n",
	      words[3]);
      return -1;
    }
  } else {
    error_estimate = 0.0;
  }

  // FILTER
  if(word_count > 4) {
    strcpy(filter, words[4]);
  } else {
    filter[0] = 0;
  }

  // TRANSFORMED
  if(word_count > 5) {
    if(strcmp(words[5], "NO") == 0) {
      transformed = 0;
    } else if(strcmp(words[5], "YES") == 0) {
      transformed = 1;
    } else if(!is_skeleton) {
      fprintf(stderr, "report_file: illogical 'transformed' field: %s\n",
	      words[5]);
      return -1;
    } else {
      transformed = 0; // skeleton
    }
  } else {
    transformed = 0;
  }

  // MAGNITUDE TYPE
  if(word_count > 6) {
    if(strcmp(words[6], "ABS") == 0) {
      mtype = MTYPE_ABS;
    } else if(strcmp(words[6], "DIF") == 0) {
      mtype = MTYPE_DIF;
    } else if(is_skeleton) {
      mtype = MTYPE_ABS;
    } else {
      fprintf(stderr, "report_file: illogical mag type: %s\n", words[6]);
      return -1;
    }
  } else {
    mtype = MTYPE_ABS;
  }

  // COMP STAR
  if(word_count > 7) {
    strcpy(comp_name, words[7]);
  } else {
    comp_name[0] = 0;
  }

  // COMP MAGNITUDE
  if(word_count > 8) {
    comp_magnitude = atof(words[8]);
    if(*words[8] == 0 && is_skeleton) {
      comp_magnitude = -999.0;
    } else if(comp_magnitude < -1000.0 ||
	      (comp_magnitude > -998.0 &&
	       (comp_magnitude < -2.0 || comp_magnitude > 25.0))) {
      fprintf(stderr, "report_file: illogical comp star mag: %s\n",
	      words[8]);
      return -1;
    }
  } else {
    comp_magnitude = -999.0;
  }

  //CHECK STAR
  if(word_count > 9) {
    strcpy(check_name, words[9]);
  } else {
    check_name[0] = 0;
  }

  // CHECK MAGNITUDE
  if(word_count > 10) {
    check_magnitude = atof(words[10]);
    if((*words[10] == 0 && is_skeleton) ||
       strcmp(words[10], "na") == 0 ||
       strcmp(words[10], "NA") == 0 ||
       strcmp(words[10], "Na") == 0) {
      check_magnitude = -999.0;
    } else if(check_magnitude < -1000.0 ||
	      (check_magnitude > -998.0 &&
	       (check_magnitude < -2.0 || check_magnitude > 25.0))) {
      fprintf(stderr, "report_file: illogical check star mag: %s\n",
	      words[8]);
      return -1;
    }
  } else {
    check_magnitude = -999.0;
  }

  // AIRMASS
  if(word_count > 11) {
    if(strcmp(words[11], "na") == 0) {
      airmass = -1.0;
    } else {
      airmass = atof(words[11]);
      if(*words[11] == 0 && is_skeleton) {
	airmass = -1.0;
      } else if(airmass != -1.0 && (airmass < 0.0 || airmass > 40.0)) {
	fprintf(stderr, "report_file: illogical value for airmass: %s\n",
		words[11]);
	return -1;
      }
    }
  } else {
    airmass = -1.0;
  }

  // GROUP
  
  if(word_count <= 12 || strcmp(words[12], "na") == 0) {
    group = -1;
  } else {
    group = atoi(words[12]);
  }

  // CHART
  if(word_count > 13) {
    if(strlen(words[13]) > 11) {
      fprintf(stderr, "report_file: illogical value for chart: %s\n",
	      words[13]);
      return -1;
    }
    strcpy(chart, words[13]);
  } else {
    chart[0] = 0;
  }

  // NOTES
  if(word_count <= 14 || strcmp(words[14], "na") == 0) {
    notes[0] = 0;
  } else if(strlen(words[14]) > 63) {
    fprintf(stderr, "report_file: notes too long: %s\n", words[14]);
    return -1;
  } else {
    strcpy(notes, words[14]);
  }

  skeleton = is_skeleton;
  return is_skeleton ? RFL_SKELETON : 0; // *now* it is successful
}

ReportFileLine::ReportFileLine(int word_count,
			       char **words,
			       int is_skeleton,
			       int *status) {
  *status = BuildReportFileLine(word_count, words, is_skeleton);
}

ReportFileLine::ReportFileLine(const char *string, char delim, int *status) {
  // assume status is bad. Just makes it more convenient when things
  // go wrong.
  char delim_string[2];
  delim_string[0] = delim;
  delim_string[1] = 0;

  *status = -1;

  //
  // SS CYG,2450702.1234,11.235,0.003,B,NO,ABS,105,10.593,110,11.090,1.561,1,070613,na
  //

#define MAX_NUM_FIELDS 24

  char **ap;
  char *argv[MAX_NUM_FIELDS];	// field pointers go here
  char buffer[132];
  char *input = buffer;

  if(strlen(string) > sizeof(buffer)) {
    fprintf(stderr, "report_file: input string too long.\n");
    return;
  }

  int is_skeleton = 0;		// assume not a skeleton entry
  if(*string == SKELETON_CHARACTER) {
    string++;
    is_skeleton = 1;
  }
  strcpy(buffer, string);

  {
    char *last_char = buffer + strlen(buffer) - 1;
    if(*last_char == '\n') *last_char = 0;
  }

  int num_fields = 0;
  for(ap = argv; num_fields < MAX_NUM_FIELDS &&
	(*ap = strsep(&input, delim_string)) != NULL; ) {
    num_fields++;
    ap++;
  }

  *status = BuildReportFileLine(num_fields, argv, is_skeleton);
}

#if 0 // These two functions not currently needed.
static char *IntString(int d) {
  static char buffer[12];
  sprintf(buffer, "%d", d);
  return buffer;
}

static char *DoubleString(double d, const char *format) {
  static char buffer[22];
  sprintf(buffer, format, d);
  return buffer;
}
#endif

char *
ReportFileLine::ToString(int *status, char delim) {
  char *d = (char *) malloc(512);
  if(d == 0) {
    *status = -1;
    fprintf(stderr, "ReportFileLine::ToString() malloc() failed\n");
    return 0;
  }

  char delim_string[2];
  delim_string[0] = delim;
  delim_string[1] = 0;

  int word_count;
  char **words = ToWordList(&word_count, status);
  *d = 0;
  for(int i=0; i<word_count; i++) {
    if(i != 0) strcat(d, delim_string);
    strcat(d, words[i]);
  }

  return d;
}

  // The return value and the strings it points to are allocated
  // statically. Don't try to free() anything.
char **
ReportFileLine::ToWordList(int *word_count, int *status) {
  static char *words[15];
  static char WL_obj_name[32];
  static char WL_notes[80];
  static char WL_fields[15][16];

  words[0] = WL_obj_name;
  words[14] = WL_notes;
  for(int n=1; n< 14; n++) {
    words[n] = WL_fields[n];
  }

  *status = -1;

  // STAR NAME
  strcpy(WL_obj_name, report_name);

  // JULIAN DAY
  sprintf(WL_fields[1], "%.4lf", jd);

  // MAGNITUDE
  sprintf(WL_fields[2], "%.3lf", magnitude);

  // ERROR
  sprintf(WL_fields[3], "%.3lf", error_estimate);

  // FILTER
  strcpy(WL_fields[4], filter);

  // TRANFORMED
  strcpy(WL_fields[5], (transformed ? "YES" : "NO"));

  // MAGNITUDE TYPE
  strcpy(WL_fields[6], "ABS");

  // COMP STAR
  strcpy(WL_fields[7], comp_name);

  // COMP MAGNITUDE
  sprintf(WL_fields[8], "%.3lf", comp_magnitude);

  // CHECK STAR NAME
  if(check_name[0]) {
    strcpy(WL_fields[9], check_name);

    // CHECK MAGNITUDE
    if(check_magnitude < -999.5 || check_magnitude > -998.5)
      sprintf(WL_fields[10], "%.3lf", check_magnitude);
    else
      strcpy(WL_fields[10], "na");
  } else {
    strcpy(WL_fields[9], "na");
    strcpy(WL_fields[10], "na");
  }

  // AIRMASS
  if(airmass < 0.0) {
    strcpy(WL_fields[11], "na");
  } else {
    sprintf(WL_fields[11], "%.2lf", airmass);
  }

  // GROUP
  if(group < 0) {
    strcpy(WL_fields[12], "na");
  } else {
    sprintf(WL_fields[12], "%d", group);
  }

  // CHART
  strcpy(WL_fields[13], chart);

  // NOTES
  if(notes[0] == 0) {
    strcpy(WL_notes, "na");
  } else {
    strcpy(WL_notes, notes);
  }

  *status = 0;			// success
  *word_count = 15;		// fixed value
  return words;
}
  
const char *
GetReportFileHeader(void) {
  return "#TYPE=Extended\n#OBSCODE=MMU\n#SOFTWARE=Munkacsy/3-9-2008\n#DELIM=|\n#DATE=JD\n#OBSTYPE=CCD\n";
}

#ifdef DO_TEST
int main(int argc, char **argv) {
  char buffer[256];
  FILE *f_in = stdin;
  FILE *f_out = stdout;

  while(fgets(buffer, sizeof(buffer), f_in)) {
    int status;
    ReportFileLine rfl(buffer, ',', &status);

    if(status) {
      fprintf(stderr, "Error result during input translation.\n");
    } else {
      char *result = rfl.ToString(&status, '|');
      if(status) {
	fprintf(stderr, "Error result during output translation.\n");
      } else {
	fprintf(f_out, "%s\n", result);
	free(result);
      }
    }
  }
}

#endif
