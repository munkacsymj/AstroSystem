#include "alignment_stars.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>		// getopt()
#include <string.h>
#include <ctype.h>
#include <alt_az.h>
#include <visibility.h>

AlignmentCSVLine::~AlignmentCSVLine(void) { ; }

AlignmentCSVLine::AlignmentCSVLine(FILE *fp) {
  char input_line[120];

  // fgets will return NULL when the file ends
  if (fgets(input_line, sizeof(input_line), fp) == NULL) {
    is_valid = false;
  } else {
    is_valid = true;

    // Split the input line into words (everything on the stack)
    char *pch;
    int field_num = 0;
    pch = strtok(input_line, "\t");
    while (pch != NULL) {
      fields[field_num++] = pch;
      pch = strtok(NULL, "\t");
    }

    // make sure the line is valid
    if (field_num == 0) {
      is_valid = false;
    } else if (field_num != 6) {
      is_valid = false;
      fprintf(stderr, "Invalid field_num: %d, line = %s\n",
	      field_num, input_line);
    } else {
      // yes, it's valid. Important fields are #1 (name), #4 (RA), #5 (Dec)

      // chop the first word at the first space
      char *s = fields[0];
      while (*s && *s != ' ') s++;
      *s = 0;

      char ra_string[16];
      char dec_string[16];

      // RA format is rigid: 00h00.0m
      char *ra = fields[3];
      if (isdigit(ra[0]) && isdigit(ra[1]) && isdigit(ra[3]) &&
	  isdigit(ra[4]) && isdigit(ra[6]) && ra[2] == 'h' &&
	  ra[5] == '.' && ra[7] == 'm') {
	// good format for RA
	ra_string[0] = ra[0];
	ra_string[1] = ra[1];
	ra_string[2] = ':';
	ra_string[3] = ra[3];
	ra_string[4] = ra[4];
	ra_string[5] = ':';
	int ra_seconds = (ra[6] - '0') * 6;
	ra_string[6] = (ra_seconds/10) + '0';
	ra_string[7] = (ra_seconds % 10) + '0';
	ra_string[8] = 0;
      } else {
	// RA format was bad
	fprintf(stderr, "Bad RA format: %s\n", ra);
	is_valid = false;
      }

      // DEC format is rigid: + 0 0 (302 260) 0 0 ' 0 0 (342 200 235)
      char *dec = fields[4];
      if ((dec[0] == '+' || dec[0] == '-') &&
	  isdigit(dec[1]) && isdigit(dec[2]) &&
	  dec[3] == '\302' && dec[4] == '\260' &&
	  isdigit(dec[5]) && isdigit(dec[6]) &&
	  dec[7] == 0x27 && // quote
	  isdigit(dec[8]) && isdigit(dec[9]) &&
	  dec[10] == '\342' && dec[11] == '\200' && dec[12] == '\235') {
	// good format for DEC
	dec_string[0] = dec[0];
	dec_string[1] = dec[1];
	dec_string[2] = dec[2];
	dec_string[3] = ':';
	dec_string[4] = dec[5];
	dec_string[5] = dec[6];
	dec_string[6] = '.';
	int dec_seconds = (dec[8] - '0')*10 + (dec[9]-'0');
	// to do this right requires allowing 59 seconds to round up
	// to a minute, which means the number of minutes (and,
	// potentially, degrees) might need to be incremented. We
	// cheat. We couldn't do this if we were actually aligning on
	// this number, but the value will only be used to determine
	// visibility. 
	dec_string[7] = (dec_seconds + 3)/6 + '0';
	if (dec_string[7] > '9') dec_string[7] = '9';
	dec_string[8] = 0;
      } else {
	fprintf(stderr, "Bad DEC format: %s\n", dec);
	is_valid = false;
      }

      // grab the name
      int status;
      strcpy(this_star.name, fields[0]);
      this_star.location = DEC_RA(dec_string, ra_string, status);
      if (status != STATUS_OK) {
	fprintf(stderr,
		"Uncertain problem: DEC_RA: %s, %s\n",
		dec_string, ra_string);
	is_valid = false;
      }
    }
  }
}

void usage(void) {
  fprintf(stderr, "Usage: alignment_stars [-t hh:mm]\n");
  exit(2);
}

int main(int argc, char **argv) {
  int option_char;
  char *clock_time = 0;

  while((option_char = getopt(argc, argv, "t:")) > 0) {
    switch (option_char) {
    case 't':
      clock_time = optarg;
      break;
      
    case '?':			// invalid argument
    default:
      usage();
    }
  }

  time_t now;
  (void) time(&now);		// get current time
  JULIAN selected_time(now);
  
  if (clock_time) {
    // user provided us with a specific time; put current time into a
    // string, and then manipulate characters
    char string_area[64];
    struct tm *now_time = localtime(&now);

    if (strlen(clock_time) != 5) usage();

    sprintf(string_area, "%s %d/%d/%d", clock_time,
	    now_time->tm_mon+1, now_time->tm_mday, now_time->tm_year+1900);
    selected_time = JULIAN(string_area);
    if (!selected_time.is_valid()) {
      usage();
    }
  }

  // now read alignment stars, one at a time
  FILE *fp = fopen("/home/mark/ASTRO/REFERENCE/alignment_stars.csv", "r");
  if (!fp) {
    fprintf(stderr, "Unable to open alignment_stars.csv file.\n");
    exit(2);
  }

  do {
    AlignmentCSVLine one_line(fp);
    
    if (!one_line.IsValid()) {
      break;
    }

    AlignmentStar *one_star = one_line.convert();
    ALT_AZ alt_az(one_star->location, selected_time);
    if (IsVisible(alt_az, selected_time)) {
      printf("%s\n", one_star->name);
    }
  } while(1);
  return 0;
}
