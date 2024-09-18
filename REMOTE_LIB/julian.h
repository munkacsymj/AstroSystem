// This may look like C code, but it is really -*- C++ -*-
/*  julian.h -- Julian day implementation of time
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

#ifndef JULIAN_H
#define JULIAN_H
  
#include <time.h>

// Julian Dates: the concept of a julian date is simple: it's a time.
// Several methods are provided to convert between the various
// different ways of representing time.

// There are 4 constructors.  One builds a JULIAN from a string in the
// form of "hh:mm:ss mm/dd/yy".  Another builds a JULIAN from a Unix
// time_t value.  Another creates a "null" date.  The last creates a
// JULIAN from a double holding a julian day value.

class JULIAN {
 public:
  // constructors

  // 21:18[:19[.xxx]] 9/12/96 or in FITS format (2005-09-25T06:34:34)
  JULIAN(const char *string);
  JULIAN(time_t t);
  JULIAN(void) { julian_date = 0.0; }
  JULIAN(double jd) { julian_date = jd; }

  // For performing comparisons between JULIAN dates, we support <
  // (checking to see if one JULIAN is before the other), "-" (to
  // compute the difference (in days) between two JULIANs, and
  // "add_days" to take a JULIAN and advance it by a "delta" measured
  // in days.

  // operators "<", "-", "+", and "add_days"

friend int operator <(const JULIAN &x1, const JULIAN &x2);
friend int operator <=(const JULIAN &x1, const JULIAN &x2);
friend int operator >(const JULIAN &x1, const JULIAN &x2);
friend int operator >=(const JULIAN &x1, const JULIAN &x2);
friend double operator -(const JULIAN &x1, const JULIAN &x2) {
  return x1.julian_date - x2.julian_date; }
friend JULIAN operator +(const JULIAN &x1, double x2) {
  return JULIAN(x1.day() + x2); }

JULIAN add_days(double delta) const { return JULIAN(julian_date + delta); }

  // gets
  double day(void) const { return julian_date; }

  // to_unix() will turn a JULIAN into a time_t.
  time_t to_unix(void) const ;
  char   *to_string(void) const ; // "Thu Nov 24 18:22:48 1986\0"

  // When we are asked to construct a JULIAN and we are left with an
  // invalid JULIAN (bad initialization string?), we can use
  // is_valid() to find out if the initialization was successful.
  int is_valid(void) const { return (julian_date != 0.0); }

  // We need to know our timezone because JULIAN is
  // timezone-independent, but an initialization string like "12:22
  // 3/15/97" has different meanings in different timezones.
  static void set_timezone(char *timezonename);

  friend inline double days_between(JULIAN j1, JULIAN j2);

  double days_since_jan_1(void) const ;
  double hours_since_local_midnight(void) const ;

  // meridian() returns the meridian's hour angle (measured in
  // radians) for the current time in the range 0 -> 2*pi

  double meridian(void) const;

  // This always returns a 12-character field.
  // It is used to turn a JULIAN into a string to be included in an
  // observation report.  Depending upon the type of variable star
  // (e.g., mira vs. CV), the julian day is reported to different
  // levels of precision (specified by "num_digits").
  char *sprint (int num_digits) const ;

 private:
  double julian_date;
};

inline double days_between(JULIAN j1, JULIAN j2) {
  return j1.julian_date - j2.julian_date;
}

inline int operator <(const JULIAN &x1, const JULIAN &x2) {
  return x1.julian_date < x2.julian_date;
}
inline int operator >(const JULIAN &x1, const JULIAN &x2) {
  return x1.julian_date > x2.julian_date;
}
inline int operator <=(const JULIAN &x1, const JULIAN &x2) {
  return x1.julian_date <= x2.julian_date;
}
inline int operator >=(const JULIAN &x1, const JULIAN &x2) {
  return x1.julian_date >= x2.julian_date;
}

#endif
