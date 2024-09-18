/*  ephemeris.cc -- Program to list time of twilight for today's date
 *
 *  Copyright (C) 2007 Mark J. Munkacsy
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
#include <time.h>
#include <stdio.h>

struct interp_table_entry {
  int month;
  int day;
  int hhmm;
};

interp_table_entry civil_twilight_start_table[] = {
  { 1,  1, 641 },		// 06:41 on 1 Jan
  { 1, 13, 639 },
  { 1, 24, 634 },
  { 1, 31, 628 },
  { 2, 14, 613 },
  { 2, 28, 553 },
  { 3, 14, 531 },
  { 3, 28, 507 },
  { 4, 11, 443 },
  { 4, 25, 421 },
  { 5,  9, 401 },
  { 5, 23, 347 },
  { 6,  6, 338 },
  { 6, 20, 337 },
  { 7,  4, 343 },
  { 7, 18, 354 },
  { 8,  1, 409 },
  { 8, 15, 424 },
  { 8, 29, 440 },
  { 9, 12, 455 },
  { 9, 26, 510 },
  {10, 10, 524 },
  {10, 24, 540 },
  {11,  7, 555 },
  {11, 21, 611 },
  {12,  5, 625 },
  {12, 26, 639 },
  {12, 31, 640 } };

//                      J   F   M   A    M    J    J    A    S    O    N    D
int days_so_far[12] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};

static int year_day(int month, int day) {
  if(month < 1 || month > 12 ||
     day < 1 || day > 31) {
    fprintf(stderr, "year_day: invalid argument: %d/%d\n",
	    month, day);
    return 0;
  }

  return days_so_far[month-1]+day;
}

static double hours(int hhmm) {
  int h = hhmm/100;
  int m = (hhmm % 100);

  return h + (((double)m)/60.0);
}

static int hhmm(double hours) {
  int h = (int) hours;
  int m = (int) (0.5 + 60*(hours - (double)h));
  return 100*h + m;
}

int interpolate(interp_table_entry *table,
		int                 table_length,
		int                 month,
		int                 day) {
  int target_day = year_day(month, day);

  interp_table_entry *t = table;
  interp_table_entry *prev = t;

  while(target_day > year_day(t->month, t->day)) {
    prev = t;
    t++;
  }

  // At this point, t points to the entry after the desired time and
  // prev points to the entry prior to the current time
  int interval_start_day = year_day(prev->month, prev->day);
  int interval_end_day   = year_day(t->month, t->day);
  int interval_size      = interval_end_day - interval_start_day;
  double interval_start_hours = hours(prev->hhmm);
  double interval_end_hours   = hours(t->hhmm);
  
  double value_delta = (interval_end_hours - interval_start_hours);

  double fraction;
  if(interval_size == 0) {
    fraction = 0.0;
  } else {
    fraction = ((double)(target_day - interval_start_day))/
      (double)interval_size;
  }

  return hhmm(interval_start_hours + fraction*value_delta);
}

#define EPHEMERIS_TEST 0
#if EPHEMERIS_TEST
int main(int argc, char **argv) {
  fprintf(stderr, "ephemeris test:\n");

  int ans;
  const int num_entries =
    sizeof(civil_twilight_start_table)/sizeof(interp_table_entry);

  ans = interpolate(civil_twilight_start_table,
		    num_entries,
		    1, 1);
  fprintf(stderr, "Jan 1: %04d (should be 0641)\n", ans);

  ans = interpolate(civil_twilight_start_table,
		    num_entries,
		    12, 31);
  fprintf(stderr, "Dec 31: %04d (should be 0640)\n", ans);

  ans = interpolate(civil_twilight_start_table,
		    num_entries,
		    3, 1);
  fprintf(stderr, "Mar 1: %04d (should be 0552)\n", ans);

  ans = interpolate(civil_twilight_start_table,
		    num_entries,
		    6, 30);
  fprintf(stderr, "Jun 30: %04d (should be 0341)\n", ans);

  ans = interpolate(civil_twilight_start_table,
		    num_entries,
		    8, 29);
  fprintf(stderr, "Aug 29: %04d (should be 0440)\n", ans);

}
#else // not a test
int main(int argc, char **argv) {
  const int num_entries =
    sizeof(civil_twilight_start_table)/sizeof(interp_table_entry);

  time_t now;
  (void) time(&now);		// get current time
  struct tm *time_data = localtime(&now);

  int raw_ans = interpolate(civil_twilight_start_table,
			    num_entries,
			    time_data->tm_mon+1,
			    time_data->tm_mday);
  int hh = raw_ans/100;
  int mm = raw_ans % 100;

  // if daylight savings time, must adjust by 1 hour
  if(time_data->tm_isdst) {
    hh++;
  }

  printf("Twilight at %02d:%02d %s\n",
	 hh, mm, (time_data->tm_isdst ? "(DST)" : ""));
  return 0;
}
#endif



