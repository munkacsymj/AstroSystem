/*  test_obs_record.cc -- Test the obs_record class
 *
 *  Copyright (C) 2017 Mark J. Munkacsy
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
#include <stdio.h>
#include <unistd.h> 		// for getopt()
#include <stdlib.h>		// for atof()
#include <string.h>		// for strcmp()
#include <julian.h>
#include <session.h>
#include <strategy.h>
#include <obs_record.h>

int main(int argc, char **argv) {
  int ch;			// option character

  // Command line options:
  //
  //

  while((ch = getopt(argc, argv, "")) != -1) {
    switch(ch) {
    case 't':
      break;

    case '?':
    default:
      fprintf(stderr, "usage: %s\n", argv[0]);
      return 2;			// error return
    }
  }

  JULIAN now(time(0));
  SessionOptions options;
  options.no_session_file = 1;
  
  Session session(now, now, "/tmp/session.log", options);
  Strategy::FindAllStrategies(&session);
  fprintf(stderr, "Initializing ObsRecord.\n");
  ObsRecord obs;

  const char *starname = "v-aur";
  ObsRecord::Observation *star = obs.LastObservation(starname);
  fprintf(stderr, "%s Observation: 0x%lx\n", starname,
	  (long unsigned int) star);
  if (star) {
    fprintf(stderr, "Last obs at %.6lf\n",
	    star->when.day());
  }

  double V_mag = 13.3;
  fprintf(stderr, "-------------------------\n");
  double B_mag = obs.PredictBrightness(starname, 'B', V_mag);
  fprintf(stderr, "-------------------------\n");
  double R_mag = obs.PredictBrightness(starname, 'R', V_mag);
  fprintf(stderr, "-------------------------\n");
  double I_mag = obs.PredictBrightness(starname, 'I', V_mag);
  fprintf(stderr, "-------------------------\n");

  fprintf(stderr, "Prediction: B = %.3lf, V = %.3lf, R = %.3lf, I = %.3lf\n",
	  B_mag, V_mag, R_mag, I_mag);

  //obs.Save();
}
