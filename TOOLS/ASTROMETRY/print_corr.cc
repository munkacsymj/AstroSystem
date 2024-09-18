/*  print_corr.cc -- Program to print binary "corr" table from astrometry.net
 *  
 *
 *  Copyright (C) 2018 Mark J. Munkacsy
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

// print fitsio error messages
static void printerror( int status);

struct Column {
  const char *col_name;
  bool col_recognized;
  int print_columns;
  const char *print_format;
  int col_required_type;
  int col_number;
  union {
    double *val_double;
    float *val_float;
    int    *val_int;
  } val;
};

Column columns[] = {
  { "field_x", false, 8, "%8.2lf", TDOUBLE }, // pixels
  { "field_y", false, 8, "%8.2lf", TDOUBLE },
  { "field_ra", false, 11, "%11.6lf", TDOUBLE }, // degrees
  { "field_dec", false, 11, "%11.6lf", TDOUBLE }, // degrees
  { "index_x", false, 8, "%8.2lf", TDOUBLE },
  { "index_y", false, 8, "%8.2lf", TDOUBLE },
  { "index_ra", false, 11, "%11.6lf", TDOUBLE },
  { "index_dec", false, 11, "%11.6lf", TDOUBLE },
  { "index_id", false, 5, "%5d", TINT32BIT },
  { "field_id", false, 5, "%5d", TINT32BIT },
  { "match_weight", false, 13, "%13.2lf", TDOUBLE },
  { "FLUX", false, 11, "%11.1lf", TFLOAT },
  { "BACKGROUND", false, 11, "%11.1lf", TFLOAT },
};

#define NUM_COL_DEFINED (sizeof(columns)/sizeof(columns[0]))

//****************************************************************
// GetColumnInfo() will populate the columns[] array
//****************************************************************
void GetColumnInfo(fitsfile *fptr, int num_rows) {
  int num_columns_in_file;
  int status = 0;
  if (fits_get_num_cols(fptr, &num_columns_in_file, &status)) {
    printerror(status);
    return;
  }

  for (unsigned int i=0; i<NUM_COL_DEFINED; i++) {
    columns[i].col_recognized = false; // may be updated later
    if (fits_get_colnum(fptr, CASESEN, (char *) columns[i].col_name,
			&columns[i].col_number, &status)) {
      if (status != COL_NOT_FOUND) {
	fprintf(stderr, "Unexpected error in GetColumnInfo(col=%d)\n",
		i);
      }
      status = 0;
      continue; // go on to the next column
    }

    int actual_col_type;
    columns[i].col_recognized = true;
    if (fits_get_coltype(fptr,
			 columns[i].col_number,
			 &actual_col_type,
			 0, // repeat value
			 0, // width value
			 &status)) {
      printerror(status);
    } else {
      if (actual_col_type != columns[i].col_required_type) {
	fprintf(stderr, "ERROR: data column %s has wrong type.\n",
		columns[i].col_name);
      } else {
	fprintf(stderr, "Column %d (%s): Okay\n",
		columns[i].col_number,
		columns[i].col_name);
	switch(actual_col_type) {
	case TDOUBLE:
	  columns[i].val.val_double = new double[num_rows];
	  break;
	case TINT32BIT:
	  columns[i].val.val_int = new int[num_rows];
	  break;
	case TFLOAT:
	  columns[i].val.val_float = new float[num_rows];
	  break;
	default:
	  fprintf(stderr, "Unexpected column type for %s\n",
		  columns[i].col_name);
	}
      }
    }
  }
}

//****************************************************************
//    ReadColumns()
//    This function fills in the data arrays in the columns[] array.
//****************************************************************
void ReadColumns(fitsfile *fptr, int num_rows) {
  int status = 0;
  int any_null = 0;
  
  for (unsigned int i=0; i<NUM_COL_DEFINED; i++) {
    // skip columns that weren't defined
    if (!columns[i].col_recognized) continue;

    Column *col = &(columns[i]);
    void *data_pointer = 0;
    switch(col->col_required_type) {
    case TDOUBLE:
      data_pointer = col->val.val_double;
      break;
    case TINT32BIT:
      data_pointer = col->val.val_int;
      break;
    case TFLOAT:
      data_pointer = col->val.val_float;
      break;
    }
    if (fits_read_col(fptr,
		      col->col_required_type,
		      col->col_number,
		      1, // first row
		      1, // first element
		      num_rows, // number of elements
		      (void *) 0, // null values
		      data_pointer,
		      &any_null, // anynull
		      &status)) {
      printerror(status);
      status = 0;
    } else {
      fprintf(stderr, "Data read for %s\n",
	      col->col_name);
    }
  }
}

//****************************************************************
//        PrintColumns
//****************************************************************
void PrintColumns(fitsfile *fptr, int num_rows) {
  // print column headers
  for (unsigned int i=0; i<NUM_COL_DEFINED; i++) {
    const Column *col = &(columns[i]);

    // Skip columns that aren't present
    if (!col->col_recognized) continue;

    printf("%*s", col->print_columns, col->col_name);
  }
  printf("\n");

  for (int row = 0; row < num_rows; row++) {
    for (unsigned int i=0; i<NUM_COL_DEFINED; i++) {
      const Column *col = &(columns[i]);

      // Skip columns that aren't present
      if (!col->col_recognized) continue;

      switch(col->col_required_type) {
      case TDOUBLE:
	printf(col->print_format, col->val.val_double[row]);
	break;

      case TFLOAT:
	printf(col->print_format, col->val.val_float[row]);
	break;

      case TINT32BIT:
	printf(col->print_format, col->val.val_int[row]);
	break;
      }
    }
    printf("\n");
  }
}
    
//****************************************************************
//    usage()
//****************************************************************
void usage(void) {
  fprintf(stderr, "usage: print_corr -i corr.fits\n");
  exit(-2);
}

//****************************************************************
//        main()
//****************************************************************
int main(int argc, char **argv) {
  int option_char;
  char *filename = 0;

  while((option_char = getopt(argc, argv, "i:")) > 0) {
    switch (option_char) {
    case 'i':
      filename = optarg;
      break;

    case '?':			// invalid argument
    default:
      fprintf(stderr, "Invalid argument.\n");
      usage();
    }
  }
  
  if (!filename) {
    fprintf(stderr, "Must provide -i filename.fits arguments.\n");
    usage();
  }

  fitsfile *fptr;       /* pointer to the FITS file, defined in fitsio.h */
  int status = 0;

  /* open the file, verify we have read access to the file. */
  if ( fits_open_file(&fptr, filename, READONLY, &status) ) {
    printerror( status );
    goto close_file;
  }

  //
  // Query number of HDUs
  //
  int num_hdu;
  if ( fits_get_num_hdus(fptr, &num_hdu, &status)) {
    printerror( status );
    goto close_file;
  } else {
    printf("%s: Contains %d Header-Data Units\n",
	   filename, num_hdu);
  }

  //
  // Advance to the last HDU
  //
  int hdu_type;
  if ( fits_movabs_hdu(fptr, num_hdu, &hdu_type, &status)) {
    printerror( status );
    goto close_file;
  }
  
  //
  // Query current HDU number
  //
  int hdu_index;
  hdu_index = fits_get_hdu_num(fptr, &hdu_index);
  printf("Looking at HDU #%d\n", hdu_index);

  //
  // Query HDU type
  //
  if ( fits_get_hdu_type(fptr, &hdu_type, &status)) {
    printerror(status);
    goto close_file;
  } else {
    const char *hdu_type_string = "";
    switch (hdu_type) {
    case IMAGE_HDU:
      hdu_type_string = "IMAGE";
      break;

    case ASCII_TBL:
      hdu_type_string = "ASCII TABLE";
      break;

    case BINARY_TBL:
      hdu_type_string = "BINARY TABLE";
      break;

    default:
      hdu_type_string = "<unknown>";
      break;
    }
    printf("Header type = %s\n", hdu_type_string);
  }

  // Figure out how many data rows are present
  long num_rows;
  if (fits_get_num_rows(fptr, &num_rows, &status)) {
    printerror(status);
    goto close_file;
  }

  // Lookup data on all columns to verify types and establish position
  // of each column. 
  GetColumnInfo(fptr, num_rows);
  // Now read the data
  ReadColumns(fptr, num_rows);

  // ... and print results
  PrintColumns(fptr, num_rows);

 close_file:
  if ( fits_close_file(fptr, &status) )
    printerror( status );
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

