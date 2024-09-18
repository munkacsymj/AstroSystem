/* This may look like C code, but it is really -*-c++-*- */
/*  camera_api.h -- Implements user view of camera
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
#ifndef _CAMERA_API_H
#define _CAMERA_API_H

class Drifter; // forward declaration
#include <Image.h>

// connect_to_camera() will establish a connection to the camera server
// process running on the scope computer.  It will block for as long
// as necessary to establish the connection. If unable to establish a
// connection (for whatever reason), it will print an error message to
// stderr and will exit.
void connect_to_camera(void);
// camera_is_available() returns 1 if a connection can be established;
// a 0 if something goes wrong
int camera_is_available(void);

struct subframe_t {
  int box_left {0}; // 0-based, left<right
  int box_right {0};
  int box_bottom {0}; // 0-based, bottom<top
  int box_top {0};

  bool box_uncropped(void) { return box_left == 0 and
      box_right == 0 and
      box_bottom == 0 and
      box_top == 0 and
      box_left == 0; }
  int box_width(void) { return 1+box_right-box_left; }
  int box_height(void) { return 1+box_top-box_bottom; }
};

class exposure_flags {
public:
  enum E_PixelFormat { E_uint16, E_uint32, E_float };
  subframe_t subframe;
private:
  int flag_word;
  double suggested_track_exposure;

  static const int E_TRACK_CCD = 1;
  static const int E_MAIN_CCD  = 0;
  static const int E_CCD_MASK  = 0x01;

  static const int E_SHUTTER_OPEN = 2;
  static const int E_SHUTTER_SHUT = 0;
  static const int E_SHUTTER_MASK = 2;

  static const int E_CONCURRENT_TRACK = 0x04;
  static const int E_CONCURRENT_TRACK_MASK = 0x04;

  static const int E_TRACK_REQ_MASK = 0x08;
  static const int E_TRACK_OPTIONAL = 0x00;
  static const int E_TRACK_REQUIRED = 0x08;

  bool e_compress { false };
  int e_readoutgain { 0 };
  int e_readoutmode { 0 };
  int e_binning { 1 };
  int e_offset { 5 };
  double e_usbtraffic {0};
  E_PixelFormat e_outputformat { E_uint16 };
  
  // see Filter.cc for values that can be found here
  static const int E_FILTER_MASK = 0xf0;
  static const int E_FILTER_SHIFT = 4; // bits shifted to the left

public:
  exposure_flags(const char *profile_name=nullptr); // fetch flags from image_profiles.json

  void SetFilter(Filter filter) {
    int shifted_word = (filter.FlagWordValue() << E_FILTER_SHIFT) &
      E_FILTER_MASK;
    flag_word = (shifted_word | (flag_word & ~E_FILTER_MASK));
  }

  Filter FilterRequested(void) {
    Filter response;
    response.SetFilterIDIndex((flag_word & E_FILTER_MASK) >> E_FILTER_SHIFT);
    return response;
  }

  void SetConcurrentTrack(int SolidTrackRequired) {
    flag_word = ((flag_word | E_CONCURRENT_TRACK) |
		 (SolidTrackRequired ? E_TRACK_REQUIRED : E_TRACK_OPTIONAL));
  }
  void SetDoNotTrack(void) {
    flag_word = (flag_word & ~E_CONCURRENT_TRACK_MASK);
  }
  int TrackingRequested(void) {
    return (flag_word & E_CONCURRENT_TRACK_MASK) == E_CONCURRENT_TRACK;
  }
  int TrackingOptional(void) {
    return (flag_word & E_TRACK_REQ_MASK) == E_TRACK_OPTIONAL;
  }

  void SetSuggestedTrackExposureTime(double seconds) {
    suggested_track_exposure = seconds;
  }
  double GetSuggestedTrackExposureTime(void) {
    return suggested_track_exposure;
  }

  void SetTrackCCD(void)
  { flag_word = ((flag_word & ~E_CCD_MASK) | E_TRACK_CCD);}
  void SetMainCCD(void)
  { flag_word = ((flag_word & ~E_CCD_MASK) | E_MAIN_CCD);}
    
  int IsTrackCCD(void) { return (flag_word & E_CCD_MASK) == E_TRACK_CCD; }
  int IsMainCCD(void)  { return (flag_word & E_CCD_MASK) == E_MAIN_CCD; }

  void SetShutterOpen(void)
  { flag_word = ((flag_word & ~E_SHUTTER_MASK) | E_SHUTTER_OPEN); }
  void SetShutterShut(void)
  { flag_word = ((flag_word & ~E_SHUTTER_MASK) | E_SHUTTER_SHUT); }
  int IsShutterShut(void)
  { return (flag_word & E_SHUTTER_MASK) == E_SHUTTER_SHUT; }
  int IsShutterOpen(void)
  { return (flag_word & E_SHUTTER_MASK) == E_SHUTTER_OPEN; }

  void SetCompression(bool do_compress) {
    e_compress = do_compress; }
  bool IsCompression(void) { return e_compress; }

  void SetUSBTraffic(double traffic) {
    e_usbtraffic = traffic; }
  double USBTraffic(void) { return e_usbtraffic; }

  void SetOffset(int offset) {
    e_offset = offset; }
  int GetOffset(void) { return e_offset; }

  void SetGain(int gain) {
    e_readoutgain = gain; }
  int GetGain(void) { return e_readoutgain; }

  void SetReadoutMode(int mode) {
    e_readoutmode = mode; }
  int GetReadoutMode(void) { return e_readoutmode; }

  void SetBinning(int binning) {
    e_binning = binning; }
  int GetBinning(void) { return e_binning; }

  void SetOutputFormat(E_PixelFormat format) {
    e_outputformat = format;
  }
  E_PixelFormat GetOutputFormat(void) { return e_outputformat; }
  
};

void expose_image_local(double exposure_time_seconds,
			exposure_flags &ExposureFlags,
			const char *local_FITS_filename,
			const char *purpose = 0,
			Drifter *drifter = 0);

// Warning: This version of expose_image() provides an Image pointer
// instead of the more normal way of fetching an image (via a disk
// file). This Image will have a few keywords present in the
// associated ImageInfo, but will NOT have the full rich set of
// keywords that is present when the version that creates a file is
// invoked. 
void expose_image(double exposure_time_seconds,
		  Image **NewImage,
		  exposure_flags &ExposureFlags,
		  const char *purpose = 0,
		  Drifter *drifter = 0);

// This command is the same as the previous expose_image() function,
// except that this command *must* be issued on the host machine where
// the camera is.  The resulting image file will be kept on the host
// machine. (This command can also be used from another machine, but
// the resulting image file will not be sent out from the host; it
// will remain in the host machine's filesystem. This may be useful if
// a filename is chosen that is NFS mounted so as to be visible to the
// machine issuing this function.)
void host_expose_image(double exposure_time_seconds,
		       exposure_flags &ExposureFlags,
		       char *host_FITS_filename);

// This form will choose an appropriate filename and return a full
// path to the place where the image was stored. It will only grab an
// image from the sub-image specified. Count lines/pixels starting
// with zero at the bottom and zero along the left edge.  The
// left parameter should be evenly divisible by 3 and the right
// parameter should be one less than a multiple of 3 (binning rules).
char *expose_image(double exposure_time_seconds,
		   exposure_flags &ExposureFlags,
		   const char *purpose = 0,
		   Drifter *drifter = 0);
inline char *expose_image_next(double exposure_time_seconds,
			       exposure_flags &ExposureFlags,
			       const char *purpose = 0,
			       Drifter *drifter = 0) {
  return expose_image(exposure_time_seconds,
		      ExposureFlags,
		      purpose,
		      drifter);
}

void do_qhy_test(void);

// This function returns 1 if it was successful in getting the CCD
// cooler information; otherwise 0 is returned.
// 
// for cooler_flags:
#define COOLER_NO_WAIT 1	// wait for response from cooler
#define COOLER_NO_SEND 2	// don't send message
static const int CCD_COOLER_ON = 0x01; // mode
static const int CCD_COOLER_REGULATING = 0x02; // mode

int CCD_cooler_data(double *ambient_temp,
		    double *ccd_temp,
		    double *cooler_setpoint,
		    int    *cooler_power,
		    double *humidity,
		    int    *mode,
		    int    cooler_flags = 0);
// fd of cooler camera command socket
int camera_socket(void);

// Use this class to send cooler commands to the cooler controller.
class CoolerCommand {
public:
  CoolerCommand(void);
  void SetCoolerOff(void);
  void SetCoolerManual(double PowerLevel); // 0->1.0
  void SetCoolerSetpoint(double TempC);
  int Send(void);		// returns 1 if successful
private:
  int mode;
  double Power;
  double Setpoint;
};

// Use this class to send filter configuration commands to the CCD
// controller. If you do a Send() without issuing any "Set...()"
// commands, then will just do a query to find current filter
// configuration. Otherwise, will change the configuration in the CCD
// server. 
class FilterCommand {
public:
  FilterCommand(void);
  void SetNoFilter(void);
  void SetFixedFilter(Filter filter);
  void SetWheelFilters(int num_filters, const Filter *filters);
  int Send(void);		// returns 1 if successful

  int GetNumFilters(void);	// returns 0..5
  Filter *GetFilters(void);	// delete [] when done
private:
  int JustQuery;
  int NumFilters;
  Filter *installed_filters;
};

#endif
