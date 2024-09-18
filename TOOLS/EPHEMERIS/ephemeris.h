// This may look like C code, but it is really -*- C++ -*-
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
