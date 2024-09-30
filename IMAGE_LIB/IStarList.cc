/*  IStarList.cc -- Manages the list of stars in an image
 *
 *  Copyright (C) 2007 Mark J. Munkacsy

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
#include "Image.h"
#include "IStarList.h"
#include <stdio.h>
#include <stdlib.h>		// malloc(), free()
#include <string.h>
#include <fitsio.h>

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

int
IStarList::IStarAdd(IStarList::IStarOneStar *new_one) {
  StarArrayCorrect = 0;		// StarArray needs fixing

  if(head == 0) {
    last = head = new_one;
  } else {
    last->next = new_one;
    last = new_one;
  }

  new_one->next = 0;

  // assignment inside calculation of return value
  return((new_one->index_no = NumStars++));
}  

int
IStarList::IStarAdd(double weighted_sum_x,
		    double weighted_sum_y,
		    int x,	// coordinates of center
		    int y,
		    double pixel_sum, // sum of all pixel values
		    int number_pixels) {
  IStarOneStar *newStar = new IStarOneStar;

  if(newStar == 0) {
    perror("Cannot allocate memory for IStarOneStar in IStarList");
    return -1;
  }

  newStar->weighted_sum_x = weighted_sum_x;
  newStar->weighted_sum_y = weighted_sum_y;
  newStar->validity_flags = 0;
  newStar->info_flags     = 0;
  newStar->delete_pending = 0;
  newStar->magnitude      = 0.0;
  newStar->x              = x;
  newStar->y              = y;
  newStar->nlls_x         = x;
  newStar->nlls_y         = y;
  newStar->pixel_sum      = pixel_sum;
  newStar->number_pixels  = number_pixels;

  sprintf(newStar->StarName, "S%03d", NumStars);

  // assignment inside calculation of return value
  return(IStarAdd(newStar));
}

void IStarList::IStarMarkStarForDeletion(int index) {
  if(StarArrayCorrect == 0) FixStarArray();
  StarArray[index]->delete_pending = 1;
}

void
IStarList::IStarExecuteDeletions(void) {
  StarArrayCorrect = 0;

  IStarOneStar **i_star = &head;
  while(*i_star) {
    if((*i_star)->delete_pending) {
      IStarOneStar *to_kill = (*i_star);
      (*i_star) = to_kill->next;
      delete to_kill;
    } else {
      i_star = &((*i_star)->next);
    }
  }
  FixStarArray();
}


double &
IStarList::IStarWeightedSumX(int index) {
  if(StarArrayCorrect == 0) FixStarArray();
  return StarArray[index]->weighted_sum_x;
}

double &
IStarList::IStarWeightedSumY(int index) {
  if(StarArrayCorrect == 0) FixStarArray();
  return StarArray[index]->weighted_sum_y;
}

int &
IStarList::IStarX(int index) {
  if(StarArrayCorrect == 0) FixStarArray();
  return StarArray[index]->x;
}

int &
IStarList::IStarY(int index) {
  if(StarArrayCorrect == 0) FixStarArray();
  return StarArray[index]->y;
}

double &
IStarList::IStarPixelSum(int index) {
  if(StarArrayCorrect == 0) FixStarArray();
  return StarArray[index]->pixel_sum;
}

int    &
IStarList::IStarNumberPixels(int index) {
  if(StarArrayCorrect == 0) FixStarArray();
  return StarArray[index]->number_pixels;
}

void 
IStarList::FixStarArray(void) {
  if(StarArray) {
    free(StarArray);
  }

  StarArray = (IStarOneStar **) malloc(sizeof(IStarOneStar *) * NumStars);
  if(StarArray == 0) {
    perror("Cannot allocate memory for StarArray in IStarList");
    return;
  }

  NumStars = 0;
  for(IStarOneStar *i = head; i; i=i->next) {
    i->index_no = NumStars++;
    StarArray[i->index_no] = i;
  }

  StarArrayCorrect = 1;		// now it's all fixed up
}
  
IStarList::IStarList(void) {
  NumStars = 0;
  head = last = 0;
  StarArray = 0;
  StarArrayCorrect = 0;
  ImageRotationAngle = 0.0;
}

IStarList::~IStarList(void) {
  IStarOneStar *here, *next;

  next = head;
  while(next) {
    here = next;
    next = here->next;

    free(here);
  }

  free(StarArray);
}

void
IStarList::IStarOneStar::AddPixel(double pixel_value,
				  int pixelX,
				  int pixelY) {
  number_pixels++;
  weighted_sum_x += (pixel_value * (pixelX - x));
  weighted_sum_y += (pixel_value * (pixelY - y));
  pixel_sum += pixel_value;
}
  
IStarList::IStarOneStar *
IStarList::FindByIndex(int index) {
  if(!StarArrayCorrect) {
    if(last->index_no == index) return last;

    FixStarArray();
  }
  
  return StarArray[index];
}

void
IStarList::PrintStarSummary(FILE *fp) {
  IStarOneStar *star;

  fprintf(fp, "\n");
  for(int j=0; j<NumStars; j++) {
    star = FindByIndex(j);
    fprintf(fp,
	    "star %d (%s) has nlls_counts %f, pixel_sum %f\n",
	    j, star->StarName, 
	    star->nlls_counts, star->pixel_sum);
  }
  fprintf(fp, "\n");
}  

double
IStarList::IStarOneStar::StarCenterX(void) {
  if(validity_flags & NLLS_FOR_XY) {
    return nlls_x;
  }
  return weighted_sum_x/pixel_sum + x;
}  

double
IStarList::IStarOneStar::StarCenterY(void) {
  if(validity_flags & NLLS_FOR_XY) {
    return nlls_y;
  }
  return weighted_sum_y/pixel_sum + y;
}  

double 
IStarList::StarCenterX(int index) {
  if(StarArrayCorrect == 0) FixStarArray();
  return StarArray[index]->StarCenterX();
}

double 
IStarList::StarCenterY(int index) {
  if(StarArrayCorrect == 0) FixStarArray();
  return StarArray[index]->StarCenterY();
}

/****************************************************************/
/*        IStarList and the FITS table				*/
/****************************************************************/

/* COLUMN DEFINITIONS:

   1. (STARNAME) char[16]: name used in HGSC files
   2. (X)        TDOUBLE: X-coordinate of pixel (measured in pixels with
                          0 on the left edge
   3. (Y)        TDOUBLE: Y-coordinate of pixel (measured in pixels with
                          0 on the top edge
   4. (DEC)      TDOUBLE: Center declination (radians)
   5. (RA)       TDOUBLE: Center Right Ascension (radians)
   6. (MAG)      TDOUBLE: Magnitude
   7. (BKGD)     TDOUBLE: Background counts (after dark subtraction and
                          flat-fielding)
   8. (COUNTS)   TDOUBLE: Intensity counts (after dark subtraction and
                          flat-fielding)
   9. (PHOT)     TDOUBLE: Measured photometric magnitude
  10. (FLAGS)    TLONG:   Validity and Info flags:
                            0x01:   X,Y based on NLLS
			    0x02:   MAG valid
			    0x04:   BKGD valid
			    0x08:   COUNTS valid
			    0x10:   DEC/RA valid
			    0x20:   CORRELATED
			    0x40:   PHOTOMETRY valid
			   0x100:   ERROR_VALID
                           0x1000:   STAR_IS_COMP
                           0x2000:   STAR_IS_CHECK
                           0x4000:   STAR_IS_SUBMIT
  11. (MAG_ERROR) TDOUBLE: Error in photometric magnitude
			  */
#define TABLE_NUMBER_FIELDS 11

static const char *column_names[TABLE_NUMBER_FIELDS] = {
  "STARNAME",
  "X",
  "Y",
  "DEC",
  "RA",
  "MAG",
  "BKGD",
  "COUNTS",
  "PHOT",
  "FLAGS",
  "MAG_ERROR",
};

static const char *column_formats[TABLE_NUMBER_FIELDS] = {
  "A16",
  "D12.4",
  "D12.4",
  "D16.8",
  "D16.9",
  "D9.3",
  "D10.3",
  "D10.3",
  "D15.5",
  "I8",
  "D15.5",
};

static const char *column_units[TABLE_NUMBER_FIELDS] = {
  "",
  "PIXELS",
  "PIXELS",
  "RADIANS",
  "RADIANS",
  "",
  "COUNTS",
  "COUNTS",
  "",
  "",
  "",
};
  

void
IStarList::SaveIntoFITSFile(const char *filename, int rewrite_okay) {
  fitsfile *fptr;       /* pointer to the FITS file, defined in fitsio.h */
  int status = 0;

  /* open the file, verify we have write access to the file. */
  if ( fits_open_file(&fptr, filename, READWRITE, &status) ) {
         printerror( status );
	 return;
  }

  if (not GoToStarlistHDU(fptr)) {
    // failure means no existing starlist HDU
    // Create a HDU for the star table.
    if ( fits_create_tbl(fptr,
			 ASCII_TBL,
			 0,	// initial number of rows
			 TABLE_NUMBER_FIELDS,
			 (char **) column_names,
			 (char **) column_formats,
			 (char **) column_units,
			 0,	// extname
			 &status)) {
      printerror(status);
    }
  } else {

    if(!rewrite_okay) {
      fprintf(stderr, "image file already has star list.\n");
      return;
    }
  }

  // Check number of columns in second header
  int num_columns;
  long num_rows;
  if(fits_get_num_cols(fptr, &num_columns, &status)) {
    printerror(status);
  }
  if(num_columns != TABLE_NUMBER_FIELDS) {
    fprintf(stderr, "IStarList: Wrong # columns (%d vs. %d)\n",
	    num_columns, TABLE_NUMBER_FIELDS);
  } else {
    fits_get_num_rows(fptr, &num_rows, &status);
    if(num_rows > NumStars) {
	// Table is too big.  Need to delete some rows before writing
	// stuff into it.
      fits_delete_rows(fptr, 1, num_rows, &status);
    } else if(num_rows < NumStars) {
      if(fits_insert_rows(fptr, 0, (NumStars - num_rows), &status)) {
	printerror(status);
      }
    }
  }

  // Okay, now we're ready to write stuff into the table
  // Create the column arrays
  char **col_names   = new char * [NumStars];
  double *x_vals     = new double [NumStars];
  double *y_vals     = new double [NumStars];
  float *dec_vals    = new float  [NumStars];
  double *ra_vals    = new double [NumStars];
  double *mag_vals   = new double [NumStars];
  double *phot_vals  = new double [NumStars];
  double *bkgd_vals  = new double [NumStars];
  double *count_vals = new double [NumStars];
  unsigned long   *flag_vals  = new unsigned long   [NumStars];
  double *mag_err_vals = new double [NumStars];

  int i;
  if(StarArrayCorrect == 0) FixStarArray();

  // setup STARNAME
  for(i=0; i<NumStars; i++) {
    col_names[i] = StarArray[i]->StarName;
  }
  if(fits_write_col(fptr, TSTRING, 1, 1, 0, NumStars, col_names, &status)) {
    printerror(status);
  }

  // setup X/Y
  for(i=0; i<NumStars; i++) {
    if(StarArray[i]->validity_flags & NLLS_FOR_XY) {
      x_vals[i] = StarArray[i]->nlls_x;
      y_vals[i] = StarArray[i]->nlls_y;
    } else {
      x_vals[i] = StarArray[i]->StarCenterX();
      y_vals[i] = StarArray[i]->StarCenterY();
    }
  }
  if(fits_write_col(fptr, TDOUBLE, 2, 1, 0, NumStars, x_vals, &status)||
     fits_write_col(fptr, TDOUBLE, 3, 1, 0, NumStars, y_vals, &status)) {
    printerror(status);
  }

  // setup DEC/RA
  status = 0;
  for(i=0; i<NumStars; i++) {
    if(StarArray[i]->validity_flags & DEC_RA_VALID) {
      dec_vals[i] = StarArray[i]->dec_ra.dec();
      ra_vals[i]  = StarArray[i]->dec_ra.ra_radians();
    } else {
      ra_vals[i] = dec_vals[i] = 0.0;
    }
  }
  if(fits_write_col(fptr, TFLOAT, 4, 1, 0, NumStars, dec_vals, &status))
    printerror(status);
  
  if(fits_write_col(fptr, TDOUBLE, 5, 1, 0, NumStars, ra_vals, &status))
    printerror(status);

  // setup MAGNITUDE
  for(i=0; i<NumStars; i++) {
    if(StarArray[i]->validity_flags & MAG_VALID) {
      mag_vals[i] = StarArray[i]->magnitude;
    } else {
      mag_vals[i] = 0.0;
    }
  }
  if(fits_write_col(fptr, TDOUBLE, 6, 1, 0, NumStars, mag_vals, &status)) {
    printerror(status);
  }

  // setup PHOTOMETRY
  for(i=0; i<NumStars; i++) {
    if(StarArray[i]->validity_flags & PHOTOMETRY_VALID) {
      phot_vals[i] = StarArray[i]->photometry;
    } else {
      phot_vals[i] = -99.0;
    }
  }
  if(fits_write_col(fptr, TDOUBLE, 9, 1, 0, NumStars, phot_vals, &status)) {
    printerror(status);
  }

  // setup MAGNITUDE ERRORS
  for(i=0; i<NumStars; i++) {
    if(StarArray[i]->validity_flags & ERROR_VALID) {
      mag_err_vals[i] = StarArray[i]->magnitude_error;
    } else {
      mag_err_vals[i] = -99.0;
    }
  }
  if(fits_write_col(fptr, TDOUBLE, 11, 1, 0, NumStars, mag_err_vals, &status)) {
    printerror(status);
  }

  // setup BACKGROUND COUNTS
  for(i=0; i<NumStars; i++) {
    if(StarArray[i]->validity_flags & BKGD_VALID) {
      bkgd_vals[i] = StarArray[i]->nlls_background;
    } else {
      bkgd_vals[i] = 0.0;
    }
  }
  if(fits_write_col(fptr, TDOUBLE, 7, 1, 0, NumStars, bkgd_vals, &status)) {
    printerror(status);
  }

  // setup COUNTS
  for(i=0; i<NumStars; i++) {
    if(StarArray[i]->validity_flags & COUNTS_VALID) {
      count_vals[i] = StarArray[i]->nlls_counts;
    } else {
      count_vals[i] = 0.0;
    }
  }
  if(fits_write_col(fptr, TDOUBLE, 8, 1, 0, NumStars, count_vals, &status)) {
    printerror(status);
  }

  // setup FLAGS
  for(i=0; i<NumStars; i++) {
    flag_vals[i] = StarArray[i]->validity_flags;
    flag_vals[i] |= (StarArray[i]->info_flags << 12);
  }
  if(fits_write_col(fptr, TLONG, 10, 1, 0, NumStars, flag_vals, &status)) {
    printerror(status);
  }

  delete [] col_names;
  delete [] x_vals;
  delete [] y_vals;
  delete [] dec_vals;
  delete [] ra_vals;
  delete [] mag_vals;
  delete [] phot_vals;
  delete [] bkgd_vals;
  delete [] count_vals;
  delete [] flag_vals;
  delete [] mag_err_vals;
    
  // All done. Close the file.
  if ( fits_close_file(fptr, &status)) {
         printerror( status );
  }
}

// initialize from a FITS file
IStarList::IStarList(const char *fits_filename) {
  fitsfile *fptr;       /* pointer to the FITS file, defined in fitsio.h */
  int status = 0;

  /* open the file, verify we have read access to the file. */
  if ( fits_open_file(&fptr, fits_filename, READONLY, &status) ) {
         printerror( status );
	 return;
  }
  InitializeFromFITSFile(fptr);

  if ( fits_close_file(fptr, &status)) {
         printerror( status );
  }
}  

// initialize from a FITS file
void IStarList::InitializeFromFITSFile(fitsfile *fptr) {
  int status = 0;

  // get the rotation angle
  ImageInfo info(fptr);
  if(info.RotationAngleValid()) {
    ImageRotationAngle = info.GetRotationAngle();
  } else {
    ImageRotationAngle = 0.0;	// default
  }

  // Default initialization (empty star list)
  NumStars = 0;
  head = last = 0;
  StarArray = 0;
  StarArrayCorrect = 0;

  if (not GoToStarlistHDU(fptr)) return;
    
  int num_columns;
  long num_rows;
  if(fits_get_num_cols(fptr, &num_columns, &status)) {
    printerror(status);
  }
  if(num_columns != TABLE_NUMBER_FIELDS) {
    fprintf(stderr, "IStarList: Wrong # columns (%d vs. %d)\n",
	    num_columns, TABLE_NUMBER_FIELDS);
    return;			// return with empty star list
  } else {
    fits_get_num_rows(fptr, &num_rows, &status);
  }

  if (num_rows == 0) return;

  // Create the right number of stars into the list
  int i;
  for(i=0; i<num_rows; i++) {
    IStarAdd(0.0, 0.0, 0, 0, 0.0, 0);
  }
  FixStarArray();

  // Okay, now we're ready to read the stuff
  // Create the column arrays
  char **col_names   = new char * [NumStars];
  double *x_vals     = new double [NumStars];
  double *y_vals     = new double [NumStars];
  double *dec_vals   = new double [NumStars];
  double *ra_vals    = new double [NumStars];
  double *mag_vals   = new double [NumStars];
  double *phot_vals  = new double [NumStars];
  double *bkgd_vals  = new double [NumStars];
  double *count_vals = new double [NumStars];
  long   *flag_vals  = new long   [NumStars];
  double *mag_err_vals = new double [NumStars];

  // provide an area to store the star names.
  char *namepool = new char[NumStars*STARNAME_LENGTH];
  for(i=0; i<num_rows; i++) {
    col_names[i] = namepool + i*STARNAME_LENGTH;
  }

  fits_read_col(fptr, TSTRING, 1, 1, 0, NumStars, 0, col_names, 0, &status);
  fits_read_col(fptr, TDOUBLE, 2, 1, 0, NumStars, 0, x_vals, 0, &status);
  fits_read_col(fptr, TDOUBLE, 3, 1, 0, NumStars, 0, y_vals, 0, &status);
  fits_read_col(fptr, TDOUBLE, 4, 1, 0, NumStars, 0, dec_vals, 0, &status);
  fits_read_col(fptr, TDOUBLE, 5, 1, 0, NumStars, 0, ra_vals, 0, &status);
  fits_read_col(fptr, TDOUBLE, 6, 1, 0, NumStars, 0, mag_vals, 0, &status);
  fits_read_col(fptr, TDOUBLE, 7, 1, 0, NumStars, 0, bkgd_vals, 0, &status);
  fits_read_col(fptr, TDOUBLE, 8, 1, 0, NumStars, 0, count_vals, 0, &status);
  fits_read_col(fptr, TDOUBLE, 9, 1, 0, NumStars, 0, phot_vals, 0, &status);
  fits_read_col(fptr, TLONG,  10, 1, 0, NumStars, 0, flag_vals, 0, &status);
  fits_read_col(fptr, TDOUBLE,11, 1, 0, NumStars, 0, mag_err_vals, 0, &status);

  for(i=0; i<num_rows; i++) {
    strcpy(StarArray[i]->StarName, col_names[i]);
    StarArray[i]->nlls_x = x_vals[i];
    StarArray[i]->nlls_y = y_vals[i];
    if(flag_vals[i] & DEC_RA_VALID) {
      StarArray[i]->dec_ra = DEC_RA(dec_vals[i], ra_vals[i]);
    }
    if(flag_vals[i] & MAG_VALID) {
      StarArray[i]->magnitude = mag_vals[i];
    }
    if(flag_vals[i] & PHOTOMETRY_VALID) {
      StarArray[i]->photometry = phot_vals[i];
    }
    if(flag_vals[i] & BKGD_VALID) {
      StarArray[i]->nlls_background = bkgd_vals[i];
    }
    if(flag_vals[i] & COUNTS_VALID) {
      StarArray[i]->nlls_counts = count_vals[i];
    }
    StarArray[i]->validity_flags = (flag_vals[i] | NLLS_FOR_XY) & (0xfff);
    StarArray[i]->info_flags = (flag_vals[i] >> 12);
    if(flag_vals[i] & ERROR_VALID) {
      StarArray[i]->magnitude_error = mag_err_vals[i];
    }
  }

  delete [] col_names;
  delete [] x_vals;
  delete [] y_vals;
  delete [] dec_vals;
  delete [] ra_vals;
  delete [] mag_vals;
  delete [] phot_vals;
  delete [] bkgd_vals;
  delete [] count_vals;
  delete [] flag_vals;
  delete [] namepool;
  delete [] mag_err_vals;
}

int compare_istars(const void *u1, const void *u2) {
  IStarList::IStarOneStar **s1 = (IStarList::IStarOneStar **) u1;
  IStarList::IStarOneStar **s2 = (IStarList::IStarOneStar **) u2;

  if((*s1)->nlls_counts > (*s2)->nlls_counts) return -1;
  // return 0 if equal, +1 if order to be swapped
  return ((*s1)->nlls_counts < (*s2)->nlls_counts) ? 1 : 0;
}

void
IStarList::SortByBrightness(void) {
  FixStarArray();

  qsort(StarArray,
	NumStars,
	sizeof(IStarList::IStarOneStar *),
	compare_istars);
}

IStarList::IStarOneStar *
IStarList::FindByName(const char *name) {
  IStarOneStar *s = head;
  while(s) {
    if (strcmp(s->StarName, name) == 0) return s;
    s = s->next;
  }
  return nullptr;
}
