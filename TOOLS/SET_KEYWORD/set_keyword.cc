/*  set_keyword.cc -- Program to set keyword/value pairs in .fits files
 *  FITS file
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
#include <string.h>
#include <unistd.h> 		// for getopt()
#include <stdlib.h>		// for atof()
#include <fitsio.h>
#include <Image.h>

// print fitsio error messages
static void printerror( int status);

void usage(void) {
  fprintf(stderr,
	  "set_keyword -i image_filename.fits keyword value keyword value\n");
  exit(2);
}

int main(int argc, char **argv) {
  int ch;			// option character
  char *image_filename = 0;

  // Command line options:
  //
  // -i image.fits     Name of file to get new keywords
  // all other arguments are taken as keywords and values in pairs
  //

  while((ch = getopt(argc, argv, "i:")) != -1) {
    switch(ch) {
    case 'i':
      image_filename = optarg;
      break;

    case '?':
    default:
      usage();
    }
  }
  argc -= optind;
  argv += optind;

  if(image_filename == 0) {
    usage();
  }

  ////////////////////////////////////////////////////////////////
  // create an array of images from the remaining arguments
  ////////////////////////////////////////////////////////////////
  if(argc < 2) {
    fprintf(stderr,
	    "usage: set_keyword: at least 1 keyword and value must be provided\n");
    return 2;
  }

  fitsfile *fptr;       /* pointer to the FITS file, defined in fitsio.h */
  int status = 0;

  /* open the file, verify we have read access to the file. */
  if ( fits_open_file(&fptr, image_filename, READONLY, &status) ) {
    printerror( status );
    goto close_file;
  }
  GoToImageHDU(fptr);

  while(argc > 0) {

    char record[80];
    int num_records;
    if ( fits_get_hdrspace(fptr, &num_records, 0, &status)) {
      printerror( status );
      goto close_file;
    }

    for(int i=1; i<= num_records; i++) {
      if(fits_read_record(fptr, i, record, &status)) {
	printerror( status );
	goto close_file;
      } else {
	printf("%s: %s\n", image_filename, record);
      }
    }

  close_file:
    if ( fits_close_file(fptr, &status) )
      printerror( status );
  }
}

static void printerror( int status)
{
    /*****************************************************/
    /* Print out cfitsio error messages and exit program */
    /*****************************************************/


  if (status) {
    fits_report_error(stderr, status); /* print error report */

    exit( status );    /* terminate the program, returning error status */
  }
  return;
}

