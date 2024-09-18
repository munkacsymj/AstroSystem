// This may look like C code, but it is really -*- C++ -*-
/*  named_stars.h -- Manage database of named stars and objects
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

#ifndef _NAMED_STARS_H
#define _NAMED_STARS_H

#include <dec_ra.h>

//
// You never "browse" the database of named stars. Instead, you must
// start with the star's name. Use the name in the constructor of a
// NamedStar and then see what IsKnown() returns. The only attribute
// available on a named star is the star's location (J2000).
//
class NamedStar {
public:
  NamedStar(const char *starname);
  NamedStar(const DEC_RA &location);

  int IsKnown(void) { return status==OKAY; }
  const DEC_RA &Location(void) { return location; }
  const char *Name(void) { return name; }

private:
  const static int OKAY=1;
  int status;			// either OKAY or ~OKAY

  DEC_RA location;
  char name[32];
};


#endif
