/*  sync_session.cc -- Implements mount error data archiving
 *
 *  Copyright (C) 2017 Mark J. Munkacsy

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

#include <errno.h>		// errno
#include <sys/file.h>		// flock()
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>		// strlen()
#include <unistd.h>		// ftruncate()
#include <gsl/gsl_vector.h>
#include <gsl/gsl_matrix.h>
#include <gsl/gsl_permutation.h>
#include <gsl/gsl_linalg.h>
#include "mount_model_int.h"
#include "scope_api.h"
#include "Image.h"		// DateToDirname()
#include "refraction.h"

//****************************************************************
//        SyncSession
//****************************************************************

SyncSession::~SyncSession(void) {
  std::list<SyncPoint *>::iterator it;

  for (it = all_sync_points.begin(); it != all_sync_points.end(); it++) {
    delete *it;
  }
  if (session_filename) free((void *) session_filename);
}

#define BUFLEN 132 // max length of SyncSession line in bytes

SyncSession::SyncSession(const char *filename) {
  FILE *fp = fopen(filename, "r");
  session_filename = strdup(filename);

  if (fp) {
    //********************************
    //        Read SyncSession File
    //********************************
    double session_H0, session_D0; // these are dummy and ignored
    char buffer[BUFLEN];
    if(fgets(buffer, BUFLEN, fp) == 0) {
      fprintf(stderr, "Error reading SyncSession first line\n");
    } else {
      if ((sscanf(buffer, "%lf %lf", &session_H0, &session_D0)) != 2) {
	fprintf(stderr, "Error reading SyncSession first line\n");
      }
    }

    //********************************
    //    Read all sync points
    //********************************
    while(fgets(buffer, BUFLEN, fp)) {
      double buf_jd;
      int west_side;
      char buf_sidereal_time[32];
      double buf_ha, buf_ha_reported, buf_dec, buf_dec_reported;
      if (sscanf(buffer, "%lf %d %lf %lf %lf %lf %s",
		 &buf_jd, &west_side, &buf_ha, &buf_dec,
		 &buf_ha_reported, &buf_dec_reported,
		 buf_sidereal_time) != 7) {
	fprintf(stderr, "Error reading SyncSession sync point line\n");
      } else {
	SyncPoint *sp = new SyncPoint;
	sp->hour_angle_raw = buf_ha_reported;
	sp->declination_raw = buf_dec_reported;
	sp->hour_angle_true = buf_ha;
	sp->declination_true = buf_dec;
	sp->west_side_of_mount = west_side;
	sp->time_of_sync = JULIAN(buf_jd);
	sp->flipped = dec_axis_is_flipped(buf_ha, west_side);
	sp->location_raw = DEC_RA(sp->declination_raw, sp->hour_angle_raw, sp->time_of_sync);
	sp->location_true = DEC_RA(sp->declination_true, sp->hour_angle_true, sp->time_of_sync);
	strcpy(sp->sidereal_time_of_sync, buf_sidereal_time);

	all_sync_points.push_back(sp);
      }
    }
    fclose(fp);
  } else {
    // Make sure that we can create/open this file for writing.
    FILE *fp = fopen(session_filename, "w");
    
    if (!fp) {
      fprintf(stderr, "SaveSyncSession: Unable to create sync_session file.\n");
      fprintf(stderr, "(tried to create %s)\n", session_filename);
      return;
    }
    fclose(fp); // will be reopened when we SaveSession()
  }
}

//****************************************************************
//        SaveSyncSession
//    (Save SyncSession to file in today's directory)
//****************************************************************
void
SyncSession::SaveSession(void) {
  if (!session_filename) {
    fprintf(stderr, "SaveSession(): no filename specified.\n");
  } else {
    FILE *fp = fopen(session_filename, "w");
    
    if (!fp) {
      fprintf(stderr, "SaveSyncSession: Unable to create sync_session file.\n");
      fprintf(stderr, "(tried to create %s)\n", session_filename);
      return;
    }

    //********************************
    //        Write H0, D0
    //********************************
    fprintf(fp, "%lf %lf\n", 0.0, 0.0); // H0,D0 ignored

    //********************************
    //        Write all sync points
    //********************************
    std::list<SyncPoint *>::iterator it;
    for (it = all_sync_points.begin(); it != all_sync_points.end(); it++) {
      fprintf(fp, "%lf %d %.9lf %.9lf %.9lf %.9lf %s\n",
	      (*it)->time_of_sync.day(),
	      ((*it)->west_side_of_mount ? 1 : 0),
	      (*it)->hour_angle_true, (*it)->declination_true,
	      (*it)->hour_angle_raw, (*it)->declination_raw,
	      (*it)->sidereal_time_of_sync);
    }

    // Finish up
    fclose(fp);
  }
}

