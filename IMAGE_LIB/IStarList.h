/* This may look like C code, but it is really -*-c++-*- */
/*  IStarList.h -- Manages the list of stars in an image
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
#ifndef _ISTARLIST_H
#define _ISTARLIST_H

#include <stdio.h>
#include <fitsio.h>
#include "dec_ra.h"

// Validity flags:
#define NLLS_FOR_XY       0x01
#define MAG_VALID         0x02
#define BKGD_VALID        0x04
#define COUNTS_VALID      0x08
#define DEC_RA_VALID      0x10
#define CORRELATED        0x20
#define PHOTOMETRY_VALID  0x40
#define SELECTED          0x80
#define ERROR_VALID       0x100

// Info Flags
#define STAR_IS_COMP      0x01
#define STAR_IS_CHECK     0x02
#define STAR_IS_SUBMIT    0x04
#define STAR_IS_INFRAME   0x08 // used in bad_pixel processing

#define STARNAME_LENGTH   32

class IStarList {
public:
  class IStarOneStar {
  public:
    char StarName[STARNAME_LENGTH];

    double weighted_sum_x;
    double weighted_sum_y;
    double nlls_x, nlls_y;
    double nlls_background;
    double nlls_counts;
    double measured_flux;	// from IRAF
    double magnitude_error;	// from IRAF

    int validity_flags;
    int info_flags;

    double magnitude;
    double photometry;		// measured magnitude
    double flux;		// measured flux (PHOTOMETRY_VALID)
    DEC_RA dec_ra;

    int x, y;			// reference x and y location
    double pixel_sum;
    int number_pixels;
    int index_no;
    IStarOneStar *next;

    void AddPixel(double pixel_value,
		  int pixelX,
		  int pixelY);
    double StarCenterX(void);
    double StarCenterY(void);

    int delete_pending;
  };

  // Standard constructor/destructor
  IStarList(void);
  ~IStarList(void);

  double ImageRotationAngle;
  int NumStars;			// number of stars in the list
  int IStarAdd(IStarOneStar *new_one);
  int IStarAdd(double weighted_sum_x,
	       double weighted_sum_y,
	       int x,		// coordinates of ref point (1st point)
	       int y,
	       double pixel_sum, // sum of all pixel values
	       int number_pixels);
  double &IStarWeightedSumX(int index);
  double &IStarWeightedSumY(int index);
  double StarCenterX(int index);
  double StarCenterY(int index);
  int    &IStarX(int index);
  int    &IStarY(int index);
  double &IStarPixelSum(int index);
  int    &IStarNumberPixels(int index);

  // The call to IStarDeleteStar() does not really take effect until
  // a call to IStarExecuteDeletions()
  void   IStarMarkStarForDeletion(int index);
  void   IStarExecuteDeletions(void);

  // Sort the list so that the smallest index (0) is the brightest
  // star and the dimmest has the largest index.
  void   SortByBrightness(void);

  IStarOneStar *FindByIndex(int index);
  IStarOneStar *FindByName(const char *name);

  void PrintStarSummary(FILE *fp);

  void SaveIntoFITSFile(const char *filename, int rewrite_okay=1);
  IStarList(const char *fits_filename);	// initialize from a FITS file

  void InitializeFromFITSFile(fitsfile *fptr);
  IStarList(fitsfile *fptr) { InitializeFromFITSFile(fptr); }

private:

  IStarOneStar *head, *last;
  IStarOneStar **StarArray;

  int StarArrayCorrect;		// if 0, means StarArray doesn't match
				// linked list and needs to be brought
				// up to date
  void FixStarArray(void);
};
#endif

