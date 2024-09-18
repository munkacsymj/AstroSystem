// This may look like C code, but it is really -*- C++ -*-
/*  obs_record.h -- maintain database of all observations
 *
 *  Copyright (C) 2017, 2007 Mark J. Munkacsy

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
#include "strategy.h"
#include <list>
#include <julian.h>

// So, what, you might ask, is an ObsRecord? It's a complete
// collection of "Observation"s that have been made on interesting
// stars. Conceptually, there's only one ObsRecord, and it is
// persistent across sessions.  The constructor below recalls the
// persistent set of all Observations that have been given to
// RememberObservation() during all current and previous sessions. For
// any given star, we don't promise that old Observations will be kept
// in the ObsRecord, only that the most recent Observation will be
// there. 

class ObsRecord {

public:
  struct Observation {
    bool empty_record;		// if true, only the comment_field is valid
    JULIAN when;		// valid +/- 1 hour
    Strategy *what;
    char *starname;
    double execution_time;	// time in seconds
    double V_mag;		// test with isnormal()
    double B_mag;		// these are all "sloppy" and should not
    double R_mag;		// be used for analysis. They may or may
    double I_mag;		// not be transformed.
    char *comment_field;

    Observation(void);
    ~Observation(void) {;}
  };

  void RememberObservation(Observation &obs);

  ObsRecord(void);		// initialize from file

  Observation *LastObservation(const char *name);
  Observation *FindObservation(const char *name, JULIAN time_of_obs);

  // PredictBrightness() will return NAN if it can't make a valid
  // prediction. 
  double PredictBrightness(const char *name,
			   char filter_letter,
			   double v_mag);

  void Save(void);		// important to call this if you've
				// changed or added any observations
				// (it's okay to Save() multiple
				// times.) 

private:
  std::list<Observation *> all_obs;
  const char *Obs_Filename;
  struct timespec last_disk_sync;

  void SyncWithDisk(void);
  void ReadEntireFile(void);
};
