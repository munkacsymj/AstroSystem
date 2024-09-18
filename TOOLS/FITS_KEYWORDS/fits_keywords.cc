/*  fits_keywords.cc -- Program to print all keywords (and values) in a
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

int main(int argc, char **argv) {

  // Command line options:
  for(int file_no=1; file_no < argc; file_no++) {
    char *filename = argv[file_no];
    fitsfile *fptr;       /* pointer to the FITS file, defined in fitsio.h */
    int status = 0;

    /* open the file, verify we have read access to the file. */
    if ( fits_open_file(&fptr, filename, READONLY, &status) ) {
      printerror( status );
      goto close_file;
    }
    GoToImageHDU(fptr);

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
	printf("%s: %s\n", filename, record);
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
  }
  return;
}

