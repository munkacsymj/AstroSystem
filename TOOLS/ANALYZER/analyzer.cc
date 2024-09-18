/*  analyzer.cc -- Provides a dependency tree for Astro_DB.
 *
 *  Copyright (C) 2022 Mark J. Munkacsy

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
#include <stdlib.h>		// strtol()
#include <unistd.h>		// getopt()
#include <iostream>		// cerr

#include <astro_db.h>
#include "dnode.h"

void usage(void) {
  std::cerr << "Usage: analyzer [-f] [-t target] [-p num_threads] -d /home/IMAGES/mm-dd-yyyy\n";
  exit(-2);
}

int main(int argc, char **argv) {
  int ch;			// option character
  const char *target = nullptr;
  const char *root_dir = nullptr;
  int num_threads = 1;
  const char *analysis_technique = "OneComp";
  bool force_update = false;

  // Command line options:
  // -t target
  // -p nn                        Number of concurrent threads to use
  // -d /home/IMAGES/9-2-2022     Root directory
  // -a analysis_technique
  // -f                           Force update even if not (seemingly) needed

  while((ch = getopt(argc, argv, "ft:p:d:")) != -1) {
    switch(ch) {
    case 't':
      target = optarg;
      break;

    case 'p':
      {
	char *endptr = nullptr;
	num_threads = strtol(optarg, &endptr, 10);
	if (*endptr != '\0' || num_threads < 1 || num_threads > 10) {
	  std::cerr << "analyzer: invalid integer for -p <num_threads>\n";
	  num_threads = 1;
	}
      }
      break;

    case 'f':
      force_update = true;
      break;

    case 'a':
      analysis_technique = optarg;
      break;

    case 'd':
      root_dir = optarg;
      break;

    case '?':
    default:
      usage();
   }
  }

  if (root_dir == nullptr) usage();

  AstroDB astro_db(JSON_READWRITE, root_dir);
  DNodeTree dtree(astro_db, analysis_technique);

  if (target) {
    dtree.SatisfyTarget(target, force_update);
  } else {
    dtree.SatisfyTarget("*", force_update);
  }

  return 0;
}

  
