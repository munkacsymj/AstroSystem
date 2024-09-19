/*  merge_catalogs.cc -- Merges two catalogs into one
 *
 *  Copyright (C) 2007 Mark J. Munkacsy
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
#include <math.h>
#include <stdio.h>
#include <unistd.h> 		// for getopt()
#include <stdlib.h>		// for atof()
#include <string.h>		// strdup()
#include <named_stars.h>
#include "HGSC.h"
#include <fitsio.h>

/****************************************************************/
/*        usage()						*/
/****************************************************************/

void usage(void) {
  fprintf(stderr, "usage: merge_catalogs -w file1 -c file2 -o outfile\n");
  exit(2);
}

/****************************************************************/
/*        main()						*/
/****************************************************************/

int main(int argc, char **argv) {
  int ch;			// option character
  char *widefield_file = 0;
  char *normal_catalog_file = 0;
  char *output_file = 0;

  // Command line options:
  // -w filename     name of widefield catalog file
  // -c filename     name of "normal" catalog file
  // -o output_file  where result is placed
  //

  while((ch = getopt(argc, argv, "w:c:o:")) != -1) {
    switch(ch) {
    case 'w':			// widefield version
      widefield_file = optarg;
      break;

    case 'c':			// normal catalog file
      normal_catalog_file = optarg;
      break;

    case 'o':			// output filename
      output_file = optarg;
      break;

    case '?':
    default:
      usage();
    }
  }

  if(widefield_file == 0 || normal_catalog_file == 0 || output_file == 0) {
    usage();
  }

  FILE *wide_fp = fopen(widefield_file, "r");
  if(!wide_fp) {
    fprintf(stderr, "Cannot open widefield catalog file %s\n", widefield_file);
    usage();
  }
  FILE *normal_fp = fopen(normal_catalog_file, "r");
  if(!normal_fp) {
    fprintf(stderr, "Cannot open catalog file %s\n", normal_catalog_file);
    usage();
  }
  
  HGSCList *WideList = new HGSCList(wide_fp);
  HGSCList *CatList  = new HGSCList(normal_fp);
  HGSCList *AnswerList = new HGSCList();

  HGSCIterator it(*WideList);
  HGSC *star;
  for(star = it.First(); star; star = it.Next()) {
    star->is_widefield = 1;
    HGSC *copy_star = new HGSC;
    *copy_star = *star;
    AnswerList->Add(*copy_star);
  }
  HGSCIterator it_cat(*CatList);
  for(star = it_cat.First(); star; star = it_cat.Next()) {
    HGSC *alt_star;
    for(alt_star = it.First(); alt_star; alt_star = it.Next()) {
      if(star->location.dec() == alt_star->location.dec() &&
	 star->location.ra() == alt_star->location.ra()) break;
    }
    if(alt_star) {
      // is a duplicate
      fprintf(stderr, "%s is a dup.\n", star->label);
    } else {
      // is different
      HGSC *copy_star = new HGSC;
      *copy_star = *star;
      AnswerList->Add(*copy_star);
    }
  }

  AnswerList->Write(output_file);
}
