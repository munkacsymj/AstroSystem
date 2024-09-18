/* This may look like C code, but it is really -*-c++-*- */
/*  report_file.h -- Manage the extended format AAVSO report
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
#ifndef _REPORT_FILE_H
#define _REPORT_FILE_H

#define MTYPE_ABS    106
#define MTYPE_DIF    107

#define RFL_SKELETON 72		// just a random number, != 0 & -1

class ReportFileLine {
public:
  // status == 0  --> okay,
  // status == -1 --> bad.
  // status == SKELETON --> was a skeleton line
  ReportFileLine(const char *string, char delim, int *status);
  ReportFileLine(int word_count, char **words, int is_skeleton, int *status);
  ReportFileLine(void);
  ~ReportFileLine(void) {;}

  // This function takes the report-file-line and puts it into
  // standard reporting format. Result should be released with free()
  // after use.
  char *ToString(int *status, char delim);

  // The return value and the strings it points to are allocated
  // statically. Don't try to free() anything.
  char **ToWordList(int *word_count, int *status);
  

  int skeleton;			// 0 => not skeleton, otherwise -1
  char report_name[32];
  double jd;			// julian date
  double magnitude;
  double error_estimate;
  char filter[8];
  int  transformed;
  int  mtype;			// either DIF or ABS
  char comp_name[32];
  double comp_magnitude;
  char check_name[32];
  double check_magnitude;
  double airmass;
  int group;			// grouping, usually -1
  char chart[12];
  char notes[64];

private:
  // returns status
  int BuildReportFileLine(int word_count, char **words, int is_skeleton);
  
};

const char *
GetReportFileHeader(void);

#endif
