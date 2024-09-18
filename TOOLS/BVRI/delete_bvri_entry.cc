/*  delete_bvri_entry.cc -- Delete an image sequence from BVRI database
 *
 *  Copyright (C) 2019 Mark J. Munkacsy
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
#include <time.h>		// ctime()
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>		// for strcat()
#include <stdio.h>
#include <unistd.h> 		// for getopt()
#include <stdlib.h>		// for atof()
#include <assert.h>
#include <gendefs.h>
#include <bvri_db.h>

//****************************************************************
//        Usage:
// delete_bvri_entry -n strategy_target_starname -i bvri.db
//****************************************************************

void usage(void) {
  fprintf(stderr,
	  "usage: delete_bvri_entry -n strategy_target_starname -i bvri.db\n");
  exit(-2);
}

// simplify_path() will create a copy of the pathname pointed at by
// <p> and in the process will turn any consecutive pair of '//' into
// a single '/'
const char *simplify_path(const char *p) {
  char *result = (char *) malloc(strlen(p) + 2);
  const char *s = p;
  char *o = result;
  do {
    if (*s == '/' && *(s+1) == '/') s++;
    *o++ = *s;
  } while(*s++);
  return result;
}

//****************************************************************
//        main()
//****************************************************************
int
main(int argc, char **argv) {
  int ch;			// option character
  BVRI_DB *db = 0;
  const char *target_starname = 0;

  // Command line options:
  // -i bvri.db         Name of the database to be used
  // -n target_starname
  
  while((ch = getopt(argc, argv, "i:n:s:f:")) != -1) {
    switch(ch) {
    case 'n':
      target_starname = optarg;
      break;

    case 'i':
      db = new BVRI_DB(optarg, DBASE_MODE_WRITE);
      if (!db) {
	fprintf(stderr, "update_bvri_db: cannot open database file %s\n", optarg);
	usage();
	/*NOTREACHED*/
      }
      break;

    case '?':
    default:
      usage();
      /*NOTREACHED*/
    }
  }

  if(db == 0 || target_starname == 0 ) {
    usage();
    /*NOTREACHED*/
  }

  fprintf(stderr, "DBASE starts off with %d records.\n",
	  db->NumRecords());
  db->DeleteStarRecords(target_starname);
  fprintf(stderr, "DBASE holds %d records after erase().\n",
	  db->NumRecords());

  return 0; // done
}

