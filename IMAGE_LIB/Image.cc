/*  Image.cc -- Manage an image
 *
 *  Copyright (C) 2007, 2020 Mark J. Munkacsy

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
#include <math.h>		// define HUGE
#include <cstdint>		// uint32
#include <stdlib.h>		// malloc(), free()
#include <unistd.h>		// unlink()
#include <fitsio.h>
#include <stdio.h>		// fprintf()
#include <time.h>		// time()
#include <sys/types.h>
#include <sys/stat.h>		// for stat()
#include <fcntl.h>
#include <errno.h>		// for errno
#include <string.h>		// for strcat() and strlcpy()
#include <TCS.h>
#include <gendefs.h>
#include "Image.h"
#include <string>
#include <list>
#include <iostream>
using namespace std;

#define Str(x) #x
#define Xstr(x) Str(x)
#define LINENO Xstr(__LINE__)

// print fitsio error messages
static void printerror(const char *message,  int status);

int median_compare_pixels(const void *a, const void *b) {
  double r = (* (const double *) a) - (* (const double *) b);
  if(r == 0) return 0;
  if(r < 0.0) return -1;
  return 1;
}

// FITS Comments
struct FITSComment {
  const char *keyword;
  const char *comment;
} FITSComments[] = {
		    { "FILTER", " Filter used" },
		    { "FOC-BLUR", " [pixel radius] Measure of focus blur" },
		    { "DATE-OBS", " Exposure start time" },
		    { "EXPOSURE", " [sec] shutter open time" },
		    { "EXP_T2", " [sec] time to shutter fully open" },
		    { "EXP_T3", " [sec] time to shutter start shuitting" },
		    { "EXP_T4", " [sec] time to shutter fully shut" },
		    { "CDELT1", " [arcsec/pixel] N/S Plate scale" },
		    { "CDELT2", " [arcsec/pixel] E/W Plate scale" },
		    { "EGAIN", " [e/ADU] camera gain" },
		    { "DEC_NOM", " [dd:mm.mm] Telescope commanded declination" },
		    { "RA_NOM", " [hh:mm:ss.s] Telescope commanded right ascension" },
		    { "FOCUS", " [ticks] net focus offset from reset position" },
		    { "AIRMASS", " [atmospheres] air mass" },
		    { "NORTH-UP", " [bool] camera orientation" },
		    { "ROTATION", " [rad] CW rotation wrt North=up" },
		    { "PURPOSE", " Reason for exposure" },
		    { "HA_NOM", " [rad] commanded hour angle" },
		    { "ELEVATIO", " [rad] Altitude of image center " },
		    { "AZIMUTH", " [rad] Azimuth of image center " },
		    { "OBSERVER", " Name of observer" },
		    { "TAMBIENT", " [deg C] Ambient termperature" },
		    { "TCCD", " [dec C] Detector temperature" },
		    { "SETNUM", " Unique number identifying sequence" },
		    { "SITELON", " [deg] Telescope longitude" },
		    { "SITELAT", " [deg] Telescope latitude" },
		    { "OBJECT", " Name of object being imaged" },
		    { "EQUINOX", " System used for Dec/RA" },
		    { "TELESCOP", " Telescope identifier" },
		    { "INSTRUME", " Camera identifier" },
		    { "CALSTAT", " Calibrations applied (B, D, F, L)" },
		    { "DATAMAX", " [ADU] Highest ADU pixel value not saturated" },
		    { "XBINNING", " Binning factor in the X direction" },
		    { "YBINNING", " Binning factor in the Y direction" },
		    { "BINNING", " Binning factor applied in both the X and Y directions" },
		    { "OFFSET",   " [0..255] Camera-commanded offset" },
		    { "READMODE", " Camera readout mode " },
		    { "FRAMEX", " [pixel] Subframe origin on X axis" },
		    { "FRAMEY", " [pixel] Subframe origin on Y axis" },
		    { "SNSRMODE", " Sensor mode" },
		    { "SNSRGAIN", " Commanded sensor gain setting" },
		    { "CAMGAIN",  " Commanded sensor gain setting" },
		    { "PSF_P1", " [pixel] PSF shape parameter X direction" },
		    { "PSF_P2", " [pixel] PSF shape parameter Y direction" },
		    { "WCSTYPE", " Type of WCS coordinate alignment used" },
		    { "WCSULDEC", " [rad] Declination of upper left corner" },
		    { "WCSURDEC", " [rad] Declination of upper right corner" },
		    { "WCSLLDEC", " [rad] Declination of lower left corner" },
		    { "WCSLRDEC", " [rad] Declination of lower right corner" },
		    { "WCSULRA", " [rad] Right Ascension of upper left corner" },
		    { "WCSURRA", " [rad] Right Ascension of upper right corner" },
		    { "WCSLLRA", " [rad] Right Ascension of lower left corner" },
		    { "WCSLRRA", " [rad] Right Ascension of lower right corner" },
		    { "WCSROT", " [rad] Image rotation angle" },
		    { "WCSDECCTR", " [rad] Declination of image center" },
		    { "WCSRACTR", " [rad] Right Ascension of image center" },
		    { "WCSSCALE", " [arcsec/pixel?] Image x- and y-scale" },
};

const char *CommentForKeyword(const char *keyword) {
  for (unsigned int i=0; i<sizeof(FITSComments)/sizeof(FITSComments[0]); i++) {
    if (strcmp(keyword, FITSComments[i].keyword) == 0) {
      return FITSComments[i].comment;
    }
  }
  return "";
}

void
ImageInfo::SetValue(const string &keyword, const string &value) {
  if (key_values.count(keyword) == 0) {
    const char *c_keyword = keyword.c_str();
    const char *c_comment = CommentForKeyword(c_keyword);
    if (c_comment == nullptr) {
      fprintf(stderr, "ERROR: Image.cc: missing comment for keyword %s\n",
	      c_keyword);
    } else {
      key_comments[keyword] = string(c_comment);
    }
  }
  key_values[keyword] = value;
}

void
ImageInfo::SetComment(const string &keyword, const string &comment) {
  key_comments[keyword] = comment;
}

//
// statistics()
//    1) If statistics are valid, just return a pointer to them,
//    2) otherwise, recalculate the statistics
//
Statistics *
Image::statistics(void) {
  if(!StatisticsValid) {
    UpdateStatistics(AllPixelStatistics, 0 /* WHOLE_FRAME */);
    StatisticsValid = 1;
  }

  return AllPixelStatistics;
}

//
// add()
//    Add two images.  Must be of the same size.
//
void
Image::add(const Image *i) {
  bool do_delete = false;

  ImageInfo *i_info = i->GetImageInfo();
  ImageInfo *info = this->GetImageInfo();

  int this_binning = 1;
  int i_binning = 1;

  if (i_info && i_info->BinningValid()) {
    i_binning = i_info->GetBinning();
  }

  if (info && info->BinningValid()) {
    this_binning = info->GetBinning();
  }
  
  if (i_binning != this_binning) {
    fprintf(stderr, "Image::add() binning mismatch: %d vs %d\n",
	    i_binning, this_binning);
    return; // do nothing because of the mismatch
  }

  if(i->height != height ||
     i->width  != width) {
    // mismatch: still might work if image i is superset of pixels in
    // image "this"
    if (info && info->FrameXYValid() &&
	i_info && i_info->FrameXYValid()) {
      // both images have valid subframe data
      const int this_x0 = info->GetFrameX()/this_binning;
      const int this_y0 = info->GetFrameY()/this_binning;
      const int i_x0 = i_info->GetFrameX()/i_binning;
      const int i_y0 = i_info->GetFrameY()/i_binning;

      if (i_x0 <= this_x0 and
	  i_y0 <= this_y0 and
	  i_x0+i->width >= this_x0+this->width and
	  i_y0+i->height >= this_y0+this->height) {
	i = i->CreateSubImage(this_y0 - i_y0,
			      this_x0 - i_x0,
			      this->height,
			      this->width);
	do_delete = true;
      }
    }


    if(not do_delete) {
      fprintf(stderr, "Image::add() size mismatch: %dx%d + %dx%d\n",
	      width, height, i->width, i->height);
      return; // after doing nothing because of the mismatch
    }
  }

  int row, col;

  for(row = 0; row < height; row++) {
    for(col = 0; col < width; col++) {
      pixel(col, row) += i->pixel(col, row);
    }
  }
  // invalidates any existing statistics
  if (do_delete) delete i;
  StatisticsValid = 0;
}
  
void
Image::subtract(const Image *i) {
  const Image *source = i;
  const Image *binned_image = nullptr;
  const Image *subimage = nullptr;

  ImageInfo *i_info = i->GetImageInfo();
  ImageInfo *info = this->GetImageInfo();

  int this_binning = 1;
  int i_binning = 1;

  if (i_info && i_info->BinningValid()) {
    i_binning = i_info->GetBinning();
  }

  if (info && info->BinningValid()) {
    this_binning = info->GetBinning();
  }
  
  if (i_binning != this_binning) {
    // Can we safely bin image i?
    if (i_binning == 1) {
      binned_image = i->bin(this_binning);
      i_info = binned_image->GetImageInfo();
      i_binning = this_binning;
      source = binned_image;
    } else {
      fprintf(stderr, "Image::subtract() binning mismatch: %d vs %d\n",
	      i_binning, this_binning);
      return; // do nothing because of the mismatch
    }
  }

  if(source->height != height ||
     source->width  != width) {
    bool fixed = false;
    // mismatch: still might work if image i is superset of pixels in
    // image "this"
    if (info && info->FrameXYValid() &&
	i_info && i_info->FrameXYValid()) {
      // both images have valid subframe data
      const int this_x0 = info->GetFrameX()/this_binning;
      const int this_y0 = info->GetFrameY()/this_binning;
      const int i_x0 = i_info->GetFrameX()/i_binning;
      const int i_y0 = i_info->GetFrameY()/i_binning;

      if (i_x0 <= this_x0 and
	  i_y0 <= this_y0 and
	  i_x0+source->width >= this_x0+this->width and
	  i_y0+source->height >= this_y0+this->height) {
	subimage = source->CreateSubImage(this_y0 - i_y0,
					  this_x0 - i_x0,
					  this->height,
					  this->width);
	source = subimage;
	fixed = true;
      }
    }

    if (not fixed) {
      fprintf(stderr, "Image::subtract() size mismatch: %dx%d + %dx%d\n",
	      width, height, i->width, i->height);
      return; // after doing nothing because of the mismatch
    }
  }

  int row, col;

  for(row = 0; row < height; row++) {
    for(col = 0; col < width; col++) {
      pixel(col, row) -= source->pixel(col, row);
    }
  }
  if (binned_image) delete binned_image;
  if (subimage) delete subimage;
  StatisticsValid = 0;
}
  
void
Image::subtractKeepPositive(const Image *i) {
  subtract(i);
  double sum_v = 0.0;
  for (int y=0; y<height; y++) {
    for (int x=0; x<width; x++) {
      sum_v += pixel(x,y);
    }
  }
  sum_v = sum_v/(height*width);
  if (sum_v < 500.0) {
    const double offset = 500.0 - sum_v;
    for (int y=0; y<height; y++) {
      for (int x=0; x<width; x++) {
	sum_v += offset;
      }
    }
  }
}

void
Image::scale(const Image *i) {
  const Image *source = i;
  Image *binned_image = nullptr;
  const Image *subimage = nullptr;

  ImageInfo *i_info = i->GetImageInfo();
  ImageInfo *info = this->GetImageInfo();

  int this_binning = 1;
  int i_binning = 1;

  if (i_info && i_info->BinningValid()) {
    i_binning = i_info->GetBinning();
  }

  if (info && info->BinningValid()) {
    this_binning = info->GetBinning();
  }
  
  if (i_binning != this_binning) {
    // Can we safely bin image i?
    if (i_binning == 1) {
      binned_image = (i->bin(this_binning));
      binned_image->scale(1.0/(this_binning*this_binning));
      i_info = binned_image->GetImageInfo();
      i_binning = this_binning;
      source = binned_image;
      binned_image->WriteFITSFloat("/tmp/binned.fits");
    } else {
      fprintf(stderr, "Image::scale() binning mismatch: %d vs %d\n",
	      i_binning, this_binning);
      return; // do nothing because of the mismatch
    }
  }

  if(source->height != height ||
     source->width  != width) {
    bool fixed = false;
    // mismatch: still might work if image i is superset of pixels in
    // image "this"
    if (info && info->FrameXYValid() &&
	i_info && i_info->FrameXYValid()) {
      // both images have valid subframe data
      const int this_x0 = info->GetFrameX()/this_binning;
      const int this_y0 = info->GetFrameY()/this_binning;
      const int i_x0 = i_info->GetFrameX()/i_binning;
      const int i_y0 = i_info->GetFrameY()/i_binning;

      if (i_x0 <= this_x0 and
	  i_y0 <= this_y0 and
	  i_x0+source->width >= this_x0+this->width and
	  i_y0+source->height >= this_y0+this->height) {
	subimage = source->CreateSubImage(this_y0 - i_y0,
					  this_x0 - i_x0,
					  this->height,
					  this->width);
	source = subimage;
	subimage->WriteFITSFloat("/tmp/subimage.fits");
	fixed = true;
      }
    }

    if (not fixed) {
      fprintf(stderr, "Image::scale() size mismatch: %dx%d + %dx%d\n",
	      width, height, i->width, i->height);
      return; // after doing nothing because of the mismatch
    }
  }

  int row, col;

  for(row = 0; row < height; row++) {
    for(col = 0; col < width; col++) {
      const double z = source->pixel(col, row);
      if(z)
	pixel(col, row) /= z;
    }
  }
  if (binned_image) delete binned_image;
  if (subimage) delete subimage;
  StatisticsValid = 0;
}
  
void
Image::scale(double d) {
  int row, col;

  for(row = 0; row < height; row++) {
    for(col = 0; col < width; col++) {
      pixel(col, row) *= d;
    }
  }
  StatisticsValid = 0;
}
  
void
Image::clip_low(double d) {
  int row, col;

  for(row = 0; row < height; row++) {
    for(col = 0; col < width; col++) {
      if(pixel(col, row) < d)
	pixel(col, row) = d;
    }
  }
  StatisticsValid = 0;
}
  
void
Image::clip_high(double d) {
  int row, col;

  for(row = 0; row < height; row++) {
    for(col = 0; col < width; col++) {
      if(pixel(col, row) > d)
	pixel(col, row) = d;
    }
  }
  StatisticsValid = 0;
}
  
//
// Constructor:
//    Create an empty image of a specified height and width. All image
//    pixels are initialized to zero.
//
Image::Image(int i_height, int i_width) {
  height = i_height;
  width  = i_width;

  i_pixels       = (double *) malloc(sizeof(double) * width * height);
  StatisticsMask = (int *) malloc(sizeof(int) * width * height);

  {
    int row, col;
    for(row=0; row < height; row++) {
      for(col = 0; col < width; col++) {
	pixel(col, row) = 0.0;
      }
    }
  }

  AllPixelStatistics = (Statistics *) malloc(sizeof(Statistics));
  MaskedStatistics   = (Statistics *) malloc(sizeof(Statistics));
  ThisStarList       = 0;
  StatisticsValid    = 0;
  image_info         = nullptr;
  SetImageFormat(USHORT_IMG);
}

Image::Image(const void *fits_file_in_mem, size_t fits_filelength) {
  fitsfile *fptr;       /* pointer to the FITS file, defined in fitsio.h */
  int status = 0;
  size_t filelength = fits_filelength;
  status = 0;

  /* open the file, verify we have read access to the file. */
  if ( fits_open_memfile(&fptr, "", READONLY,
			 (void **) &fits_file_in_mem,
			 &filelength,
			 0, 0, &status) ) {
    printerror("fits_open_memfile, line " LINENO ,  status );
    return;
  }

  InitializeImage(fptr);

  if ( fits_close_file(fptr, &status) )
    printerror("fits_close_file, line " LINENO , status );
}
  
//
// Constructor:
//    Create an image from a FITS file
//
Image::Image(const char *fits_filename) {

  fitsfile *fptr;       /* pointer to the FITS file, defined in fitsio.h */
  int status = 0;

  /* open the file, verify we have read access to the file. */
  if ( fits_open_file(&fptr, fits_filename, READONLY, &status) ) {
    printerror("fits_open_file, line " LINENO ,  status );
    return;
  }

  InitializeImage(fptr);

  if ( fits_close_file(fptr, &status) )
    printerror("fits_close_file: line " LINENO , status );
}
  
//
// Destructor
//
Image::~Image(void) {
  free(i_pixels);
  free(AllPixelStatistics);
  free(MaskedStatistics);
  free(StatisticsMask);
  if(ThisStarList)
    delete ThisStarList;
  if(image_info)
    delete image_info;
}

const char *encode_FITS_filename(const char *path, bool do_compress=true) {
  if (path[strlen(path)-1] == ']' or not do_compress) {
    // already ends with [] coding, so do nothing
    return strdup(path);
  }
  // "[compress]" adds 10 more characters
  char *new_path = (char *) malloc(strlen(path) + 15);
  strcpy(new_path, path);
  strcat(new_path, "[compress]");
  return new_path;
}

void
Image::WriteFITS(const char *filename, bool compress) const {
  fitsfile *fptr;
  long naxes[2] = { width, height };
  char FITSFilename[256];
  int status = 0;

  // unlink the file if it previously existed
  (void) unlink(filename);

  // '!' means overwrite existing file, if it exists
  sprintf(FITSFilename, "!%s%s", encode_FITS_filename(filename, false),
	  (compress ? "[compress]" : ""));
  if(fits_create_file(&fptr,
		      FITSFilename,
		      &status)) {
    printerror("fits_create_file: line " LINENO , status);
    return;
  }

    /* create an image */
  if(fits_create_img(fptr, USHORT_IMG, 2, naxes, &status)) {
    printerror("fits_create_img: line " LINENO , status);
    return;
  }
  if(fits_write_date(fptr, &status)) {
    printerror("fits_write_date: line " LINENO , status);
    return;
  }

  /* now store the image */
  unsigned short int one_row[width];
  double min_value = 65535.0;
  
  for(int row=0; row<height; row++) {
    for(int col=0; col<width; col++) {
      if (pixel(col, row) < min_value) min_value = pixel(col, row);
    }
  }

  double min_value_offset = 0.0;
  if (min_value < 1.0) {
    min_value_offset = (1.0 - min_value);
    //fprintf(stderr, "WriteFITS: adding bias of %.1lf to all pixels.\n",
    //	    min_value_offset);
  }

  for(int row=0; row<height; row++) {
    for(int col=0; col<width; col++) {
      double v = pixel(col, row);
      if (v + min_value_offset > 65535.0) {
	one_row[col] = 65535;
      } else {
	one_row[col] = (unsigned short) (v+min_value_offset);
      }
    }
    fits_write_img_usht(fptr,	      // file
			0,	      // group
			1+ row*width, // first element
			width,	      // number of elements
			one_row,      // points to row
			&status);     // status
  }

  // write amplifying data if available
  if(GetImageInfo()) {
    GetImageInfo()->WriteFITS(fptr);
  }

  if(fits_close_file(fptr, &status)) {
    printerror("fits_close_file: line " LINENO , status);
    return;
  }
}

void
Image::WriteFITS32(const char *filename, bool compress) const {
  fitsfile *fptr;
  long naxes[2] = { width, height };
  char FITSFilename[256];
  int status = 0;

  // unlink the file if it previously existed
  (void) unlink(filename);

  sprintf(FITSFilename, "!%s%s", filename,
	  (compress ? "[compress]" : ""));
  if(fits_create_file(&fptr,
		      FITSFilename,
		      &status)) {
    printerror("fits_create_file: line " LINENO , status);
    return;
  }

    /* create an image */
  if(fits_create_img(fptr, ULONG_IMG, 2, naxes, &status)) {
    printerror("fits_Create_img: line " LINENO , status);
    return;
  }
  if(fits_write_date(fptr, &status)) {
    printerror("fits_write_date: line " LINENO , status);
    return;
  }

  /* now store the image */
  uint32_t *one_row =
    (uint32_t *) malloc(width * sizeof(uint32_t));

  int row, col;
  for(row=0; row<height; row++) {
    for(col=0; col<width; col++) {
      one_row[col] = (uint32_t) (0.5+pixel(col, row));
    }
  
    fits_write_img_uint(fptr,	      // file
			0,	      // group
			1+ row*width, // first element
			width,	      // number of elements
			one_row,      // points to row
			&status);     // status
  }

  free(one_row);

  if(GetImageInfo()) {
    GetImageInfo()->WriteFITS(fptr);
  }
  
  if(fits_close_file(fptr, &status)) {
    printerror("fits_close_file: line " LINENO , status);
    return;
  }
}

void
Image::WriteFITSFloat(const char *filename, bool compress) const {
  fitsfile *fptr;
  long naxes[2] = { width, height };
  char FITSFilename[256];
  int status = 0;

  // unlink the file if it previously existed
  (void) unlink(filename);

  // '!' will result in overwrite of a pre-existing file.
  sprintf(FITSFilename, "!%s%s", filename, (compress ? "[compress]":""));
  if(fits_create_file(&fptr,
		      FITSFilename,
		      &status)) {
    printerror("fits_create_file: line " LINENO , status);
    return;
  }

    /* create an image */
  if(fits_create_img(fptr, FLOAT_IMG, 2, naxes, &status)) {
    printerror("fits_Create_img: line " LINENO , status);
    return;
  }
  if(fits_write_date(fptr, &status)) {
    printerror("fits_write_date: line " LINENO , status);
    return;
  }

  /* now store the image */
  float one_row[width];
  int row, col;
  for(row=0; row<height; row++) {
    for(col=0; col<width; col++) {
      one_row[col] = (float) pixel(col, row);
    }
  
    fits_write_img_flt(fptr,	      // file
		       0,	      // group
		       1+ row*width, // first element
		       width,	      // number of elements
		       one_row,      // points to row
		       &status);     // status
  }

  if(GetImageInfo()) {
    GetImageInfo()->WriteFITS(fptr);
  }
  
  if(fits_close_file(fptr, &status)) {
    printerror("fits_close_file: line " LINENO , status);
    return;
  }
}
  
void
Image::UpdateStatistics(Statistics *stats, int UseMask) {
  double pixel_sum = 0.0;
  double pixel_sq_sum = 0.0;
  // Find the brightest pixel
  double darkest_pixel = HUGE_VAL;
  double brightest_pixel = -HUGE_VAL;
  //int brightest_x, brightest_y; /*NOTUSED*/
  int num_saturated = 0;

  int   pixel_count = 0;

  double data_max = 65530.0;

  ImageInfo *info = this->GetImageInfo();
  if (info and info->DatamaxValid()) {
    data_max = info->GetDatamax();
  }
    
  // Find brightest, dimmest, average, and std dev
  for(int row=0; row<height; row++) {
    for(int col=0; col<width; col++) {
      int index = row*width+col;

      if(UseMask == 0 || StatisticsMask[index] == -1) {
	double OnePixel = pixel(col, row);
	  
	if (OnePixel >= data_max) num_saturated++;

	if(brightest_pixel < OnePixel) {
	  brightest_pixel = OnePixel;
	}
	if(darkest_pixel > OnePixel) {
	  darkest_pixel = OnePixel;
	}
	pixel_sum += OnePixel;
	pixel_count++;
      }
    }
  }

  stats->AveragePixel = (pixel_sum / pixel_count);
  stats->DarkestPixel = darkest_pixel;
  stats->BrightestPixel = brightest_pixel;
  stats->num_saturated_pixels = num_saturated;

  for(int row=0; row<height; row++) {
    for(int col=0; col<width; col++) {
      int index = row*width+col;

      if(UseMask == 0 || StatisticsMask[index] == -1) {
	double OnePixel = pixel(col, row);

	const double del = OnePixel - stats->AveragePixel;
	pixel_sq_sum += (del*del);
      }
    }
  }
  
  stats->StdDev = sqrt((pixel_sq_sum/pixel_count));

  /* fprintf(stderr, "Statistics: brightest (%f) is at (%d, %d)\n",
	  brightest_pixel, brightest_x, brightest_y); */

  // Now find median
  double *pixel_array = (double *) malloc(sizeof(double) * height * width);
  if(!pixel_array) {
    perror("Cannot allocate memory for median in Image.cc");
    return;
  }

  {
    double *p = pixel_array;
    for(int row=0; row<height; row++) {
      for(int col=0; col<width; col++) {
	*p++ = pixel(col, row);
      }
    }
  }

  stats->MedianPixel = *((double *)Median(pixel_array,
					  height*width,
					  sizeof(double),
					  median_compare_pixels));
  free(pixel_array);
}  
  
double
Image::HistogramValue(double fraction) {
  double *pixel_array = (double *) malloc(sizeof(double) * height * width);
  if(!pixel_array) {
    perror("Cannot allocate memory for historgramPoint in Image.cc");
    return -1.0;
  }

  {
    double *p = pixel_array;
    for(int row=0; row<height; row++) {
      for(int col=0; col<width; col++) {
	*p++ = pixel(col, row);
      }
    }
  }

  double answer =  *((double *)HistogramPoint(pixel_array,
					      height*width,
					      sizeof(double),
					      median_compare_pixels,
					      (int) (fraction*height*width)));
  free(pixel_array);
  return answer;
}

static void printerror(const char *message , int status)
{
    /*****************************************************/
    /* Print out cfitsio error messages and exit program */
    /*****************************************************/
  fprintf(stderr, "%s\n", message);

  if (status) {
    fits_report_error(stderr, status); /* print error report */

    exit( status );    /* terminate the program, returning error status */
  }
  return;
}

// invoke "find_stars" to pre-populate the IStarList
void
Image::find_stars(void) {
  char temp_filename[32];
  // create a temp FITS file for the image...
  strcpy(temp_filename, "/tmp/find_starsXXXXXX");
  close(mkstemp(temp_filename));
  WriteFITS(temp_filename);

  // now set up the find_stars command
  char command_buffer[256];
  sprintf(command_buffer, COMMAND_DIR "/find_stars -i %s",
	  temp_filename);
  if (system(command_buffer) == -1) {
    perror("Unable to execute find_stars command");
  } else {
    if (ThisStarList) delete ThisStarList;
    ThisStarList = new IStarList(temp_filename);
  }
  unlink(temp_filename);
}

#define NEWSTARLIST
#ifndef NEWSTARLIST

IStarList *
Image::GetIStarList(void) {
  if(ThisStarList == 0) {
    // Make a star list
    const double STD_DEV_LIMIT = 4.0;// 3-sigma limit to declare a star
    const double STD_DEV_LOWER_THRESHOLD = 3.0;
    int row, col;
    int this_star;

    // calculate statistics
    const Statistics *stat = statistics();

    // create an empty starlist
    ThisStarList = new IStarList();

    // get the image rotation angle, if known (0 if unknown)
    if(image_info &&
       image_info->RotationAngleValid()) {
      ThisStarList->ImageRotationAngle = image_info->GetRotationAngle();
    } else {
      ThisStarList->ImageRotationAngle = 0.0;
    }

    const double background_variance = HistogramValue(0.75) -
      stat->MedianPixel;
    const double detection_threshold = stat->MedianPixel +
      STD_DEV_LIMIT*background_variance;

    for(int j=0; j<(width*height); j++) {
	// clear the mask array
	StatisticsMask[j] = -1;
    }
    
    // scan image until we find a pixel that exceeds the threshold level
    for(row = 1; row < (height-1); row++) {
      for(col = 1; col < (width-1); col++) {
	if(Mask(col,row) == -1 &&
	   pixel(col,row) > detection_threshold) {
	  // star found!
	  // Is it adjacent to existing star?
	  this_star = -1;	// assume new star

	  for(int xx=col-1; xx < col+1; xx++) {
	    for(int yy=row-1; yy < row+1; yy++) {
	      if(Mask(xx,yy) != -1) {
		// Yes: adjacent to existing star
		this_star = Mask(xx,yy);
		goto add_pixels;
	      }
	    }
	  }
	  // Otherwise, not adjacent to existing star; go to
	  // add_pixels with this_star = -1

	   add_pixels:
	  if(this_star == -1) {
	    this_star = ThisStarList->IStarAdd(0.0, // sumx
					       0.0, // sumy
					       col, // x
					       row, // y
					       0.0, // pixel_sum
					       0);  // num_pixels
	  }
	  AddAdjacentPixels(this_star,
			    col,
			    row,
			    stat->AveragePixel +
			    STD_DEV_LOWER_THRESHOLD*stat->StdDev);
	} // end if threshold exceeded
      } // end loop over all col
    } // end loop over all row

    for(int j=0; j< ThisStarList->NumStars; j++) {
      if(ThisStarList->IStarNumberPixels(j) <= 1) {
	ThisStarList->IStarMarkStarForDeletion(j);
      }
    }
    ThisStarList->IStarExecuteDeletions();

    fprintf(stderr, "Get IStarList found %d stars\n", GetIStarList()->NumStars);
  }
  return ThisStarList;
}
	  
void
Image::AddAdjacentPixels(int    star_index,
			 int    col,
			 int    row,
			 double threshold) {
  Mask(col,row) = star_index;
  (ThisStarList->FindByIndex(star_index))->AddPixel(pixel(col,row), col, row);

  int low_x  = col-1;
  int high_x = col+1;
  int low_y  = row-1;
  int high_y = row+1;

  if(low_x < 0) low_x = 0;
  if(high_x >= width) high_x = (width-1);
  if(low_y < 0) low_y = 0;
  if(high_y >= height) high_y = (height-1);

  int xx, yy;

  for(yy = low_y; yy <= high_y; yy++) {
    for(xx = low_x; xx <= high_x; xx++) {
      if(Mask(xx,yy) == -1 && pixel(xx,yy) > threshold) {
	AddAdjacentPixels(star_index, xx, yy, threshold);
      }
    }
  }
}

#else
//NEW STAR LIST ALGORITHM
double psf(int x, int y) {
  // for an offset x,y from the star center, return the relative
  // strength of the psf()
  const double R = 1.3;
  const double Beta = 2.5;

  const double r_sq = x*x + y*y;
  const double fact = 1.0 + (r_sq/(R*R));
  const double ans = 1.0/pow(fact, Beta);
  return ans;
}

struct match_cell {
  int x;
  int y;
  double factor;
};

struct match_pattern {
  int num_cells;
  double overall_factor;
  match_cell *cells;
} pattern;

// size must be odd
int build_pattern (int size) {
  int x, y;
  int half_size = size/2;
  if(half_size*2 + 1 != size) {
    fprintf(stderr, "build_pattern: size (%d) not odd.\n", size);
    return 0;
  }

  pattern.num_cells = size*size;
  pattern.cells = new match_cell[pattern.num_cells];
  pattern.overall_factor = 1.0;

  double cell_sum = 0.0;
  int cell_num = 0;
  for(x= -half_size; x <= half_size; x++) {
    for(y = -half_size; y <= half_size; y++) {
      cell_sum += (pattern.cells[cell_num].factor = psf(x, y));
      pattern.cells[cell_num].x = x;
      pattern.cells[cell_num].y = y;
      cell_num++;
    }
  }
  // we want cell_sum to be zero, so we add an adjustment to all cells
  const double adjustment = cell_sum/pattern.num_cells;
  for(cell_num = 0; cell_num < pattern.num_cells; cell_num++) {
    pattern.cells[cell_num].factor -= adjustment;
  }
  return 1;
}

double apply_pattern(Image *i, int y, int x, match_pattern &p) {
  int cell;
  double sum = 0.0;
  const int width = i->width;
  const int height = i->height;

  for(cell = 0; cell < p.num_cells; cell++) {
    const int cell_x = x+p.cells[cell].x;
    const int cell_y = y+p.cells[cell].y;

    if(cell_x < 0 ||
       cell_y < 0 ||
       cell_x >= width ||
       cell_y >= height) continue;

    sum += i->pixel(cell_x, cell_y)*p.cells[cell].factor;
  }

  return sum*p.overall_factor;
}

IStarList *
Image::PassiveGetIStarList(void) {
  return ThisStarList;
}

IStarList *
Image::RecalculateIStarList(void) {
  if(ThisStarList) {
    delete ThisStarList;
    ThisStarList = 0;
  }
  return GetIStarList();
}

IStarList *
Image::GetIStarList(void) {
  if (ThisStarList && ThisStarList->NumStars == 0) {
    delete ThisStarList;
    ThisStarList = 0;
  }
  
  if(ThisStarList == 0) {
    // Make a star list
    const double STD_DEV_LIMIT = 4.0;// 3-sigma limit to declare a star

    int row, col;
    build_pattern(9);		// +/- 4 cells either side of center

    Image target(height, width);
    for(row = 0; row < height; row++) {
      for(col = 0; col < width; col++) {
	target.pixel(col, row) = pixel(col, row);
	//target.pixel(col, row) = apply_pattern(this, row, col, pattern);
      }
    }

    // calculate statistics
    Statistics *stat = target.statistics();

    // create an empty starlist
    ThisStarList = new IStarList();

    // get the image rotation angle, if known (0 if unknown)
    if(image_info &&
       image_info->RotationAngleValid()) {
      ThisStarList->ImageRotationAngle = image_info->GetRotationAngle();
    } else {
      ThisStarList->ImageRotationAngle = 0.0;
    }

    const double background_variance = stat->StdDev;
    const double detection_threshold = stat->MedianPixel +
      STD_DEV_LIMIT*background_variance;

    // find the star centers
    for(row = 3; row < height-3; row++) {
      for(col = 3; col < width-3; col++) {
	const double pix = target.pixel(col, row);
	if(pix > detection_threshold &&
	   pix >= target.pixel(col-1, row) &&
	   pix >= target.pixel(col+1, row) &&
	   pix >= target.pixel(col-1, row+1) &&
	   pix >= target.pixel(col, row+1) &&
	   pix >= target.pixel(col+1, row+1) &&
	   pix >= target.pixel(col-1, row-1) &&
	   pix >= target.pixel(col, row-1) &&
	   pix >= target.pixel(col+1, row-1)) {
	  // must also have at least 3 pixels above the threshold
	  int beyond_threshold = 0;
	  for (int j=col-1; j < col+2; j++) {
	    for (int k=row-1; k < row+2; k++) {
	      if (detection_threshold < target.pixel(j, k)) {
		beyond_threshold++;
	      }
	    }
	  }
	  if (beyond_threshold >= 3) {
	    // it's a star
	    int new_star = ThisStarList->IStarAdd(0.0, 0.0, col, row, 0.0, 0);
	    IStarList::IStarOneStar *new_s = ThisStarList->FindByIndex(new_star);

	    // for now we use a standard 7-pixel-square box
	    const int box_lim = 3;
	    for(int my = row-box_lim; my <= row+box_lim; my++) {
	      for(int mx = col-box_lim; mx <= col+box_lim; mx++) {
		new_s->AddPixel(pixel(mx, my), mx, my);
	      }
	    }
	  }
	} // end if was a local maximum
      }
    }

    // for(int j=0; j< ThisStarList->NumStars; j++) {
    //   if(ThisStarList->IStarNumberPixels(j) <= 1) {
    //     ThisStarList->IStarMarkStarForDeletion(j);
    //   }
    // }
    // ThisStarList->IStarExecuteDeletions();

    fprintf(stderr, "Get IStarList Found %d stars\n", ThisStarList->NumStars);
  }
  return ThisStarList;
}
	  

#endif // STAR LIST ALGORITHM

void
CompositeImage::CompositeCenter(double *x,
				double *y) {
  double pixel_sum = 0.0;
  double x_sum = 0.0;
  double y_sum = 0.0;

  for(int row = 0; row < height; row++) {
    for(int col = 0; col < width; col++) {
      pixel_sum += pixel(col, row);

      x_sum += col*pixel(col, row);
      y_sum += row*pixel(col, row);
    }
  }

  *x = x_sum/pixel_sum;
  *y = y_sum/pixel_sum;
}

CompositeImage *
BuildComposite(Image **i_array,
	       int     num_images) {
  int j;
  CompositeImage *composite = new CompositeImage(100,100);

  for(j=0; j < num_images; j++) {
    Image *this_image = i_array[j];
    const int Biggest_Star = this_image->LargestStar();

    const double center_x =
      this_image->GetIStarList()->StarCenterX(Biggest_Star);
    const double center_y =
      this_image->GetIStarList()->StarCenterY(Biggest_Star);

    int k_x =  (int)(center_x + 0.5) - composite->COMPOSITE_FACTOR/2;
    int k_y =  (int)(center_y + 0.5) - composite->COMPOSITE_FACTOR/2;

    for(int offsetx=-1; offsetx < composite->COMPOSITE_FACTOR+1; offsetx++) {
      for(int offsety=-1; offsety < composite->COMPOSITE_FACTOR+1; offsety++) {
	composite->AddPixelToComposite(this_image,
				       Biggest_Star,
				       k_x + offsetx - center_x,
				       k_y + offsety - center_y,
				       k_x + offsetx,
				       k_y + offsety);
      }
    }
  }

  for(int row=0; row < composite->height; row++) {
    for(int col=0; col < composite->width; col++) {
      composite->pixel(col, row) /=
	(composite->PixelCountArray[col+row*composite->width]);
    }
  }

  return composite;
}

CompositeImage *
BuildComposite(Image *orig_image,
	       IStarList *starlist,
	       int composite_size) {
  int j;

  CompositeImage *composite = new CompositeImage(composite_size,
						 composite_size);

  for(j=0; j < starlist->NumStars; j++) {
    const double center_x = starlist->StarCenterX(j);
    const double center_y = starlist->StarCenterY(j);

    composite->AddStarToComposite(orig_image, center_x, center_y);
  }
  
  return composite;
}

int
Image::LargestStar(void) {
  if(GetIStarList()->NumStars < 1) {
    fprintf(stderr, "composite_fwhm: no stars found.\n");
    return -1;
  }
  
  int StarIndex = GetIStarList()->NumStars;
  int biggest_star = -1;
  double sizeofbiggeststar = 0.0;
  int j;

  for(j=0; j<StarIndex; j++) {
    double this_star_size = GetIStarList()->IStarPixelSum(j);
    if(this_star_size > sizeofbiggeststar) {
      biggest_star = j;
      sizeofbiggeststar = this_star_size;
    }
  }

  if (biggest_star == -1) {
    sizeofbiggeststar = 99.9; // now will look at magnitude
    for(j=0; j<StarIndex; j++) {
      // if no valid magnitude, skip the star
      if ((GetIStarList()->FindByIndex(j)->validity_flags & MAG_VALID) == 0) continue;
      double this_star_size = GetIStarList()->FindByIndex(j)->magnitude;
      if(this_star_size < sizeofbiggeststar) {
	biggest_star = j;
	sizeofbiggeststar = this_star_size;
      }
    }
  }
    
  return biggest_star;
}  

double
Image::composite_fwhm(void) {
  double b;			// this holds the answer

  // find a "background" level by getting the median of the "border" stars
  int x, y;
  double border_pixels[2*(width+height)];
  for(x=0; x<width; x++) {
    border_pixels[x] = pixel(x, 0);
    border_pixels[x+width] = pixel(x, height-1);
  }
  for(y=0; y<height; y++) {
    border_pixels[width+width+y] = pixel(0, y);
    border_pixels[width+width+height+y] = pixel(width-1, y);
  }
  const double median_pixel = *((double *)Median(border_pixels,
						 2*(width+height),
						 sizeof(double),
						 median_compare_pixels));
  fprintf(stderr, "composite_fwhm: median pixel = %.1f\n",
	  median_pixel);
  
  int x_ref=0;
  int y_ref=0;
  double largest = pixel(0,0);

  for(x=0; x<width; x++) {
    for(y=0; y<height; y++) {
      if(pixel(x,y) > largest) {
	largest = pixel(x,y);
	x_ref = x;
	y_ref = y;
      }
    }
  }
  
  fprintf(stderr, "composite_fwhm: brightest pixel at (%d, %d)\n",
	  x_ref, y_ref);

  double x_tot = 0.0;
  double y_tot = 0.0;
  double a_tot = 0.0;
  for(x=x_ref-5; x<x_ref+5; x++) {
    for(y=y_ref-5; y<y_ref+5; y++) {
      const double this_pixel = pixel(x,y) - median_pixel;
      a_tot += this_pixel;
      x_tot += (x*this_pixel);
      y_tot += (y*this_pixel);
    }
  }

  const double star_ctr_x = x_tot/a_tot;
  const double star_ctr_y = y_tot/a_tot;

  fprintf(stderr, "composite: star center at (%.1f, %.1f)\n",
	  star_ctr_x, star_ctr_y);
  /*  
      const int box_left   = ((int)(star_ctr_x + 0.5)) - 4;
      const int box_bottom = ((int)(star_ctr_y + 0.5)) - 4;
      const int box_right  = ((int)(star_ctr_x + 0.5)) + 4;
      const int box_top    = ((int)(star_ctr_y + 0.5)) + 4;
  */
  const int box_left = 0;
  const int box_bottom = 0;
  const int box_right = width;
  const int box_top = height;

  //
  // Now we've overlayed all the star pixels into the composite
  // image. Now find the center of the composite. We will set the Mask
  // to all 0 in the composite and toss the center pixel into the
  // star. All the adjacent pixels should be picked up automagically.
  //

  {
    // Compute the shape of the composite star.
    double sum_x = 0.0,
      sum_y  = 0.0,
      sum_xy = 0.0,
      sum_xx = 0.0,
      sum_yy = 0.0;
    double threshold_pixel = 0.0;
    const int pixel_count = (box_top-box_bottom+1) * (box_right-box_left+1);
    int row, col;
    int count = 0;

    for(row=box_bottom; row<=box_top; row++) {
      for(col=box_left; col<=box_right; col++) {
	const double this_pixel = pixel(col,row);
	threshold_pixel += this_pixel;
      }
    }
    threshold_pixel /= pixel_count;
    threshold_pixel -= median_pixel;

    for(row=box_bottom; row<=box_top; row++) {
      for(col=box_left; col<=box_right; col++) {
	  const double this_pixel = pixel(col,row) - median_pixel;

	  if(this_pixel < threshold_pixel) continue;
	  double scale_pixel;		// ln(pixel)
	  double sq_x;

	  if(this_pixel >= 1.0) {
	    scale_pixel = log(this_pixel);
	  } else {
	    scale_pixel = 0.0;
	  }
	  double del_x = (col - star_ctr_x);
	  double del_y = (row - star_ctr_y);
	  
	  sq_x = (del_x*del_x + del_y*del_y);

	  /* fprintf(stderr, "linear (%f, %f) at (%d,%d) coord\n",
		  sq_x, scale_pixel, col, row); */

	  sum_x  += sq_x;
	  sum_y  += scale_pixel;
	  sum_xy += sq_x*scale_pixel;
	  sum_xx += sq_x*sq_x;
	  sum_yy += scale_pixel*scale_pixel;
	  count++;
      }
    }

    // compute linear coefficients:
    b = (sum_xy - (sum_x*sum_y/count))/(sum_xx - (sum_x*sum_x/count));
    fprintf(stderr, "Std err = %f\n",
	    sqrt(((sum_yy - sum_y*sum_y/count)/(count - 2))/
		 (sum_xx - sum_x*sum_x/count)));
  }
  return -b;			// return inverse to make a positive number
}

/****************************************************************/
/*        hartman_index(angle)					*/
/****************************************************************/
double
Image::hartman_index(double angle_offset) {
  // provide some debugging info
  GetIStarList()->PrintStarSummary(stderr);

  if(GetIStarList()->NumStars < 1) {
    fprintf(stderr, "composite_fwhm: no stars found.\n");
    return 0.0;
  }
  
  const int StarIndex = LargestStar();

  const double star_ctr_x = GetIStarList()->StarCenterX(StarIndex);
  const double star_ctr_y = GetIStarList()->StarCenterY(StarIndex);
  
  const int box_left   = ((int)(star_ctr_x + 0.5)) - 4;
  const int box_bottom = ((int)(star_ctr_y + 0.5)) - 4;
  const int box_right  = ((int)(star_ctr_x + 0.5)) + 4;
  const int box_top    = ((int)(star_ctr_y + 0.5)) + 4;

  double b;

  {
    // Compute the shape of the composite star.
    double pixel_sum = 0.0;
    double hartman_sum = 0.0;
    int row, col;

    for(row=box_bottom; row<=box_top; row++) {
      for(col=box_left; col<=box_right; col++) {
	  const double this_pixel = pixel(col,row);

	  const double del_x = (col - star_ctr_x);
	  const double del_y = (row - star_ctr_y);
	  
	  const double this_angle = 3.0*(atan2(del_x, del_y)-angle_offset);
	  const double hartman_component = this_pixel * cos(this_angle);

	  const double sq_x = (del_x*del_x + del_y*del_y);
	  const double x = sqrt(sq_x);

#if 1
	  fprintf(stderr, "linear (%f, %f) at (%d,%d) coord, pixel=%f\n",
		  x, hartman_component, col, row, this_pixel);
#endif

	  pixel_sum += x*this_pixel;
	  hartman_sum += x*hartman_component;
      }
    }

    // compute linear coefficients:
    b = hartman_sum / pixel_sum;
    fprintf(stderr, "pixel sum = %f, hartman_sum = %f, ratio = %f\n",
	    pixel_sum, hartman_sum, b);
  }
  return -b;			// return inverse to make a positive number
}

/****************************************************************/
/*        General-purpose utilities				*/
/****************************************************************/

void AddObjectKeyword(const char *filename, const char *object) {
  ImageInfo info(filename);
  info.SetObject(object);
  info.WriteFITS();
}

char *DateToDirname(void) {
  struct tm *time_data;
  struct stat stat_struct;
  static char dirname[64];
  time_t now = time(0);
  // roll the clock back 12 hours so that the day rolls over at noon,
  // not midnight
  now -= (12*60*60);

  time_data = localtime(&now);
  sprintf(dirname,
	  "/home/IMAGES/%d-%d-%d",
	  1 + time_data->tm_mon,
	  time_data->tm_mday,
	  1900 + time_data->tm_year);
  if(stat(dirname, &stat_struct) < 0) {
    if(errno == ENOENT) {
      /* need to create the directory for today */
      if(mkdir(dirname, 0777) < 0) {
	perror("Unable to create today's directory");
      } else {
	fprintf(stderr, "Created directory for today: %s\n", dirname);
      }
    } else {
      perror("Cannot read today's directory");
    }
  }
  
  return dirname;
}

char *DateTimeString(void) {
  static char timestring[64];
  time_t now = time(0);

  strftime(timestring, sizeof(timestring), "%c", localtime(&now));
  return timestring;
}

char *FilenameAppendSuffix(char *root_filename, char suffix) {
  // verify last 5 characters are ".fits"
  static char new_filename[140];
  unsigned int root_length = strlen(root_filename);
  if(root_length < 8 || root_length >= sizeof(new_filename) ||
     strcmp(".fits", root_filename + root_length - 5) != 0) {
    fprintf(stderr,
	    "IMAGE_LIB: FilenameAppendSuffix given invalid filename.\n");
    return 0;
  }

  char short_filename[128];
  strcpy(short_filename, root_filename);
  short_filename[root_length -5] = 0;
  sprintf(new_filename, "%s%c.fits", short_filename, suffix);

  return new_filename;
}
  

char *NextValidImageFilename(void) {
  static int image_number = 0;
  struct stat stat_struct;	// result of call to stat()
  char *dirname = DateToDirname();

  static char full_filename[128];

  do {
    sprintf(full_filename, "%s/image%03d.fits", dirname, image_number);
    if(stat(full_filename, &stat_struct) < 0) {
      if(errno == ENOENT) {
	return full_filename;
      } else {
	perror("Cannot check on possible image file.");
	return 0;
      }
    } else {
      image_number++;
    }
  } while(1);
  /*NOTREACHED*/
  return 0;
}

void
CompositeImage::AddPixelToComposite(Image *SourceImage,
				    int    StarIndex,
				    double rel_x, double rel_y,
				    int    col,
				    int    row) {

  const int composite_x = (int) (rel_x * COMPOSITE_FACTOR +
				 width/2 + 0.5);
  const int composite_y = (int) (rel_y * COMPOSITE_FACTOR +
				 height/2 + 0.5);

  int xx, yy;

  for(xx = (composite_x - COMPOSITE_FACTOR/2);
      xx <= (composite_x + COMPOSITE_FACTOR/2);
      xx++) {
    for(yy = (composite_y - COMPOSITE_FACTOR/2);
	yy <= (composite_y + COMPOSITE_FACTOR/2);
	yy++) {
      if(xx < 0 || yy < 0 || xx >= width || yy >= height)
	continue;

      // Now add this pixel to the composite star
      pixel(xx,yy) += SourceImage->pixel(col, row);
      PixelCountArray[yy*width + xx]++;
    }
  }
  
}

void
Image::PrintBiggestStar(FILE *fp) {
  int StarIndex = LargestStar();

  if(StarIndex < 0) {
    fprintf(fp, "No star found.\n");
  } else {
    const int region_size = 12;
    const int x_low = (int)GetIStarList()->StarCenterX(StarIndex) -
      region_size/2;
    const int x_high = (int)GetIStarList()->StarCenterX(StarIndex) +
      region_size/2;;
    const int y_low = (int)GetIStarList()->StarCenterY(StarIndex) -
      region_size/2;
    const int y_high = (int)GetIStarList()->StarCenterY(StarIndex) +
      region_size/2;;
    int x, y;
    
    for(y=y_low; y<=y_high; y++) {
      for(x=x_low; x<= x_high; x++) {
	fprintf(fp, "%d\t%d\t%f\n",
		x - x_low,
		y - y_low,
		pixel(x,y));
      }
    }
  }
}

void
Image::PrintImage(FILE *fp) {
  int x, y;

  for(y=0; y<height; y++) {
    for(x=0; x<width; x++) {
      fprintf(fp, "%4d ", (int) (pixel(x,y)+0.5));
    }
    fprintf(fp, "\n");
  }
}

void 
CompositeImage::CompositeQuads(double *upper_right,
			       double *upper_left,
			       double *lower_right,
			       double *lower_left) {
  double sum_ur = 0.0;
  double sum_ul = 0.0;
  double sum_lr = 0.0;
  double sum_ll = 0.0;

  for(int row = 0; row < height/2; row++) {
    for(int col = 0; col < width/2; col++) {
      sum_lr += pixel(col+width/2, row+height/2);
      sum_ll += pixel(col, row+height/2);
      sum_ur += pixel(col+width/2, row);
      sum_ul += pixel(col, row);
    }
  }

  const double pixel_sum = sum_ur + sum_ul + sum_lr + sum_ll;
  *upper_right = sum_ur/pixel_sum;
  *upper_left  = sum_ul/pixel_sum;
  *lower_right = sum_lr/pixel_sum;
  *lower_left  = sum_ll/pixel_sum;
}
					      
Image *
Image::CreateSubImage (int box_bottom_y, // should say "top"
		       int box_left_x,
		       int box_height,
		       int box_width) const {
  Image *newOne = new Image(box_height, box_width);

  for(int row = 0; row < box_height; row++) {
    for(int col = 0; col < box_width; col++) {
      newOne->pixel(col, row) = pixel(col + box_left_x,
				      row + box_bottom_y);
    }
  }
  return newOne;
}

void GoToImageHDU(fitsfile *fptr) {
  int status = 0;
  int num_hdu;
  
  if (fits_get_num_hdus(fptr, &num_hdu, &status)) {
    printerror("GoToStarlistHDU: line " LINENO, status);
    return;
  }
  
  for (int i=0; i<num_hdu; i++) {
    if (fits_movabs_hdu(fptr, i+1, nullptr, &status)) {
      printerror("GoToImageHDU: line " LINENO, status);
      return;
    }
    char extension[80];
    char comment[80];
    char naxis_keyword[] = "NAXIS";
    int naxis;
    if (fits_read_key(fptr, TINT, naxis_keyword, &naxis, comment, &status)) {
      printerror("GoToImageHDU: line " LINENO, status);
      return;
    }
    if (naxis == 0) continue; // this isn't the image
    char extension_keyword[] = "XTENSION";
    if (fits_read_key(fptr, TSTRING, extension_keyword, extension, comment, &status)) {
      // no extension keyword, so this is the (uncompressed) image HDU
      return;
    }
    if (strcmp(extension, "BINTABLE") == 0 ||
	strcmp(extension, "IMAGE") == 0) return; // (compressed) image HDU

    fprintf(stderr, "GoToImageHDU: Bad fits format: extension=%s\n",
	    extension);
  }
  // should never get here!
  return;
}

// return 1 on success, 0 if no starlist HDU
int GoToStarlistHDU(fitsfile *fptr) {
  int status = 0;
  int num_hdu;
  
  if (fits_get_num_hdus(fptr, &num_hdu, &status)) {
    printerror("GoToStarlistHDU: line " LINENO, status);
    return 0;
  }
    
  for (int i=0; i<num_hdu; i++) {
    if (fits_movabs_hdu(fptr, i+1, nullptr, &status)) {
      printerror("GoToStarlistHDU: line " LINENO, status);
      return 0;
    }
    char extension[80];
    char comment[80];
    char extension_keyword[] = "XTENSION";
    if (fits_read_key(fptr, TSTRING, extension_keyword, extension, comment, &status)) {
      // no extension keyword, so this is the (uncompressed) image HDU
      status = 0;
      continue;
    }
    if (strcmp(extension, "TABLE") == 0) return 1; // Starlist table
  }
  // if we got here, then there's no starlist HDU
  return 0;
}

void
Image::InitializeImage(fitsfile *fptr) {
  int status = 0;
  int nfound;
  long naxes[2];
  int format;

  GoToImageHDU(fptr);

  if (fits_get_img_dim(fptr, &nfound, &status)) {
    printerror("get_img_dim(): line " LINENO, status);
    return;
  }
  if (nfound != 2) {
    fprintf(stderr, "InitializeImage: wrong # dimensions: %d\n", nfound);
    return;
  }

  if (fits_get_img_size(fptr, 2, naxes, &status)) {
    printerror("get_img_size(): line " LINENO, status);
    return;
  }
  if (fits_get_img_equivtype(fptr, &format, &status)) {
    printerror("get_img_type: line " LINENO, status);
    return;
  }
  //fprintf(stderr, "Image:get_img_type() returned %d\n", format);
  SetImageFormat(format);

  // Pick up the image size
  width  = naxes[0];
  height = naxes[1];

  double *temp_buffer;
  int AnyNull;			// anyone care??

  temp_buffer = (double *) malloc(sizeof(double) *
					  width * height);
  i_pixels    = (double *) malloc(sizeof(double) * width * height);
  StatisticsMask = (int *) malloc(sizeof(int) * width * height);

  if(temp_buffer == 0 || i_pixels == 0) {
    fprintf(stderr, "cb_window: unable to malloc temp_buffer.\n");
    return;
  }

  /* read the image into the temp_buffer */
  if(fits_read_2d_dbl(fptr,	// FITS file
		       0,	// group (not used)
		       0,	// null value (disabled)
		       naxes[0], // dim1 of temp_buffer
		       naxes[0], // dim1 of FITS file image
		       naxes[1], // dim2 of FITS file image
		       temp_buffer, // destination
		       &AnyNull,
		       &status)) {
    printerror("fits_read_2d_dbl: line " LINENO , status);
    return;
  }

  {
    int row, col;
    for(row=0; row < height; row++) {
      for(col = 0; col < width; col++) {
	pixel(col, row) = temp_buffer[row*width + col];
      }
    }
  }

  free(temp_buffer);
  AllPixelStatistics = (Statistics *) malloc(sizeof(Statistics));
  MaskedStatistics   = (Statistics *) malloc(sizeof(Statistics));
  StatisticsValid    = 0;
  image_info         = new ImageInfo(fptr);
  ThisStarList       = new IStarList(fptr);
#if 0 // not sure why this code was here.....
  if(ThisStarList->NumStars == 0) {
    delete ThisStarList;
    ThisStarList = 0;
  }
#endif
}  

DEC_RA
Image::ImageCenter(int &status) { // STATUS_OK if successful
  ImageInfo *info = GetImageInfo();
  if (info && info->WCSValid()) {
    const WCS *wcs = info->GetWCS();
    status = STATUS_OK;
    return wcs->Transform(width/2, height/2);
  }

  // get the starlist
  IStarList *list = GetIStarList();
  IStarList::IStarOneStar *one_star = 0;

  // find a star with a DEC/RA
  for(int i = 0; i < list->NumStars; i++) {
    IStarList::IStarOneStar *trial_star;

    trial_star = list->FindByIndex(i);
    if(trial_star->validity_flags & DEC_RA_VALID) {
      one_star = trial_star;
      break;
    }
  }

  if(one_star == 0) {
    // no star was found with a valid DEC/RA.  Might be because
    // nothing matched the catalog or might be because there aren't
    // any stars in the image.
    status = !STATUS_OK;
    return DEC_RA();
  }

  // These two offsets are positive if the star is East of the center
  // or is North of the center.
  PCS pix_offset;
  pix_offset.x = one_star->StarCenterX();
  pix_offset.y = one_star->StarCenterY();

  // rotation angle fixes North/South up swap after meridian flip
  double rotationAngle;
  if(image_info && image_info->RotationAngleValid()) {
    rotationAngle = image_info->GetRotationAngle();
  } else {
    rotationAngle = 0.0;
  }
  
  double image_scale = 1.52; // ST-9 default
  PCS center { width/2.0, height/2.0 };
  if(image_info && image_info->CDeltValid()) {
    image_scale = image_info->GetCDelt1();
  }
  TCStoImage pix_xform(image_scale, center, rotationAngle);
  TCS star_offset = pix_xform.toTCS(pix_offset);

  // We pretend that the image is centered on the star we found
  star_offset.x = -star_offset.x;
  star_offset.y = -star_offset.y;
  TCStoDecRA dec_ra_xform(one_star->dec_ra);
  DEC_RA TrueCenter = dec_ra_xform.toDecRA(star_offset);

  status = STATUS_OK;
  return TrueCenter;
}

/****************************************************************/
/*        ImageInfo						*/
/****************************************************************/
DEC_RA *
ImageInfo::GetNominalDecRA(void) {
  const string dec_string = GetValueString("DEC_NOM");
  const string ra_string = GetValueString("RA_NOM");
  int status = STATUS_OK;
  DEC_RA *loc = new DEC_RA(dec_string.c_str(), ra_string.c_str(), status);
  return loc;
}

JULIAN
ImageInfo::GetExposureStartTime(void) {
  return JULIAN(GetValueString("DATE-OBS").c_str());
}

JULIAN
ImageInfo::GetExposureMidpoint(void) {
  return GetExposureStartTime().add_days(GetExposureDuration()/(3600.0 * 24.0));
}

void
ImageInfo::SetRotationAngle(double angle) {
  char rot_string[80];
  sprintf(rot_string, "%.15lf", angle);
  SetValue(string("ROTATION"), string(rot_string));
}

Filter
ImageInfo::GetFilter(void) {
  if(KeywordPresent("FILTER")) {
    const char *filter_string = GetValueString(string("FILTER")).c_str();
    char filtername[2];
    filtername[0] = filter_string[0];
    filtername[1] = 0;
    return Filter(filtername);
  } else {
    return Filter("None");
  }
}

void
ImageInfo::SetFilter(Filter filter) {
  char filtername[2];
  filtername[0] = filter.NameOf()[0];
  filtername[1] = 0;
  SetValueString(string("FILTER"), string(filtername));
}

void
ImageInfo::SetNominalDecRA(DEC_RA *loc) {
  char dec_buffer[32];
  char ra_buffer[32];

  strcpy(dec_buffer, loc->string_dec_of());
  strcpy(ra_buffer, loc->string_longra_of());
  SetValueString(string("DEC_NOM"), string(dec_buffer));
  SetValueString(string("RA_NOM"), string(ra_buffer));
}

void
ImageInfo::SetFocus(double f) {	// net focus time [msec]
  char buffer[32];
  sprintf(buffer, "%d", (int) (f+0.5));
  SetValue(string("FOCUS"), string(buffer));
}

void
ImageInfo::SetExposureStartTime(JULIAN t) {
  time_t exposure_start_time = t.to_unix();
  struct tm *gt = gmtime(&exposure_start_time);
  char date_time_string[FLEN_VALUE];
  int status = 0;
    
  if(fits_time2str(1900 + gt->tm_year,
		   1    + gt->tm_mon,
		   gt->tm_mday,
		   gt->tm_hour,
		   gt->tm_min,
		   (double) gt->tm_sec,
		   1,
		   date_time_string,
		   &status)) {
    printerror("SetExposureStartTime: line " LINENO , status);
    return;
  }
  SetValueString(string("DATE-OBS"), string(date_time_string));
}

void
ImageInfo::SetExposureDuration(double d) { // shutter open time [seconds]
  char buffer[32];
  sprintf(buffer, "%.9lf", d);
  SetValue(string("EXPOSURE"), string(buffer));
}

void
ImageInfo::SetNorthIsUp(int north_is_up) {
  char buffer[2];
  buffer[0] = (north_is_up ? 'T' : 'F');
  buffer[1] = 0;
  SetValue(string("NORTH-UP"), string(buffer));
}

// Sets SITELAT, SITELON, OBSERVER
void
ImageInfo::SetLocalDefaults(void) {
  SetSiteLatLon(41.579347, -71.242241);
  SetObserver("MARK MUNKACSY (MMU)");
  //SetEGain(2.8); // electrons/ADU
  //SetCdelt(1.52, 1.52); // arcsec/pixel
}

void
ImageInfo::SetExpt2(double t2) {
  char buffer[32];
  sprintf(buffer, "%.9lf", t2);
  SetValue(string("EXP_T2"), string(buffer));
}

void
ImageInfo::SetExpt3(double t3) {
  char buffer[32];
  sprintf(buffer, "%.9lf", t3);
  SetValue(string("EXP_T3"), string(buffer));
}

void
ImageInfo::SetExpt4(double t4) {
  char buffer[32];
  sprintf(buffer, "%.9lf", t4);
  SetValue(string("EXP_T4"), string(buffer));
}

void
ImageInfo::SetObject(const char *object) {
  SetValueString(string("OBJECT"), string(object));
}

void
ImageInfo::SetCamGain(int gain) { // 0..255
  char buffer[32];
  sprintf(buffer, "%d", gain);
  SetValue(string("CAMGAIN"), string(buffer));
}

void
ImageInfo::SetReadmode(int mode) {   // 0..3
  char buffer[32];
  sprintf(buffer, "%d", mode);
  SetValue(string("READMODE"), string(buffer));
}

void
ImageInfo::SetOffset(int offset) {   // 0..255
  char buffer[32];
  sprintf(buffer, "%d", offset);
  SetValue(string("OFFSET"), string(buffer));
}

void
ImageInfo::SetBinning(int binning) { // 0..9
  char buffer[32];
  sprintf(buffer, "%d", binning);
  SetValue(string("BINNING"), string(buffer));
}

void
ImageInfo::SetHourAngle(double ha) { // hour angle in radians
  char buffer[32];
  sprintf(buffer, "%.9lf", ha);
  SetValue(string("HA_NOM"), string(buffer));
}

void
ImageInfo::SetAzEl(ALT_AZ alt_az) { // az/el in radians
  char az_buffer[32];
  char el_buffer[32];
  sprintf(az_buffer, "%.15lf", alt_az.azimuth_of());
  sprintf(el_buffer, "%.15lf", alt_az.altitude_of());
  SetValue(string("ELEVATIO"), string(el_buffer));
  SetValue(string("AZIMUTH"), string(az_buffer));
}

ALT_AZ
ImageInfo::GetAzEl(void) {
  const double azimuth = GetValueDouble("AZIMUTH");
  const double elevation = GetValueDouble("ELEVATIO");
  return ALT_AZ(elevation, azimuth);
}

void
ImageInfo::SetPSFPar(double par1, double par2) { // x,y blur
  char par1_buf[32];
  char par2_buf[32];
  sprintf(par1_buf, "%.9lf", par1);
  sprintf(par2_buf, "%.9lf", par2);
  SetValue(string("PSF_P1"), string(par1_buf));
  SetValue(string("PSF_P2"), string(par2_buf));
}

void
ImageInfo::SetPurpose(const char *purpose) { // purpose of the observation
  SetValueString(string("PURPOSE"), string(purpose));
}

void
ImageInfo::SetSetNum(int set_num) { // observation set number
  char buffer[32];
  sprintf(buffer, "%d", set_num);
  SetValue(string("SETNUM"), string(buffer));
}

void
ImageInfo::SetBlur(double x, double y) { // x, y blur in pixels
  char blurx_buf[32];
  char blury_buf[32];
  sprintf(blurx_buf, "%.9lf", x);
  sprintf(blury_buf, "%.9lf", y);
  SetValue(string("BLUR_X"), string(blurx_buf));
  SetValue(string("BLUR_Y"), string(blury_buf));
}

void
ImageInfo::SetObserver(const char *observer) {
  SetValueString(string("OBSERVER"), string(observer));
}

void
ImageInfo::SetAmbientTemp(double t) { // ambient temp in degrees C
  char buffer[32];
  sprintf(buffer, "%.2lf", t);
  SetValue(string("TAMBIENT"), string(buffer));
}

void
ImageInfo::SetCCDTemp(double t) {	// CCD temp in degrees C
  char buffer[32];
  sprintf(buffer, "%.2lf", t);
  SetValue(string("TCCD"), string(buffer));
}

void
ImageInfo::SetSiteLatLon(double lat, double lon) {	// degrees (+/-)
  char lat_buf[32];
  char lon_buf[32];
  sprintf(lat_buf, "%.9lf", lat);
  sprintf(lon_buf, "%.9lf", lon);
  SetValue(string("SITELAT"), string(lat_buf));
  SetValue(string("SITELON"), string(lon_buf));
}

void
ImageInfo::SetEGain(double eGain) {
  char buffer[32];
  sprintf(buffer, "%.2lf", eGain);
  SetValue(string("EGAIN"), string(buffer));
}

void
ImageInfo::SetAirmass(double airmass) {
  char buffer[32];
  sprintf(buffer, "%.8lf", airmass);
  SetValue(string("AIRMASS"), string(buffer));
}

void
ImageInfo::SetDatamax(double data_max) { // 65,530.0, typical
  char buffer[32];
  sprintf(buffer, "%lf", data_max);
  SetValue(string("DATAMAX"), string(buffer));
}

void
ImageInfo::SetFrameXY(int x, int y) {
  char buffer[32];
  sprintf(buffer, "%d", x);
  SetValue(string("FRAMEX"), string(buffer));
  sprintf(buffer, "%d", y);
  SetValue(string("FRAMEY"), string(buffer));
}

void
ImageInfo::SetCdelt(double cdelt1, double cdelt2) {
  char c1_buf[32];
  char c2_buf[32];
  sprintf(c1_buf, "%.3lf", cdelt1);
  sprintf(c2_buf, "%.3lf", cdelt2);
  SetValue(string("CDELT1"), string(c1_buf));
  SetValue(string("CDELT2"), string(c2_buf));
}

void
ImageInfo::SetCalStatus(const char *status_string) {
  SetValue(string("CALSTAT"), string(status_string));
}

ImageInfo *
Image::CreateImageInfo(void) {
  if(!image_info) {
    image_info = new ImageInfo(height, width);
  }

  return image_info;
}

void
Image::linearize(void) {
#if 1
  fprintf(stderr, "ERROR: linearize() invoked.\n");
  abort();
#else
  for (int row = 0; row < height; row++) {
    for (int col = 0; col < width; col++) {
      const double x = pixel(col, row);
      constexpr double Alpha = 2.5052979e-6;
      constexpr double Beta = -0.1642;
      const double adjustment = Alpha*x*x + Beta*x;
      pixel(col, row) = x + adjustment;
    }
  }
#endif
}

void
ImageInfo::SetAllInvalid(void) {
  key_values.clear();
  key_comments.clear();
  wcs = 0;
}

ImageInfo::ImageInfo(int h, int w) {
  SetAllInvalid();
  height = h;
  width = w;
}

ImageInfo::~ImageInfo(void) {
#if 0
  int status = 0;
  if (open_fptr) {
    if (fits_close_file(open_fptr, &status))
      printerror("fits_close_file, line " LINENO, status);
    open_fptr = nullptr;
  }
#endif
  delete wcs;
}

////////////////////////////////////////////////////////////////
//        FITS HEADER DATA TYPES
//    *.h type              Keyword       Type
//    ----------            ---------     -----------
//    NominalDecRA           DEC_NOM       TString
//                           RA_NOM        TString
//    Focus                  FOCUS         TLong (msec)
//    ExposureStartTime      DATE-OBS      TString
//    ExposureDuration       EXPOSURE      TDouble (secs)
//    NorthIsUp              NORTH-UP      TLOGICAL
//    RotationAngle          ROTATION      TDouble (radians)
//    Filter                 FILTER        TString
//    Object                 OBJECT        TString
//    HourAngle              HA_NOM        TDouble (radians)
//    Altitude               ELEVATIO      TDouble (radians)
//    Azimuth                AZIMUTH       TDouble (radians)
//    PSFpar1,2              PSF_P1/2      TDouble (pixels)
//    Blur_x,y               BLUR_X/Y      TDouble (pixels)
//    Observer               OBSERVER      TString
//    AmbientTemp            TAMBIENT      TDouble (Degree C)
//    CCD Temp               TCCD          TDouble (Degree C)
//    Site Lat/Lon           SITELAT/LON   TDouble (degrees)
//    Plate scale            CDELT1/2      TDouble (arcsec/pixel)
//    CCD Gain               EGAIN         TDouble (e/ADU)
//    Calibration Status     CALSTAT       TString (see comments)
//    Focus Blur             FOC-BLUR      TDouble (pixels)
//    Airmass                AIRMASS       TDouble (atmospheres)
//    Purpose                PURPOSE       TString
////////////////////////////////////////////////////////////////

ImageInfo::ImageInfo(const char *filename) {
  SetAllInvalid();
  fitsfile *fptr;
  int status = 0;
  this->associated_filename = filename;

  /* open the file, verify we have read access to the file. */
  if ( fits_open_file(&fptr, filename, READWRITE, &status) ) {
    printerror("fits_open_file, line " LINENO ,  status );
    return;
  }

  ReadAllKeys(fptr);
  if (fits_close_file(fptr, &status))
      printerror("fits_close_file, line " LINENO, status);
}

static bool KeywordToIgnore(const char *key) {
  std::list<std::string> badwords{"BITPIX",
				  "BZERO",
				  "SIMPLE",
				  "EXTEND",
				  "BSCALE",
				  "PCOUNT",
				  "GCOUNT",
				  "XTENSION",
				  "TFIELDS",
				  "TTYPE1",
				  "TFORM1",
				  "ZIMAGE",
				  "ZTILE1",
				  "ZTILE2",
				  "ZCMPTYPE",
				  "ZNAME1",
				  "ZNAME2",
				  "ZVAL1",
				  "ZVAL2",
				  "EXTNAME",
				  "ZSIMPLE",
				  "ZBITPIX",
				  "NAXIS",
				  "NAXIS1",
				  "NAXIS2",
				  "ZNAXIS",
				  "ZNAXIS1",
				  "ZNAXIS2",
				  "ZEXTEND"};
  std::string keystring(key);
  for (auto w:badwords) {
    if (w == keystring) return true;
  }
  return false;
}

void
ImageInfo::ReadAllKeys(fitsfile *fptr) {
  SetAllInvalid();
  GoToImageHDU(fptr);

  int  status = 0;
  int num_keys_exist = 0;
  int morekeys = 0;

  if(fits_get_hdrspace(fptr, &num_keys_exist, &morekeys, &status)) {
    fprintf(stderr, "ImageInfo: error reading number of keys.\n");
    return;
  }

  long naxes[2];
  if (fits_get_img_size(fptr, 2, naxes, &status)) {
    fprintf(stderr, "ImageInfo: error reading height/width.\n");
  } else {
    width = naxes[0];
    height = naxes[1];
  }

  for (int i=1; i<=num_keys_exist; i++) {
    char keyword[80];
    char value[80];
    char comment[80];
    if(fits_read_keyn(fptr, i, keyword, value, comment, &status)) {
      fprintf(stderr, "ImageInfo: error reading keyword %d\n", i);
    } else {
      // BITPIX, BZERO, and BSCALE are special and are not picked up
      // here. Instead, they are set using fits_create_img()
      if(not KeywordToIgnore(keyword)) {
	key_values[string(keyword)] = string(value);
	key_comments[string(keyword)] = string(comment);
      }
    }
  }

  wcs = NewWCS(this);
}

ImageInfo::ImageInfo(fitsfile *fptr) {
  ReadAllKeys(fptr);
}

void
Image::WriteFITSAuto(const char *filename, bool compress) const {
  switch(image_format) {
  case USHORT_IMG:
    return WriteFITS16(filename, compress);
  case ULONG_IMG:
    return WriteFITS32(filename, compress);
  case FLOAT_IMG:
  case DOUBLE_IMG:
    return WriteFITSFloat(filename, compress);
  default:
    fprintf(stderr, "ERROR! Image.cc: invalid image_format\n");
  }
  /*NOTREACHED*/
}

void
ImageInfo::WriteFITS(fitsfile *fitsptr) {
  // This should only be issued once on an open file.  Doing it more
  // than once will have unpredictable effects. In particular, a
  // keyword won't be erased if a WriteFITS is done with a validity
  // flag cleared after first being executed with that validity flag
  // set. Stay out of trouble and only issue one WriteFITS while the
  // file is open.

  // If fitsptr == null, then use the fitsptr kept in this->open_fptr
  //
  fitsfile *fptr = fitsptr;
  int status = 0;
  bool use_standalone = (fitsptr == nullptr);
  if (use_standalone) {
    if (fits_open_file(&fptr, associated_filename, READWRITE, &status)) {
      printerror("fits_open_file, line " LINENO , status);
      return;
    }
  }
  
  GoToImageHDU(fptr);

  if(wcs && wcs->IsValidWCS()) {
    wcs->UpdateFITSHeader(this);
  }

  for (auto e : key_values) {
    const string &keyword = e.first;
    const string &value = e.second;
    const string &comment = key_comments[keyword];
    char buffer[181];

    sprintf(buffer, "%-8s= %20s / %s",
	    keyword.c_str(), value.c_str(), comment.c_str());
    for (int i = strlen(buffer); i < 80; i++) {
      buffer[i] = ' ';
    }
    buffer[80] = 0;

    status = 0;
    if (fits_update_card(fptr, keyword.c_str(), buffer, &status)) {
      printerror("fits_update_card: ", status);
      return;
    }
  }
  if (use_standalone) {
    if ( fits_close_file(fptr, &status) )
      printerror("fits_close_file, line " LINENO , status );
  }
}
  
void
CompositeImage::AddStarToComposite(Image *SourceImage,
				   double star_center_x,
				   double star_center_y) {
  //fprintf(stderr, "adding star with center at (%.2f, %.2f)\n",
  //	  star_center_x, star_center_y);

  const int source_width = width/COMPOSITE_FACTOR;
  const int source_height = height/COMPOSITE_FACTOR;

  // if star would fall off the edge, skip it
  if (star_center_x - source_width/2 < 2.0 ||
      star_center_y - source_height/2 < 2.0 ||
      star_center_x + source_width/2 >= SourceImage->width-1 ||
      star_center_y + source_height/2 >= SourceImage->height-1) return;

  int k_x =  (int)(star_center_x + 0.5) - source_width/2;
  int k_y =  (int)(star_center_y + 0.5) - source_height/2;

  for(int offsetx=-1; offsetx < source_width+1; offsetx++) {
    for(int offsety=-1; offsety < source_height+1; offsety++) {

      for(int stepx = 0; stepx < COMPOSITE_FACTOR; stepx++) {
	for(int stepy = 0; stepy < COMPOSITE_FACTOR; stepy++) {
	  double minoffsetx = -0.5 + (1.0/(2.0*COMPOSITE_FACTOR)) + stepx*(1.0/COMPOSITE_FACTOR);
	  double minoffsety = -0.5 + (1.0/(2.0*COMPOSITE_FACTOR)) + stepy*(1.0/COMPOSITE_FACTOR);

	  double mapped_x = ((k_x + offsetx + minoffsetx)-star_center_x)*
	    COMPOSITE_FACTOR + (width/2 + 0.5);
	  double mapped_y = ((k_y + offsety + minoffsety)-star_center_y)*
	    COMPOSITE_FACTOR + (height/2 + 0.5);

	  int x_low = (int) mapped_x;
	  int y_low = (int) mapped_y;
	  double high_x_fract = mapped_x - x_low;
	  double high_y_fract = mapped_y - y_low;
	  double low_x_fract = (1.0 - high_x_fract);
	  double low_y_fract = (1.0 - high_y_fract);

	  /* fprintf(stderr, "  mapping pixel from (%d, %d) to (%.2f, %.2f)\n",
	     k_x+offsetx, k_y+offsety, mapped_x, mapped_y); */
	  // Now add this pixel to the composite star
	  const double source_pixel = 
	    SourceImage->pixel(k_x+offsetx, k_y+offsety);

	  AddFractionalPixel(source_pixel,
			     low_x_fract * low_y_fract,
			     x_low, y_low);
	  AddFractionalPixel(source_pixel,
			     low_x_fract * high_y_fract,
			     x_low, 1+y_low);
	  AddFractionalPixel(source_pixel,
			     high_x_fract * low_y_fract,
			     x_low+1, y_low);
	  AddFractionalPixel(source_pixel,
			     high_x_fract * high_y_fract,
			     x_low + 1, y_low + 1);
	}
      }
    }
  }
  //ascii_print(stderr);
}

void 
CompositeImage::AddFractionalPixel(double pixel_value,
				   double fraction,
				   int col,
				   int row) {
  if (fraction <= 0.0) return;
  if (col >= 0 && col < width && row >= 0 && row < height) {
    const int PCA_index = row*width + col;
    
    double base_val = pixel(col, row) * PixelCountArray[PCA_index];
    base_val += fraction * pixel_value;
    PixelCountArray[PCA_index] += fraction;
    pixel(col, row) = base_val / PixelCountArray[PCA_index];
  }
}

void
CompositeImage::ascii_print(FILE *fp) {
  for (int row = 0; row < height; row++) {
    for (int col = 0; col < width; col++) {
      fprintf(fp, "%.1lf", pixel(col, row));
    }
    fprintf(fp, "\n");
  }
  fprintf(fp, "\n\n");
}

fitsfile *OpenAsFITSFile(const char *filename) {
  fitsfile *fptr;       /* pointer to the FITS file, defined in fitsio.h */
  int status = 0;

  /* open the file, verify we have read access to the file. */
  if ( fits_open_file(&fptr, filename, READWRITE, &status) ) {
    printerror("fits_open_file, line " LINENO ,  status );
    return 0;
  }
  return fptr;
}

void CloseFITSFile(fitsfile *f) { // This does not write!!!!
  int status = 0;
  
  if(fits_close_file(f, &status)) {
    printerror("fits_close_file: line " LINENO , status);
    return;
  }
}
  
string
ImageInfo::GetValueString(const string &keyword) {
  string &value = key_values[keyword];
  if (value[0] == '\'') {
    auto end_pos = value.find_last_of('\'');
    return value.substr(1, end_pos-1);
  } else {
    return value;
  }
}

string
ImageInfo::GetValueLiteral(const string &keyword) {
  string &value = key_values[keyword];
  return value;
}

bool
ImageInfo::KeywordPresent(const string &keyword) {
  return key_values.count(keyword) != 0;
}

double
ImageInfo::GetValueDouble(const string &keyword) {
  return atof(key_values[keyword].c_str());
}

bool
ImageInfo::GetValueBool(const string &keyword) {
  return 'T' == key_values[keyword].c_str()[0];
}

int
ImageInfo::GetValueInt(const string &keyword) {
  return atoi(key_values[keyword].c_str());
}

void
ImageInfo::SetValueString(const string &keyword, const string &value) {
  SetValue(keyword, "'" + value + "'");
}

void
ImageInfo::SetFocusBlur(double blur) {
  char buffer[32];
  sprintf(buffer, "%.4lf", blur);
  SetValue(string("FOC-BLUR"), string(buffer));
}

void
ImageInfo::PullFrom(ImageInfo *source) {
  height = source->height;
  width = source->width;

  for (const auto& n : source->key_values) {
    (void)key_values.insert({n.first, n.second});
  }
  for (const auto& n : source->key_comments) {
    (void)key_comments.insert({n.first, n.second});
  }
}

void
Image::RemoveShutterGradient(double exposure_time) {
#if 1
  fprintf(stderr, "ERROR: RemoveShutterGradient() invoked.\n");
  abort();
#else
  const double inv_exposure_time = 1.0/exposure_time;
  const double gradient_slope = (3.3e-6)*inv_exposure_time*inv_exposure_time -
    0.000111*inv_exposure_time;
  // gradient_slope will be a negative number. Row 0 will have
  // received more photons than row 512.
  for (int y=0; y<height; y++) {
    const double row_adjust = 1.0 + (y-height/2)*gradient_slope;
    for (int x=0; x<width; x++) {
      pixel(x,y) = pixel(x,y)/row_adjust;
    }
  }
#endif
}

// Return an image binned
Image *
Image::bin(int binning) const {
  if (binning < 1 or binning > 8) {
    fprintf(stderr, "ERROR: Image::bin(%d) -- invalid binning ratio.\n",
	    binning);
    return nullptr;
  }

  fprintf(stderr, "Image::bin(%d) going from (%d x %d) to (%d x %d)\n",
	  binning, width, height, width/binning, height/binning);

  Image *i = new Image(height/binning, width/binning);
  ImageInfo *orig_info = GetImageInfo();
  if (orig_info == nullptr) {
    fprintf(stderr, "ERROR: Image::bin() cannot bin image with no ImageInfo\n");
    return nullptr;
  }

  const int usable_height = (height/binning)*binning;
  const int usable_width = (width/binning)*binning;
  for (int y=0; y<usable_height; y++) {
    int tgt_y = y/binning;
    for (int x=0; x<usable_width; x++) {
      int tgt_x = x/binning;

      i->pixel(tgt_x, tgt_y) += pixel(x,y);
    }
  }

  ImageInfo *info = i->GetImageInfo();
  if (info == nullptr) info = i->CreateImageInfo();
  info->PullFrom(GetImageInfo());

  if (orig_info->BinningValid()) {
    info->SetBinning(orig_info->GetBinning()*binning);
  } else {
    info->SetBinning(binning);
  }

  if (orig_info->CDeltValid()) {
    double cdelt1 = orig_info->GetCDelt1();
    double cdelt2 = orig_info->GetCDelt2();
    info->SetCdelt(cdelt1*binning, cdelt2*binning);
  }

  if (orig_info->DatamaxValid()) {
    info->SetDatamax(orig_info->GetDatamax()*binning*binning);
  }
  return i;
}

// Release with free() when filename (return value) is no longer
// needed.
char *CreateTmpCopy(const char *orig_filename) {
  bool do_copy = false;
  int fd;
  char *newname = nullptr;

  char ftemplate[] = "/tmp/ImageXXXXXX.fits";
  fd = mkstemps(ftemplate, 5);
  if (fd < 0) {
    // failed.
    std::cerr << "ERROR: CreateTmpCopy(): Error creating copy. Errno = "
	      << errno << std::endl;
  } else {
    // succeeded
    do_copy = true;
    newname = strdup(ftemplate);
  }

  if (do_copy) {
    int fd_orig = open(orig_filename, O_RDONLY);
    if (fd_orig < 0) {
      perror("Error: CreateTmpCopy(): Unable to open source Image file");
      free(newname);
      close(fd);
      return nullptr;
    }
    static constexpr long BUFFERSIZE = 4096*32;
    char *buffer = (char *) malloc(BUFFERSIZE);
    if (!buffer) {
      perror("Error: CreateTmpCopy(): Unable to allocate buffer memory");
      free(newname);
      close(fd_orig);
      close(fd);
      return nullptr;
    }

    bool keep_going = true;
    bool error_encountered = false;
    do {
      ssize_t amount = read(fd_orig, buffer, BUFFERSIZE);
      if (amount == 0) {
	// end of file
	error_encountered = false;
	keep_going = false;
      } else if (amount < 0) {
	perror("Error: CreateTmpCopy(): Error reading source Image file");
	error_encountered = true;
      } else {
	bool do_more = false;
	ssize_t previously_written = 0;
	do {
	  ssize_t to_write = amount-previously_written;
	  ssize_t num_written = write(fd, buffer+previously_written, to_write);
	  if (num_written < 0) {
	    perror("Error: CreateTmpCOpy(): Error writing new Image file");
	    error_encountered = true;
	    break;
	  } else if(num_written != to_write) {
	    previously_written += num_written;
	    do_more = true;
	  }
	} while(do_more and not error_encountered);
      }
    } while(keep_going and not error_encountered);

    if (error_encountered) {
	free(newname);
	close(fd);
	close(fd_orig);
	return nullptr;
    } 
    close(fd);
    close(fd_orig);
    return newname;
  }
  free(newname);
  return nullptr;
}

