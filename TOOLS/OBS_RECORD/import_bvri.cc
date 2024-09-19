/*  import_bvri.cc -- Bring BVRI measurements into the obs_record
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
#include <bvri_db.h>
#include <HGSC.h>
#include <ctype.h>		// toupper()

void usage(void) {
  fprintf(stderr, "Usage: import_bvri [-d /home/IMAGES/date] [-a]\n");
  exit(2);
}

bool case_independent_cmp(const char *s1, const char *s2) {
  while (*s1 && *s2) {
    if (*s1 != *s2 &&
	(toupper(*s1) != toupper(*s2))) return false;
    s1++;
    s2++;
  }
  return true;
}
  
//****************************************************************
//        ImportBVRIfile()
// Import one bvri.db file into the observations database
//****************************************************************
void ImportBVRIfile(const char *bvri_filename, ObsRecord *obs) {
  // Open the BVRI database file
  const char *current_target_string = nullptr;
  HGSCList *current_catalog = nullptr;
  unsigned int count = 0;
  BVRI_DB bvri_db(bvri_filename); // open READONLY
  BVRI_REC_list *all_bvri = bvri_db.GetAllRecords();

  BVRI_REC_list::iterator it;
  for (it = all_bvri->begin(); it != all_bvri->end(); it++) {
    BVRI_DB_REC *bvri = (*it);
    if (current_target_string == nullptr or
	strcmp(current_target_string, bvri->DB_fieldname) != 0) {
      current_catalog = new HGSCList(bvri->DB_fieldname);
      current_target_string = bvri->DB_fieldname;
    }
      
    // only process stars which have a starname that matches the
    // fieldname (this means that the star is the primary star for the
    // field).
    HGSC *cat_star = current_catalog->FindByLabel(bvri->DB_starname);
    if (cat_star and cat_star->do_submit) {
      ObsRecord::Observation *observation =
	obs->FindObservation(bvri->DB_starname, bvri->DB_obs_time);
      if (!observation) {
	fprintf(stderr,
		"Warning: couldn't find entry in observations for %s at %.6lf\n",
		bvri->DB_fieldname, bvri->DB_obs_time.day());
	continue;
      }

      count++;
      switch (bvri->DB_AAVSO_filter_letter) {
      case 'B':
	observation->B_mag = bvri->DB_rawmag;
	break;
      case 'V':
	observation->V_mag = bvri->DB_rawmag;
	break;
      case 'R':
	observation->R_mag = bvri->DB_rawmag;
	break;
      case 'I':
	observation->I_mag = bvri->DB_rawmag;
	break;
      default:
	fprintf(stderr, "import_bvri: invalid color for %s: '%c'\n",
		bvri->DB_starname, bvri->DB_AAVSO_filter_letter);
      }
    }
  }
  fprintf(stderr, "import_bvri: imported %d measurements from %s\n",
	  count, bvri_filename);
}

//****************************************************************
//        main()
//****************************************************************
int main(int argc, char **argv) {
  int ch;			// option character
  char bvri_filename[132];

  bvri_filename[0] = 0; // clear the filename

  // Command line options:
  //
  //

  while((ch = getopt(argc, argv, "d:")) != -1) {
    switch(ch) {
    case 'd':
      sprintf(bvri_filename, "%s/bvri.db", optarg);
      break;

    case '?':
    default:
      usage();
    }
  }

  if (bvri_filename[0] == 0) usage();

  // ... and then open the Observations database
  //JULIAN now(time(0));
  //SessionOptions options;
  //options.no_session_file = 1;
  
  //Session session(now, now, "/tmp/session.log", options);
  //Strategy::FindAllStrategies(&session);
  //fprintf(stderr, "Initializing ObsRecord.\n");
  ObsRecord obs;

  ImportBVRIfile(bvri_filename, &obs);

  obs.Save();
}
