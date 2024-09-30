/* This may look like C code, but it is really -*-c++-*- */
/*  camera_indi.h -- Implements user view of ST-10XME
 *
 *  Copyright (C) 2024 Mark J. Munkacsy

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
#pragma once

#include "camera_api.h"
#include "astro_indi.h"
#include "blocker_indi.h"
#include <Image.h>
#include <memory>

class Drifter;			// forward declaration

class CAMERA_INDI : public LocalDevice {
public:
  CAMERA_INDI(AstroDevice *device, const char *connection_port);
  ~CAMERA_INDI(void);

  bool CAMERAPresent(void) const { return this->dev->is_connected; }

  // returns 0 on success
  int ExposureStart(double exposure_time_seconds,
		    const char *purpose,
		    exposure_flags &ExposureFlags);
  void WaitForImage(void);
  char *ReceiveImage(exposure_flags &ExposureFlags,
		     const char *fits_filename,
		     const char *purpose);
  

  //char *expose_image(double exposure_time_seconds,
  //		     exposure_flags &ExposureFlags,
  //		     const char *fits_filename,
  //		     const char *purpose,
  //		     Drifter *drifter);

  void DoINDIRegistrations(void);

private:
  void FetchImage(INDI::Property indi_prop);
  void AddKeywords(unique_ptr<Image> &image);
  double GetEGain(long gain_setting, int readoutmode);
  
  exposure_flags user_flags;
  double user_exp_time;
  time_t exposure_start_time;
  std::string user_purpose;

  unique_ptr<Image> new_image;
  
  long requested_location;
  Blocker blob_blocker;
  AstroDevice *dev {nullptr};
public: 
  AstroValueNumber cam_exposure_time{this,"CCD_EXPOSURE", "CCD_EXPOSURE_VALUE"};
  AstroValueNumber cam_frame_x{this,"CCD_FRAME", "X"};
  AstroValueNumber cam_frame_y{this,"CCD_FRAME", "Y"};
  AstroValueNumber cam_frame_width{this,"CCD_FRAME", "WIDTH"};
  AstroValueNumber cam_frame_height{this,"CCD_FRAME", "HEIGHT"};
  AstroValueSwitch cam_type_light{this,"CCD_FRAME_TYPE", "FRAME_LIGHT"};
  AstroValueSwitch cam_type_bias{this,"CCD_FRAME_TYPE", "FRAME_BIAS"};
  AstroValueSwitch cam_type_dark{this,"CCD_FRAME_TYPE", "FRAME_DARK"};
  AstroValueSwitch cam_type_flat{this,"CCD_FRAME_TYPE", "FRAME_FLAT"};
  AstroValueNumber cam_binningx{this,"CCD_BINNING", "HOR_BIN"};
  AstroValueNumber cam_binningy{this,"CCD_BINNING", "VER_BIN"};
  AstroValueSwitch cam_compress{this,"CCD_COMPRESSION", "CCD_COMPRESS"};
  AstroValueSwitch cam_uncompress{this,"CCD_COMPRESSION", "CCD_RAW"};
  AstroValueSwitch cam_frame_reset{this,"CCD_FRAME_RESET", "RESET"};
  AstroValueNumber cam_chipwidth{this,"CCD_INFO", "CCD_MAX_X"};
  AstroValueNumber cam_chipheight{this,"CCD_INFO", "CCD_MAX_Y"};
  AstroValueNumber cam_pixelsize{this,"CCD_INFO", "CCD_PIXEL_SIZE"};
  AstroValueBLOB   cam_blob1{this,"CCD1", "CCD1"};
  AstroValueSwitch cam_debug_enable {this, "DEBUG", "ENABLE"};
  AstroValueSwitch cam_debug_disable {this, "DEBUG", "DISABLE"};
  AstroValueSwitch cam_debug_err   {this,"DEBUG_LEVEL", "DBG_ERROR"};
  AstroValueSwitch cam_debug_warn  {this,"DEBUG_LEVEL", "DBG_WARNING"};
  AstroValueSwitch cam_debug_sess  {this,"DEBUG_LEVEL", "DBG_SESSION"};
  AstroValueSwitch cam_debug_debug {this,"DEBUG_LEVEL", "DBG_DEBUG"};
  AstroValueSwitch cam_log_err   {this,"LOGGING_LEVEL", "LOG_ERROR"};
  AstroValueSwitch cam_log_warn  {this,"LOGGING_LEVEL", "LOG_WARNING"};
  AstroValueSwitch cam_log_sess  {this,"LOGGING_LEVEL", "LOG_SESSION"};
  AstroValueSwitch cam_log_debug {this,"LOGGING_LEVEL", "LOG_DEBUG"};
  AstroValueSwitch cam_log_file  {this,"LOG_OUTPUT", "FILE_DEBUG"};
  AstroValueNumber cam_sim_xres {this,"SIMULATOR_SETTINGS", "SIM_XRES"};
  AstroValueNumber cam_sim_yres {this,"SIMULATOR_SETTINGS", "SIM_YRES"};
  AstroValueNumber cam_sim_xsize {this,"SIMULATOR_SETTINGS", "SIM_XSIZE"}; // pixel size, microns
  AstroValueNumber cam_sim_ysize {this,"SIMULATOR_SETTINGS", "SIM_YSIZE"};
  AstroValueNumber cam_sim_maxval {this,"SIMULATOR_SETTINGS", "SIM_MAXVAL"};
  AstroValueNumber cam_sim_satur {this,"SIMULATOR_SETTINGS", "SIM_SATURATION"};
  AstroValueNumber cam_sim_lim_mag {this,"SIMULATOR_SETTINGS", "SIM_LIMITINGMAG"};
  AstroValueNumber cam_sim_noise {this,"SIMULATOR_SETTINGS", "SIM_NOISE"};
  AstroValueNumber cam_sim_skyglow {this,"SIMULATOR_SETTINGS", "SIM_SKYGLOW"};

  friend void CAMERAPropertyUpdate(INDI::Property property);

};

extern CAMERA_INDI *camera;

