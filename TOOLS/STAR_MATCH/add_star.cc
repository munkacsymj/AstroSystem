/*  add_star.cc -- Program to add a star from an image to a catalog 
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
#include <string.h>
#include <stdio.h>
#include <unistd.h> 		// for getopt()
#include <stdlib.h>		// for atof()
#include <Image.h>
#include <HGSC.h>
#include <IStarList.h>
#include <gendefs.h>

void usage(void) {
      fprintf(stderr,
	      "usage: add_star -n catalogname -i image_filename.fits -t new-name -s starname\n");
      exit(-2);
}

int main(int argc, char **argv) {
  int ch;			// option character
  char *image_filename = 0;	// filename of the .fits image file
  char *catalog_name = 0;
  char *image_starname = 0;	// the name star_match had assigned
  char *tagname = 0;		// the name to be given to the star

  // Command line options:
  // -n catalog_name       Name of region around which image was taken
  // -i imagefile.fits
  // -s image_starname
  // -t "new" starname

  while((ch = getopt(argc, argv, "n:i:s:t:")) != -1) {
    switch(ch) {
    case 'n':			// name of star
      catalog_name = optarg;
      break;

    case 's':			// existing name of star in image
      image_starname = optarg;
      break;

    case 'i':			// image filename
      image_filename = optarg;
      break;

    case 't':			// name to be assigned to the star
      tagname = optarg;
      break;

    case '?':
    default:
      usage();
   }
  }

  // all four command-line arguments are required
  if(image_filename == 0 ||
     catalog_name == 0 ||
     image_starname == 0 ||
     tagname == 0) {
    usage();
  }
  
  IStarList stars(image_filename);
  if(stars.NumStars == 0) {
    fprintf(stderr, "Cannot find any stars in image.\n");
    exit(-2);
  }

  // Refine the stars and update in the List.
  int i;
  IStarList::IStarOneStar *one_star = 0;

  for(i=0; i < stars.NumStars; i++) {
    if(strcmp(image_starname, stars.FindByIndex(i)->StarName) == 0) {
      one_star = stars.FindByIndex(i);
      break;
    }
  }

  if(one_star == 0) {
    fprintf(stderr, "Cannot find %s in image's starlist.\n", image_starname);
    exit(-2);
  }

  char HGSCfilename[132];
  sprintf(HGSCfilename, CATALOG_DIR "/%s", catalog_name);
  FILE *catalog_file = fopen(HGSCfilename, "a");
  if(!catalog_file) {
    perror("add_star: cannot open existing catalog file");
    exit(-2);
  }


  HGSC new_star(one_star->dec_ra.dec(),
		one_star->dec_ra.ra_radians(),
		0.0,		// magnitude unknown
		tagname);

  new_star.AddToFile(catalog_file);

  fclose(catalog_file);
  fprintf(stderr, "%s added.\n", tagname);
}
