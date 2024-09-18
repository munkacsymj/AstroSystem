/*  alignment_stars.h -- list currently-visible alignment stars for Gemini
 *
 *  Copyright (C) 2014 Mark J. Munkacsy
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
/****************************************************************/
/*      class AlignmentStar					*/
/****************************************************************/

#include <stdio.h>
#include <dec_ra.h>
#define MAX_FIELDS 12		// number of fields in the .CSV file

class AlignmentStar {
 public:
  char name[32];
  DEC_RA location;
  double magnitude;
};

class AlignmentCSVLine {
 public:
  AlignmentCSVLine (FILE *fp);
  ~AlignmentCSVLine(void);

  inline bool IsValid(void) { return is_valid; }

  inline AlignmentStar *convert(void) { return is_valid ? &this_star : NULL; }

 private:
  char *fields[MAX_FIELDS];	// pointers to stack space
  bool is_valid;
  AlignmentStar this_star;
};
