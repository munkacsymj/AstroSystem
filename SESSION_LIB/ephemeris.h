// This may look like C code, but it is really -*- C++ -*-
/*  ephemeris.h -- determining time of morning twilight
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
#ifndef _EPHEMERIS_H
#define _EPHEMERIS_H

#include <stdio.h>

#if 1
  #define JULIAN int
#else
  #include <julian.h>
#endif

enum Event {
  Civil_Twilight_Start,
};

/****************************************************************/
/*        event_time						*/
/*        (main entry to ephemeris.h)				*/
/****************************************************************/
JULIAN event_time(Event event,
		  JULIAN approx_when);

#endif // not already included
