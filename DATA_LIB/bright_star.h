// This may look like C code, but it is really -*- C++ -*-
/*  bright_star.h -- manage a database of bright stars
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

#ifndef _BRIGHT_STAR_H
#define _BRIGHT_STAR_H

#include <dec_ra.h>

class BrightStarIterator;
class BrightStarList;

class OneBrightStar {
public:
  const char *Name(void) { return OBSName; }
  const DEC_RA &Location(void) { return OBSLocation; }
  double Magnitude(void) { return OBSMagnitude; }

private:
  char *OBSName;
  DEC_RA OBSLocation;
  double OBSMagnitude;

  OneBrightStar *next;

  friend class BrightStarIterator;
  friend class BrightStarList;
};

class BrightStarList {
public:
  BrightStarList(double max_dec,        /* north-est */
		 double min_dec,        /* south-est */
		 double east_ra,        /* biggest */
		 double west_ra,        /* smallest */
		 double max_magnitude,  /* dimmest */
		 double min_magnitude);	/* brightest */

  ~BrightStarList(void);

  int NumberOfStars(void);

private:
  OneBrightStar *head;

  friend class BrightStarIterator;
};

class BrightStarIterator {
public:
  BrightStarIterator(BrightStarList *list) :
          master(list), current(0) {;}

  ~BrightStarIterator(void) { ; }

  OneBrightStar *First(void) {
    current = (master->head ? master->head->next : 0);
    return master->head;
  }
  
  OneBrightStar *Next(void) {
    OneBrightStar *n = current;
    if(current) current = current->next;
    return n;
  }

private:
  BrightStarList *master;
  OneBrightStar  *current;
};

#endif
