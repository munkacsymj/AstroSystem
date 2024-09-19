/*  delete_stars.cc -- Delete all the stars in a FITS file
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
#include <stdio.h>
#include <unistd.h> 		// for getopt()
#include <stdlib.h>		// for atof()
#include <fitsio.h>
#include <Image.h>

static void printerror( int status);

int main(int argc, char **argv) {
  int ch;			// option character
  char *image_filename = 0;		// filename of the .fits image file

  // Command line options:
  //
  // -i filename.fits   Image file
  //

  while((ch = getopt(argc, argv, "i:")) != -1) {
    switch(ch) {
    case 'i':			// image filename
      image_filename = optarg;
      break;

    case '?':
    default:
      fprintf(stderr, "usage: %s -i image_filename.fits \n", argv[0]);
      return 2;			// error return
    }
  }

  if(image_filename == 0) {
    fprintf(stderr, "usage: %s -i image_filename.fits \n", argv[0]);
    return 2;			// error return
  }

  fitsfile *fptr;       /* pointer to the FITS file, defined in fitsio.h */
  int status = 0;

  /* open the file, verify we have write access to the file. */
  if ( fits_open_file(&fptr, image_filename, READWRITE, &status) ) {
         printerror( status );
	 return 2;
  }

  int result = GoToStarlistHDU(fptr);

  if (result) {
    if(fits_delete_hdu(fptr, 0, &status)) {
      printerror(status);
    }
  }

  return 0;
}

// print fitsio error messages
static void
printerror( int status) {
    /*****************************************************/
    /* Print out cfitsio error messages and exit program */
    /*****************************************************/

  if (status) {
    fits_report_error(stderr, status); /* print error report */
    
    exit( status );    /* terminate the program, returning error status */
  }
  return;
}

