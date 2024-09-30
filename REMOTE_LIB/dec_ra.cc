/*  dec_ra.cc -- Declination/RightAscension coordinate system
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
#include <ctype.h>
#include <math.h>
#include "dec_ra.h"

static EPOCH J2000_ref(2000);
static JULIAN J2000_julian(2451545.0);

EPOCH::EPOCH(JULIAN when) {
  epoch_ref = J2000_ref.epoch_ref + days_between(when, J2000_julian)/365.25;
}

double
EPOCH::YearsAfter2000(void) {
  return epoch_ref - J2000_ref.epoch_ref;
}

EPOCH EpochOfToday(void) {
  time_t now_t = time(0);
  struct tm *t = localtime(&now_t);
  return EPOCH(t->tm_year + 1900);
}
  
// converting across Epochs; dec_ra is assumed to be in EPOCH "from"
// and we will return a DEC_RA in EPOCH "to" 
DEC_RA
ToEpoch(DEC_RA &dec_ra, EPOCH from, EPOCH to) {
  const double average_epoch_yrs =
    2000.0 + (from.YearsAfter2000()+to.YearsAfter2000())/2.0;
  const double centuries_after_1900 = (average_epoch_yrs - 1900.0)/100.0;
  const double delta_years = to.YearsAfter2000()-from.YearsAfter2000();

  double del_ra = 0.0;
  double del_dec = 0.0;

  double adj_ra, adj_dec;

  const double M = (M_PI/(12.0*3600.0)) *
    (3.07234 + 0.00186 * centuries_after_1900)*delta_years;
  const double N = (M_PI/(180.0*3600.0)) *
    (20.0468 - 0.0085 * centuries_after_1900)*delta_years;
  int cycles = 20;
  do {
    double composite_dec_rads = dec_ra.dec() + del_dec/2.0;
    double composite_ra_rads = dec_ra.ra_radians() + del_ra/2.0;

    double d_ra = M + N*sin(composite_ra_rads)*tan(composite_dec_rads);
    double d_dec = N*cos(composite_ra_rads);

    adj_ra = d_ra - del_ra;
    adj_dec = d_dec - del_dec;

    del_ra = d_ra;
    del_dec = d_dec;
  } while(cycles-- &&
	  (fabs(adj_ra) > (M_PI/180.0)*(0.1/3600.0) ||
	   fabs(adj_dec) > (M_PI/180.0)*(0.1/3600.0)));
  // fprintf(stderr, "ToEpoch took %d cycles\n", (20 - cycles));

  const double new_dec = dec_ra.dec()+del_dec;
  double new_ra = dec_ra.ra_radians()+del_ra;
  DEC_RA answer(new_dec, new_ra);
  answer.normalize();
  return answer;
}

// Return a string holding the declination of a DEC_RA.  The string
// will be in the form of "-01:12.7".  The pointer that is returned
// points to a single string buffer declared "static" here; if you
// call "string_dec_of" more than once, make sure you're finished with
// the first return value before calling it the second time.

char * DEC_RA::string_dec_of(void) const {
  static char buffer[32];
  double angle_abs = dec();
  const int negative = (angle_abs < 0.0);

  if(negative) angle_abs = -angle_abs;
  double degrees = angle_abs / DEGREES;
  
  int deg = (int) degrees;
  const int min_times_10 = (int) (0.5 + ((degrees - (double) deg) * 600.0));
  const int min = min_times_10/10;
  const int sec = (min_times_10 % 10);

  sprintf(buffer, "%s%02d:%02d.%01d", (negative ? "-" : ""), deg, min, sec);
  return buffer;
}

// Return a string holding the right ascension of a DEC_RA.  It's in
// the "normal" form of something like "03:14:23".  I've never needed
// fractions of a second.

char * DEC_RA::string_ra_of(void) const {
  static char buffer[32];
  const double hours = ra();
  
  const long nearest_second = (long) (0.5 + hours * 3600.0);

  const int hr = nearest_second/3600;
  const int min = (nearest_second/60 - hr*60);
  const int sec = nearest_second % 60;

  sprintf(buffer, "%02d:%02d:%02d", hr, min, sec);
  return buffer;
}

char *
DEC_RA::string_longra_of(void) const { // prints a decimal point for seconds
  static char buffer[36];
  const double hours = ra();
  
  const long nearest_dsecond = (long) (0.05 + hours * 36000.0);

  const int hr = nearest_dsecond/36000;
  const int min = (nearest_dsecond/600 - hr*60);
  const int dsec = nearest_dsecond % 600;

  sprintf(buffer, "%02d:%02d:%04.1lf", hr, min, (dsec/10.0));
  return buffer;
}

char * DEC_RA::string_longdec_of(void) const {
  static char buffer[32];
  double angle_abs = dec();
  const int negative = (angle_abs < 0.0);

  if(negative) angle_abs = -angle_abs;
  double degrees = angle_abs / DEGREES;
  
  int deg = (int) degrees;
  int min = (int) ((degrees - (double) deg) * 60.0);
  double sec = (degrees - (deg + min/60.0))*3600.0;

  if ((int)(sec*100.0+0.5) >= 6000) {
    min++;
    sec = 0.0;
  }
  if (min >= 60) {
    deg++;
    min = 0;
  }
  
  sprintf(buffer, "%s%02d%c%02d:%05.2lf", (negative ? "-" : ""),
	  deg, 0337, min, sec);
  return buffer;
}

void
DEC_RA::normalize(void) {
  if (dr_dec > M_PI/2.0) {
    dr_dec = M_PI - dr_dec;
    dr_ra += 12.0;
  }
  if (dr_dec < -M_PI/2.0) {
    dr_dec = (-M_PI) - dr_dec;
    dr_ra += 12.0;
  }
  if (dr_ra < 0.0) dr_ra += 24.0;
  if (dr_ra > 24.0) dr_ra -= 24.0;
}

char * DEC_RA::string_fulldec_of(void) const {
  static char buffer[32];
  double angle_abs = dec();
  const int negative = (angle_abs < 0.0);

  if(negative) angle_abs = -angle_abs;
  double degrees = angle_abs / DEGREES;
  
  int deg = (int) degrees;
  int min = (int) ((degrees - (double) deg) * 60.0);
  double sec = (degrees - (deg + min/60.0))*3600.0;

  if ((int)(sec*100.0+0.5) >= 6000) {
    min++;
    sec = 0.0;
  }
  if (min >= 60) {
    deg++;
    min = 0;
  }

  sprintf(buffer, "%s%02d:%02d:%05.2lf", (negative ? "-" : ""),
	  deg, min, sec);
  return buffer;
}

// Here is the constructor.  Create a DEC_RA from two strings.  Store
// STATUS_OK into "status" if the creation is okay.

DEC_RA::DEC_RA(const char *dec_string, const char *ra_string, int &status) {
  const char *s = dec_string;
  int negative_sign = 1;

  // skip leading whitespace in declination string
  while(*s == ' ' || *s == '\t' || *s == '\n') s++;

  if(*s == '+') {
    s++;
  } else if(*s == '-') {
    s++;
    negative_sign = -1;
  }
  
  // validity check
  if((isdigit(*s) &&
      isdigit(*(s+1)) &&
      *(s+2) == ':' &&
      isdigit(*(s+3)) &&
      isdigit(*(s+4)) &&
      (*(s+5) == '\0' || *(s+5) == ' ' || *(s+5) == '\n' ||
       (*(s+5) == '.' &&
	(isdigit(*(s+6)) ||
	 (*(s+6) == '\0' || *(s+6) == ' ' || *(s+6) == '\n'))))) ||
     (isdigit(*s) &&
     *(s+1) == ':' &&
     isdigit(*(s+2)) &&
     isdigit(*(s+3)) &&
     (*(s+4) == '\0' || *(s+4) == ' ' || *(s+4) == '\n' ||
      (*(s+4) == '.' &&
       (isdigit(*(s+5)) ||
	(*(s+5) == '\0' || *(s+5) == ' ' || *(s+5) == '\n')))))) {
    // validity check passed!
    int degrees;
    float mins;
    sscanf(s, "%d:%f", &degrees, &mins);

    dr_dec = ((double) degrees + (mins/60.0)) * DEGREES;
    if(negative_sign < 0) dr_dec = -dr_dec;
    status = STATUS_OK;
  } else {
    // We allow +0:00:00 as well as +00:00:00 and +000:00:00
    if((isdigit(*s) &&
	isdigit(*(s+1)) &&
	*(s+2) == ':' &&
	isdigit (*(s+3)) &&
	isdigit(*(s+4)) &&
	*(s+5) == ':' &&
	isdigit (*(s+6)) &&
	isdigit(*(s+7)) &&
	(*(s+8) == '\0' || *(s+8) == ' ' || *(s+8) == '\n' ||
	 (*(s+8) == '.' &&
	  (isdigit(*(s+9)) ||
	   (*(s+9) == '\0' || *(s+9) == ' ' || *(s+9) == '\n'))))) ||
       (isdigit(*s) &&
	isdigit(*(s+1)) &&
	isdigit(*(s+2)) &&
	*(s+3) == ':' &&
	isdigit (*(s+4)) &&
	isdigit(*(s+5)) &&
	*(s+6) == ':' &&
	isdigit (*(s+7)) &&
	isdigit(*(s+8)) &&
	(*(s+9) == '\0' || *(s+9) == ' ' || *(s+9) == '\n' ||
	 (*(s+9) == '.' &&
	  (isdigit(*(s+10)) ||
	   (*(s+10) == '\0' || *(s+10) == ' ' || *(s+10) == '\n'))))) ||
       (isdigit(*s) &&
	*(s+1) == ':' &&
	isdigit (*(s+2)) &&
	isdigit(*(s+3)) &&
	*(s+4) == ':' &&
	isdigit (*(s+5)) &&
	isdigit(*(s+6)) &&
	(*(s+7) == '\0' || *(s+7) == ' ' || *(s+7) == '\n' ||
	 (*(s+7) == '.' &&
	  (isdigit(*(s+8)) ||
	   (*(s+8) == '\0' || *(s+8) == ' ' || *(s+8) == '\n')))))) {
      int degrees;
      int mins;
      float secs;
      sscanf(s, "%d:%d:%f", &degrees, &mins, &secs);

      dr_dec = DEGREES * ((double) degrees + ((double) mins / 60.0) +
		((double) secs / 3600.0));
      if(negative_sign < 0) dr_dec = -dr_dec;
      status = STATUS_OK;
    } else {
      status = !STATUS_OK; // something was wrong!
    }
  }

  s = ra_string;

  // skip leading whitespace in right ascension string
  while(*s == ' ' || *s == '\t' || *s == '\n') s++;

  // validity check
  if(status == STATUS_OK &&
     isdigit(*s) &&
     isdigit(*(s+1)) &&
     *(s+2) == ':' &&
     isdigit(*(s+3)) &&
     isdigit(*(s+4)) &&
     *(s+5) == ':' &&
     isdigit(*(s+6)) &&
     isdigit(*(s+7)) &&
     (*(s+8) == '\0' || *(s+8) == ' ' || *(s+8) == '.' ||
      *(s+8) == '\t' || *(s+8) == '\n')){

    // validity check passed!
    int hr, min;
    double sec;

    sscanf(s, "%d:%d:%lf", &hr, &min, &sec);
    dr_ra = ((double) hr) +
      ((double) min / 60.0) +
      (sec/3600.0);
    status = STATUS_OK;
  } else {
    status = !STATUS_OK; // something was wrong!
  }
  if(status == STATUS_OK) {
    if(dr_dec > 90.0 || dr_dec < -90.0 ||
       dr_ra > 24.0 || dr_ra < 0.0) status = !STATUS_OK;
  }
}

void DEC_RA::increment(double delta_dec,  // in radians
		       double delta_ra) { // in radians
  // declination is easy
  dr_dec += delta_dec;

  // RA scales by 1/cos(dec)
  double cosine_dec = cos(dr_dec);
  if(cosine_dec == 0.0) {
    fprintf(stderr, "DEC_RA::increment(): singularity at pole.\n");
    return;
  }

  delta_ra *= (1.0 / cosine_dec);

  dr_ra += (delta_ra * 24.0 / (2.0 * M_PI));
}

// returns Sidereal time at 379 Vanderbilt at moment "when" (in hours)
double SiderealTime(const JULIAN when) {
  //fprintf(stderr, "JULIAN(now) = %lf\n", when.day());
  const double site_longitude = 71.2384469;
  // 2455928.0 is JD for 1/1/2000.
  const double del_D = days_between(when, JULIAN(2455928.000000));
  //fprintf(stderr, "del_D = %lf\n", del_D);
  const long double GMST = (18.697374558 + 24.06570982441908 * del_D);
  //fprintf(stderr, "GMST = %Lf\n", GMST);
  const long double GMST_int = floorl(GMST/24.0);
  //fprintf(stderr, "GMST_int = %Lf\n", GMST_int);
  //fprintf(stderr, "day fraction = %Lf\n", (GMST-GMST_int));
  double st = (GMST-GMST_int*24.0) - (site_longitude * (24.0/360.0));
  if (st < 0.0) {
    st += 24.0;
  }
  return st;
}

  // hour angle is in radians, and has been normalized into the range
  // -pi..+pi
double
DEC_RA::hour_angle(const JULIAN when) const {
  double hour_angle = (SiderealTime(when) - ra()) * M_PI/12.0;
  if(hour_angle < -M_PI) hour_angle += 2.0*M_PI;
  if(hour_angle > 2*M_PI) hour_angle -= 2.0*M_PI;

  return hour_angle;
}

DEC_RA::DEC_RA(double dec_in_radians, double ha_in_radians, JULIAN when) {
  dr_dec = dec_in_radians;

  const double adj = SiderealTime(when)*M_PI/12.0 - ha_in_radians;

  dr_ra = adj * (24.0 / (2.0 * M_PI));
  if(dr_ra < 0.0) dr_ra += 24.0;
  if(dr_ra >= 24.0) dr_ra -= 24.0;
}

     
