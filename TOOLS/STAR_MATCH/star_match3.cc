/*  star_match.cc -- Match stars in an image with a catalog
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
#include <string.h>		// for strcat()
#include <stdio.h>
#include <unistd.h> 		// for getopt()
#include <stdlib.h>		// for atof()
#include <fitsio.h>
#include <Image.h>
#include <nlls_general.h>
#include <named_stars.h>
#include "correlate3.h"
#include "aperture_phot.h"
#include <gendefs.h>

Verbosity verbosity = { .residuals = false,
			.fixups = false,
			.starlists = false,
			.catalog = false,
			.unmatched = true };

char *bias_filename = 0;	// used in high_precision.cc

void RemoveQuotes(const char *source, char *dest);
void bad_refine(void);

bool upside_down = false;

int main(int argc, char **argv) {
  int ch;			// option character
  char *image_filename = 0;		// filename of the .fits image file
  char *starname = 0;
  char *dark_filename = 0;
  char *flatfile_name = 0;
  int  force_rewrite = 0;
  char *param_filename = 0;
  int  use_existing_starlist = 0;
  int  no_shortcuts = 0;	// used with "-b" command line option
  int use_high_precision = 0;
  char *residual_filename = 0;

  // Command line options:
  // -n star_name       Name of region around which image was taken
  // -f                 Force update of image file, overwriting old stuff
  // -e                 Use pre-existing star list already in file
  // -i filename.fits   Image file
  // -d darkfile.fits   Dark image
  // -p filename        Name of file into which parameter data is written
  // -b                 True "best" match; don't quit early even if good match found
  // -h                 High-precision
  // -r filename        Place to put residuals when high-precision is used
  // -w filename        Name of bias file to be used (values in
  // arcsec)
  // -u                 (upside-down): match the image inverted

  while((ch = getopt(argc, argv, "uw:r:ebhfn:i:d:s:p:")) != -1) {
    switch(ch) {
    case 'u':
      upside_down = true;
      break;
      
    case 'b':
      no_shortcuts = 1;
      break;

    case 'w':
      bias_filename = optarg;
      break;

    case 'r':
      residual_filename = optarg;
      use_high_precision = 1; // forces high precision
      break;

    case 'h':			// high precision
      use_high_precision = 1;
      break;

    case 'e':
      use_existing_starlist = 1;
      break;

    case 'p':
      param_filename = strdup(optarg);
      break;

    case 'n':			// name of star
      starname = optarg;
      break;

    case 's':			// flatfile filename
      flatfile_name = optarg;
      break;

    case 'f':
      force_rewrite = 1;
      break;

    case 'i':			// image filename
      image_filename = optarg;
      break;

    case 'd':
      dark_filename = optarg;
      break;

    case '?':
    default:
      fprintf(stderr,
	      "usage: %s [-h] [-f] [-w biasfile] -n starname -i image_filename.fits [-d dark]\n",
	      argv[0]);
      return 2;			// error return
    }
  }

  if (no_shortcuts) {
    fprintf(stderr, "Warning: no_shortcuts ignored. I never take shortcuts.\n");
  }
  if (use_high_precision) {
    fprintf(stderr, "Warning: use_high_precision ignored. I always do that.\n");
  }

  if(image_filename == 0 || starname == 0) {
    fprintf(stderr,
	    "usage: %s [-h] [-f] -n starname -i image_filename.fits [-d dark]\n",
	    argv[0]);
    return 2;			// error return
  }
  
  Image primary_image(image_filename);

  // Subtrack the dark image if one was provided
  if(dark_filename) {
    Image DarkImage(dark_filename);
    primary_image.subtract(&DarkImage);
  }

  // Perform flat-fielding if something was specified
  if(flatfile_name) {
    Image FlatImage(flatfile_name);
    primary_image.scale(&FlatImage);
  }

  IStarList *List;
  if(use_existing_starlist) {
    List = primary_image.GetIStarList();
  } else {
    List = primary_image.RecalculateIStarList();
  }

  if(List->NumStars < 4) {
    fprintf(stderr, "Only %d stars in image. Cannot correlate.\n",
	    List->NumStars);
    exit(-2);
  }
  // Refine the stars and update in the List.
  int i;
  for(i=0; i < List->NumStars; i++) {
    aperture_measure(&primary_image, i, List);
  }
  // List->PrintStarSummary(stderr);

  // Now do the correlation thing

  // Need a nominal Declination/RA in order to perform coordinate
  // system transformations associated with correlation.  Get the
  // DEC/RA of the image from the DEC_NOM and RA_NUM keywords in the
  // FITS header.
  DEC_RA reference_location;
  ImageInfo *info = primary_image.GetImageInfo();
  if(info && info->NominalDecRAValid()) {
    reference_location = *(info->GetNominalDecRA());
  } else {
    // Backup plan: use the position of the named star
    NamedStar ReferenceStar(starname);
    if(!ReferenceStar.IsKnown()) {
      fprintf(stderr, "Don't know of star named '%s'\n", starname);
      exit(2);
    } else {
      reference_location = ReferenceStar.Location();
    }
  }

  const WCS *wcs = 0;
  Context context;
  {
    char HGSCfilename[132];
    sprintf(HGSCfilename, CATALOG_DIR"/%s", starname);
    
    // find 10 brighest stars in our image and set the "is_widefield"
    // flag (which is actually called SELECTED). 
    const int NUM_WIDEFIELD_STARS = 10;
    int star_index = NUM_WIDEFIELD_STARS;
    if(star_index > List->NumStars) star_index = List->NumStars;
      
    // brightest first.  Will find 10 brightest stars and copy them
    // into "wideList".
    List->SortByBrightness();
    if (verbosity.starlists) {
      fprintf(stderr, "Sorted starlist follows---------->\n");
      List->PrintStarSummary(stderr);
    }

    while(star_index-- > 0) {
      List->FindByIndex(star_index)->validity_flags |= SELECTED;
    }

    context.image_filename = image_filename;
    wcs = correlate(primary_image, List, HGSCfilename, &reference_location, param_filename,
		    residual_filename, context);
  }

  if (verbosity.starlists) {
    fprintf(stderr, "\n...final list follows:\n");
    List->PrintStarSummary(stderr);
  }

  // this will reload the image
  if (wcs) {
    ImageInfo info(image_filename);
    info.SetWCS(wcs);
    info.WriteFITS();
  } 
  List->SaveIntoFITSFile(image_filename, force_rewrite);

  if(param_filename) free(param_filename);
}

void RemoveQuotes(const char *source, char *dest) {
  while(*source) {
    if(*source != '\'') *dest++ = *source;
    source++;
  }
  *dest = 0;
}

void bad_refine(void) {
  fprintf(stderr, "Unable to refine correlation.\n");
  exit(-2);
}
