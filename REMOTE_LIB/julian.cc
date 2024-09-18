/*  julian.cc -- Julian day implementation of time
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
#include <time.h>		// to get localtime()
#include <stdlib.h>		// for getenv(), setenv(), unsetenv()
#include <string.h>		// for strdup()
#include <stdio.h>
#include <math.h>		// for M_PI
#include "julian.h"

// Warning: you cannot change timezone during the reading of any one
// input file. This is probably a bug? 
static char timezone_letters[4] = "EDT";

void JULIAN::set_timezone(char *timezonename) {
  if(timezonename == 0 ||
     timezonename[0] == 0 ||
     timezonename[1] == 0 ||
     timezonename[2] == 0 ||
     timezonename[3] != 0) {
    fprintf(stderr, "JULIAN::set_timezone: invalid timezone name: '%s'\n",
	    timezonename ? timezonename : "<nil>");
  } else {
    strcpy(timezone_letters, timezonename);
  }
}

JULIAN::JULIAN(time_t t) {
  // however, julian days start at noon, not at midnight, so we want
  // to make a time of "noon" look like "midnight", so subtract 12 hours.
  time_t this_time = t - (12*3600);

  // Now break that down into a struct tm for the gmt timezone
  struct tm *gmt_pieces = gmtime(&this_time);

  julian_date = ((double)(2450084 + gmt_pieces->tm_yday +
                         (gmt_pieces->tm_year-93)/4L +
                         (gmt_pieces->tm_year-96)*365L)) +
               (gmt_pieces->tm_hour*3600 + gmt_pieces->tm_min*60 +
                gmt_pieces->tm_sec)/(double)(24*3600);
}


// str_to_tm accepts a date/time in the form of
// 21:18[:19[.xxx]] 9/12/96

static int count_char(char c, const char *s) {
  // this function returns the number of characters 'c' contained in
  // string 's' 
  int count = 0;
  while(*s) {
    if(c == *s++) count++;
  }
  return count;
}

static time_t str_to_time_t(const char *string) {
  int colon_count = count_char(':', string);
  int dot_count   = count_char('.', string);
  int slash_count = count_char('/', string);

  int hours, minutes;
  int seconds = 0;
  double fractions = 0.0;
  int month, day, year;
  const char *tzone = timezone_letters;
  
  if((string[11] == 'T' || string[10] == 'T') &&
     colon_count == 2) {
    // using FITS UTC format
    // change '-' to ' ' (turn 2002-02-10T02:52:42 into "2002 02 10T02:52:42")

    char y_string[80];
    char *y = y_string;
    const char *x = string;
    do {
      if (*x != '\'') {
	if(*x == '-') *y = ' ';
	else *y = *x;
	y++;
      }
    } while(*x++);
    
    if(dot_count == 1) {
      if(sscanf(y_string, "%d %d %dT%d:%d:%lf",
		&year, &month, &day, &hours, &minutes, &fractions) != 6) {
	fprintf(stderr, "julian: UTC conversion failed\n");
	return 0;
      }
      seconds = (int) (fractions+0.5);
    } else {
      if(sscanf(y_string+1, "%d %d %dT%d:%d:%d",
		&year, &month, &day, &hours, &minutes, &seconds) != 6) {
	fprintf(stderr, "julian: UTC conversion failed\n");
	return 0;
      }
    }
    year -= 1900;
    tzone = "UTC";
  } else {
    // crude validity checks
    if(slash_count != 2) {
      return 0;
    }

    switch(colon_count*10 + dot_count) {
    case 0:  // 1234 9/8/99
      {
	int hours_and_mins;
	if(sscanf(string, "%d %d/%d/%d",
		  &hours_and_mins, &month, &day, &year) != 4) return 0;
	hours = hours_and_mins/100;
	minutes = hours_and_mins % 100;
      }
      break;
    case 10: // 12:34 9/8/99
      if(sscanf(string, "%d:%d %d/%d/%d",
		&hours, &minutes, &month, &day, &year) != 5) return 0;
      break;
    case 20: // 12:34:18 9/8/99
      if(sscanf(string, "%d:%d:%d %d/%d/%d",
		&hours, &minutes, &seconds, &month, &day, &year) != 6) return 0;
      break;
    case 21: // 12:34:18.9 9/8/99
      if(sscanf(string, "%d:%d:%lf %d/%d/%d",
		&hours, &minutes, &fractions, &month, &day, &year) != 6) return 0;
      seconds = (int) (fractions+0.5);
      break;
    default:
      return 0;
    }
  }
  
  struct tm pieces;
  pieces.tm_sec = seconds;
  pieces.tm_min = minutes;
  pieces.tm_hour = hours;
  pieces.tm_mday = day;
  pieces.tm_mon  = (month-1);
  pieces.tm_year = (year > 1900 ? (year-1900) : year);
  pieces.tm_isdst = -1; // means don't know
  pieces.tm_zone = tzone;
  //  pieces.tm_gmtoff = -3600*4;
  pieces.tm_gmtoff = 0;

  // adjust timezone stuff
  time_t return_value;
  if(strcmp(tzone, "UTC") == 0) {
    char *orig_tz = getenv("TZ");
    if(orig_tz)
      orig_tz = strdup(orig_tz);
    setenv("TZ", "", 1);		// blank string defined as UTC
  
    tzset();			// set to UTC
    return_value =  mktime(&pieces);

    // restore original timezone
    if(orig_tz) {
      setenv("TZ", orig_tz, 1);
      free(orig_tz);
    } else {
      unsetenv("TZ");
    }
    tzset();			// restore it back
  } else {
    // not UTC
    return_value =  mktime(&pieces);
  }
    
  return return_value;
}

JULIAN::JULIAN(const char *string) {
    *this = JULIAN(str_to_time_t(string));
}

#define reference_unix_time_t 1028330419
#define reference_jd          2452489.472442

//static time_t reference_unix = time(0);
//static JULIAN reference_jd   = JULIAN(reference_unix);

time_t JULIAN::to_unix(void) const {
  return (reference_unix_time_t +
        (time_t)((julian_date - reference_jd)*(double)(24*3600)));
}

char * JULIAN::to_string(void) const {
    // "Thu Nov 24 18:22:48 1986\0"
  static char buffer[32];
  time_t t = to_unix();

  strcpy(buffer, ctime(&t));
  buffer[24] = '\0'; // erase final '\n'
  return buffer;
}

// This always returns a 12-character field
char * JULIAN::sprint(int num_digits) const {
  char format[24];
  static char result[13];

  sprintf(format, "%%-12.%dlf", num_digits);
  sprintf(result, format, julian_date);
  return result;
}


double JULIAN::days_since_jan_1(void) const {
  time_t unix_time = to_unix();
  const struct tm *unix_time_info = localtime(&unix_time);

  return (double) unix_time_info->tm_yday;
}

double JULIAN::hours_since_local_midnight(void) const {
  time_t unix_time = to_unix();
  const struct tm *unix_time_info = localtime(&unix_time);

  // we want time since local astronomical midnight, so perform a
  // daylight savings time correction.
  return ((double)(unix_time_info->tm_isdst ? -1 : 0) +
	  unix_time_info->tm_hour) +
	  (unix_time_info->tm_min / 60.0) +
	  (unix_time_info->tm_sec / 3600.0);
}

  // meridian() returns the meridian's hour angle (measured in
  // radians) for the current time in the range 0 -> 2*pi

double JULIAN::meridian(void) const {
  double m = 2.0 * M_PI / 24.0 * // kept in radians
    (days_since_jan_1() * (24.0/365.0) +
     // the "6.0hrs + 42min" in the following term establishes the
     // observing location's offset from the prime meridian.
     hours_since_local_midnight() +
     (6.0 + 42.0/60.0));
  if(m > 2.0*M_PI) m -= 2.0*M_PI;
  return m;
}
