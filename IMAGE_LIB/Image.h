/* This may look like C code, but it is really -*-c++-*- */
/*  Image.h -- Manage an image
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
#ifndef _IMAGE_H
#define _IMAGE_H

#include <stdlib.h>
#include <fitsio.h>
#include <dec_ra.h>
#include <alt_az.h>
#include "Statistics.h"
#include "IStarList.h"
#include "Filter.h"
#include <string.h>		// strdup()
#include "wcs.h"

#include <string>
#include <unordered_map>
using namespace std;

// ImageInfo provides amplifying information about the image. When the
// image is stored as a FITS file, the amplifying info is stored as
// keyword data. If an Image is created by reading a FITS file, the
// ImageInfo amplifying data is created if it is present in the FITS
// file as keyword information.
class ImageInfo {
public:
  ////////////////////////////////
  //        CONSTRUCTORS
  ////////////////////////////////
  ImageInfo(int h, int w); // ONLY to be used by Image::CreateImageInfo()
  ImageInfo(fitsfile *fptr); // used by Image::Image for a "linked"
			     // ImageInfo 
  ImageInfo(const char *filename); // user-accessible for "standalone"
				   // ImageInfo 
  ~ImageInfo(void);

  void PullFrom(ImageInfo *source);

  ////////////////////////////////
  //   PUBLIC VISIBLE ATTRIBUTES
  ////////////////////////////////
  int height, width;

  ////////////////////////////////
  //        FILE OPERATIONS
  // WriteFITS takes the current ImageInfo and puts it into the FITS
  // file specified. For reading ImageInfo from a FITS file,
  // see the constructor provided to do just that.
  ////////////////////////////////
  void WriteFITS(fitsfile *fptr=nullptr); // for *both* "linked" and
					  // "standalone" ImageInfo

  ////////////////////////////////
  //        VALID checks
  ////////////////////////////////

  int NominalDecRAValid(void)      { return (KeywordPresent("DEC_NOM") and
						   KeywordPresent("RA_NOM")); }
  int FocusValid(void)             { return KeywordPresent("FOCUS"); }
  int ExposureStartTimeValid(void) { return KeywordPresent("DATE-OBS"); }
  int ExposureMidpointValid(void)  { return (KeywordPresent("DATE-OBS") &&
					     KeywordPresent("EXPOSURE")); }
  int ExposureDurationValid(void)  { return  KeywordPresent("EXPOSURE"); }
  int FilterValid(void)            { return KeywordPresent("FILTER"); }
  int NorthIsUpValid(void)         { return KeywordPresent("NORTH-UP"); }
  int RotationAngleValid(void)     { return KeywordPresent("ROTATION"); }
  int EGainValid(void)             { return KeywordPresent("EGAIN"); }
  int AirmassValid(void)           { return KeywordPresent("AIRMASS"); }
  int CDeltValid(void)             { return KeywordPresent("CDELT1"); }
  int CalStatusValid(void)         { return KeywordPresent("CATSTAT"); }
  int ObjectValid(void)            { return KeywordPresent("OBJECT"); }
  int PurposeValid(void)           { return KeywordPresent("PURPOSE"); }
  int SetNumberValid(void)         { return KeywordPresent("SETNUM"); }
  int WCSValid(void)               { return (wcs != NULL); }
  int Expt2Valid(void)		   { return KeywordPresent("EXP_T2"); }
  int Expt3Valid(void)		   { return KeywordPresent("EXP_T3"); }
  int Expt4Valid(void)		   { return KeywordPresent("EXP_T4"); }
  int FocusBlurValid(void)         { return KeywordPresent("FOC-BLUR"); }
  int CamGainValid(void)           { return KeywordPresent("CAMGAIN"); }
  int ReadmodeValid(void)          { return KeywordPresent("READMODE"); }
  int OffsetValid(void)            { return KeywordPresent("OFFSET"); }
  int CameraValid(void)            { return KeywordPresent("CAMERA"); }
  int BinningValid(void)           { return KeywordPresent("BINNING"); }
  int DatamaxValid(void)           { return KeywordPresent("DATAMAX"); }
  int FrameXYValid(void)           { return (KeywordPresent("FRAMEX") and
                                                  KeywordPresent("FRAMEY")); }

  ////////////////////////////////
  //        GET
  // If data is not valid, the return
  // value from these is undefined, but
  // calling them is guaranteed not to
  // result in segmentation fault.
  ////////////////////////////////

  // pointer to constant value. Do not modify. Valid as long as Image
  // exists.
  DEC_RA *GetNominalDecRA(void);
  double  GetFocus(void) { return GetValueInt("FOCUS"); }
  JULIAN  GetExposureStartTime(void);
  JULIAN  GetExposureMidpoint(void);
				// returns shutter open time [seconds]
  double  GetExposureDuration(void) { return GetValueDouble("EXPOSURE"); }
  int     NorthIsUp(void) { return GetValueBool("NORTH-UP"); }
  double  GetRotationAngle(void) { return GetValueDouble("ROTATION"); }
  Filter  GetFilter(void);
  // free after use.
  const char *GetObject(void) { return strdup(GetValueString("OBJECT").c_str()); }
  double GetHourAngle(void) { return GetValueDouble("HA_NOM"); }
  ALT_AZ GetAzEl(void);
  double GetPSFPar1(void) { return GetValueDouble("PSF_P1"); }
  double GetPSFPar2(void) { return GetValueDouble("PSF_P2"); }
  double GetBlurX(void) { return GetValueDouble("BLUR_X"); }
  double GetBlurY(void) { return GetValueDouble("BLUR_Y"); }
  // free after use.
  const char *GetObserver(void) { return strdup(GetValueString("OBSERVER").c_str()); }
  double GetAmbientTemp(void) { return GetValueDouble("TAMBIENT"); }
  double GetCCDTemp(void) { return GetValueDouble("TCCD"); }
  double GetSiteLongitude(void) { return GetValueDouble("SITELONG"); }
  double GetSiteLatitude(void) { return GetValueDouble("SITELAT"); }
  double GeteGain(void) { return GetValueDouble("EGAIN"); }
  double GetAirmass(void) { return GetValueDouble("AIRMASS"); }
  double GetCDelt1(void) { return GetValueDouble("CDELT1"); }
  double GetCDelt2(void) { return GetValueDouble("CDELT2"); }
  // free after use (x3)
  const char *GetCalStatus(void) { return strdup(GetValueString("CALSTAT").c_str()); }
  const char *GetPurpose(void) { return strdup(GetValueString("PURPOSE").c_str()); }
  const char *GetCamera(void) { return strdup(GetValueString("CAMERA").c_str()); }
  int GetSetNum(void) { return GetValueInt("SETNUM"); }
  // do not free after use
  const WCS *GetWCS(void) { return wcs; }
  double GetExpt2(void) { return GetValueDouble("EXP_T2"); }
  double GetExpt3(void) { return GetValueDouble("EXP_T3"); }
  double GetExpt4(void) { return GetValueDouble("EXP_T4"); }
  double GetFocusBlur(void) { return GetValueDouble("FOC-BLUR"); }
  int    GetCamGain(void) { return GetValueInt("CAMGAIN"); }
  int    GetReadmode(void) { return GetValueInt("READMODE"); }
  int    GetOffset(void) { return GetValueInt("OFFSET"); }
  int    GetBinning(void) { return GetValueInt("BINNING"); }
  double GetDatamax(void) { return GetValueDouble("DATAMAX"); }
  int    GetFrameX(void) { return GetValueInt("FRAMEX"); }
  int    GetFrameY(void) { return GetValueInt("FRAMEY"); }
  
  ////////////////////////////////
  //        SET
  ////////////////////////////////

				// Sets SITELAT, SITELON, OBSERVER
  void    SetLocalDefaults(void);

  void    SetObject(const char *object);
  void    SetHourAngle(double ha); // hour angle in radians
  void    SetAzEl(ALT_AZ alt_az); // az/el in radians
  void    SetPSFPar(double par1, double par2); // x,y blur
  void    SetBlur(double x, double y); // x, y blur in pixels
  void    SetObserver(const char *observer);
  void    SetAmbientTemp(double t); // ambient temp in degrees C
  void    SetCCDTemp(double t);	// CCD temp in degrees C
  void    SetSiteLatLon(double lat, double lon);	// degrees (+/-)
  void    SetNominalDecRA(DEC_RA *loc);
  void    SetFocus(double f);	// net focus time [msec]
  void    SetExposureStartTime(JULIAN t);
  void    SetExposureDuration(double d); // shutter open time [seconds]
  void    SetNorthIsUp(int north_is_up);
  void    SetRotationAngle(double angle); // radians
  void    SetFilter(Filter filter);
  void    SetEGain(double eGain); // electrons/ADU
  void    SetAirmass(double airmass);
  void    SetCdelt(double cdelt1, double cdelt2); // arcsec/pixel
  void    SetCalStatus(const char *status_string);
  void    SetPurpose(const char *purpose);
  void    SetSetNum(int set_number);
  void    SetWCS(const WCS *new_wcs) { wcs = new_wcs; }
  void    SetExpt2(double t2);
  void    SetExpt3(double t3);
  void    SetExpt4(double t4);
  void    SetFocusBlur(double blur);
  void    SetCamGain(int gain);	   // 0..255
  void    SetReadmode(int mode);   // 0..3
  void    SetOffset(int offset);   // 0..255
  void    SetBinning(int binning); // 0..9
  void    SetDatamax(double data_max); // 65,530.0, typical
  void    SetFrameXY(int x, int y);

  bool KeywordPresent(const string &keyword);
  string GetValueString(const string &keyword);
  string GetValueLiteral(const string &keyword);
  double GetValueDouble(const string &keyword);
  int GetValueInt(const string &keyword);
  bool GetValueBool(const string &keyword);
  void SetValue(const string &keyword, const string &value);
  void SetValueString(const string &keyword, const string &value);
  void SetComment(const string &keyword, const string &comment);

  ////////////////////////////////
  //        PRIVATE
  ////////////////////////////////
private:
  std::unordered_map<string, string> key_values;
  std::unordered_map<string, string> key_comments;

  const WCS    *wcs;		// coordinate conversion

  void SetAllInvalid(void);
  void ReadAllKeys(fitsfile *fptr);

  const char *associated_filename;
};

class Image {
public:
  int height;
  int width;

  Statistics *statistics(void);

  void add(const Image *i);
  void subtract(const Image *i);
  void subtractKeepPositive(const Image *i);
  void scale(double d);
  void scale(const Image *i);
  void clip_low(double d);
  void clip_high(double d);

  Image(const char *fits_filename);
  Image(const void *fits_file_in_mem, size_t file_filelength);
  Image(double ExposureTimeSecs, int BinMode);
  Image(int height, int width);

  ~Image(void);

  void linearize(void); // should only be done once to an image
  void RemoveShutterGradient(double exposure_time);

  // Return an image binned
  Image *bin(int binning) const;

  // A word about compression: Compression is now the standard. Most
  // WriteFITSxxx() functions will write a compressed file unless you
  // go out of the way to make it uncompressed.

  void WriteFITS(const char *filename, bool compress=true) const;
  void WriteFITS16(const char *filename, bool compress=true) const { WriteFITS(filename,compress); }
  void WriteFITS32(const char *filename, bool compress=true) const;
  void WriteFITSFloat(const char *filename, bool compress=true) const;
  void WriteFITSFloatUncompressed(const char *filename) const {
    WriteFITSFloat(filename, false); }
  void WriteFITSAuto(const char *filename, bool compress=true) const;

  inline double & pixel(int x, int y) const { return i_pixels[y*width + x]; }

  DEC_RA ImageCenter(int &status); // STATUS_OK if successful

  double composite_fwhm(void);
  double hartman_index(double angle_offset);

  // format comes from cfitsio.h. Can be USHORT_IMG, ULONG_IMG, FLOAT_IMG.
  void SetImageFormat(int format) { image_format = format; }

  // When GetIStarList is called the first time, the star list is
  // created and no subsequent changes to the image will affect the
  // star list.

  // invoke "find_stars" to pre-populate the IStarList
  void find_stars(void);
  IStarList *GetIStarList(void);
  IStarList *RecalculateIStarList(void);
  IStarList *PassiveGetIStarList(void); // never recalc

  // Get the image pixel value at a fraction (0 .. 1.0) of the
  // "histogram" from dimmest to brightest. If fraction == 0.0, you
  // get the image's dimmest pixel. If fraction == 1.0, you get the
  // image's brightest pixel. If fraction == 0.5, you get the image's
  // median pixel value.
  double HistogramValue(double fraction);

  void PrintBiggestStar(FILE *fp);
  int LargestStar(void);	// returns -1 if none found

  // Create an image that is a subset of the current image
  Image *CreateSubImage (int box_top_y, // top has smaller y index than bottom
			 int box_left_x,
			 int box_height,
			 int box_width) const;
  void PrintImage(FILE *fp);

  // Warning: may return <nil> if no image info is available
  ImageInfo *GetImageInfo(void) const { return image_info; }
  ImageInfo *CreateImageInfo(void);

private:
  void InitializeImage(fitsfile *fptr);

  double *i_pixels;
  int StatisticsValid;
  Statistics *AllPixelStatistics;
  Statistics *MaskedStatistics;
  int *StatisticsMask;
  ImageInfo *image_info;
  int image_format; // from cfitsio.h: USHORT_IMG, ULONG_IMG, FLOAT_IMG

  IStarList *ThisStarList;

  inline int &Mask(int x, int y) { return StatisticsMask[y*width + x]; }
  
  void UpdateStatistics(Statistics *stats, int UseMask);

  // recursively adds adjacent pixels to a star
  void AddAdjacentPixels(int    star_index,
			 int    col,
			 int    row,
			 double threshold);
};
  
/****************************************************************/
/*        General-purpose utilities				*/
/****************************************************************/

char *DateToDirname(void);
char *DateTimeString(void);
char *NextValidImageFilename(void);
char *FilenameAppendSuffix(char *root_filename, char suffix);
fitsfile *OpenAsFITSFile(const char *filename);
void CloseFITSFile(fitsfile *f); // This does not write!!!!
void AddObjectKeyword(const char *filename, const char *object);


/****************************************************************/
/*        CompositeImage					*/
/****************************************************************/
class 
CompositeImage : public Image {
public:
  CompositeImage(int CompositeHeight, int CompositeWidth) :
    Image(CompositeHeight, CompositeWidth) {
      PixelCountArray = (double *) malloc(sizeof(double) *
					  CompositeHeight *
					  CompositeWidth);
      for(int k = 0; k < (CompositeHeight*CompositeWidth); k++) {
	PixelCountArray[k] = 0.0;
      }
      // keep this an *even* number, *not* an odd number
      COMPOSITE_FACTOR = 10;
    }

  void AddStarToComposite(Image *SourceImage,
			  double star_center_x,
			  double star_center_y);

  void AddPixelToComposite(Image *SourceImage,
			   int StarIndex,
			   double offset_x, double offset_y,
			   int col,
			   int row);
  void CompositeCenter(double *x,
		       double *y);

  void CompositeQuads(double *upper_right,
		      double *upper_left,
		      double *lower_right,
		      double *lower_left);

  int COMPOSITE_FACTOR;

  void ascii_print(FILE *fp);

private:
  void AddFractionalPixel(double pixel_value,
			  double fraction,
			  int col,
			  int row);

public:
  // Create an array where we keep a count of the number of pixels
  // superposed into each pixel of the Composite star.
  double *PixelCountArray;
};

CompositeImage *
BuildComposite(Image **i_array,
	       int     num_images);
CompositeImage *
BuildComposite(Image *orig_image,
	       IStarList *starlist,
	       int composite_size = 100);

void GoToImageHDU(fitsfile *fptr);
int GoToStarlistHDU(fitsfile *fptr); // return 1 on success, 0 if no starlist HDU
  
// Release with free() when filename (return value) is no longer
// needed.
char *CreateTmpCopy(const char *orig_filename);

#endif
