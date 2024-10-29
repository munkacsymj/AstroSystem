/*  camera_api.cc -- Implements user view of camera
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
#include <time.h>		// gmtime()
#include <sys/time.h>		// gettimeofday()
#include <netdb.h>		// gethostbyname()
#include <netinet/in.h>
#include <string.h>		// memset()
#include <stdio.h>
#include <unistd.h>		// exit()
#include <stdlib.h>		// system()
#include <sys/socket.h>		// socket()
#include <arpa/inet.h>		// inet_ntoa()
#include <fitsio.h>
#include <vector>
#include "gen_message.h"
#include "camera_message.h"
#include "FITSMessage.h"
#include "Image.h"		// NextValidImageFilename()
#include "drifter.h"
#include "camera_api.h"
#include "ports.h"
#include "scope_api.h"		// to get CameraPointsAt()
#include "image_notify.h"
#include "image_profile.h"
#include <iostream>
#include <libindi/indiapi.h>
#include <libindi/baseclient.h>
#include "astro_indi.h"
#include "cooler_indi.h"
#include "camera_indi.h"
#include "cfw_indi.h"

static void InitializeAstroClient(void) {
  if (not AstroINDIConnected()) {
    ConnectAstroINDI();
    if (/*extern*/camera) return;
    int tries = 1000; // 1000 * 10msec = 10sec total patience
    do {
      usleep(10000);		// 10 msec
    } while(tries-- > 0 and (camera == nullptr));
    if (camera == nullptr) {
      std::cerr << "connect_to_camera: failed.";
      exit(-2);
    }
    //cooler->DoINDIRegistrations();
  }
}

static time_t ExposureStartTime;

// connect_to_camera() will establish a connection to the camera server
// process running on the camera computer.  It will block for as long
// as necessary to establish the connection. If unable to establish a
// connection (for whatever reason), it will print an error message to
// stderr and will exit.
void connect_to_camera(void) {
  InitializeAstroClient();
}

void disconnect_camera(void) {
  INDIDisconnectINDI();
}

int camera_is_available(void) {
  return camera != nullptr;
}

void update_fits_data(const char *fits_filename, const char *purpose) {
  ImageInfo info(fits_filename);

  // Note: (this is important) this is the point where "north is up"
  // and "rotation angle" meet.  We query the scope for which side of
  // the pier it is on, and set BOTH "north is up" and "rotation
  // angle" from the response.

  info.SetLocalDefaults();
  if (purpose) {
    info.SetPurpose(purpose);
  }

  // Put temperatures into the keyword list (CCD and Ambient)
  if (cooler->TempAvail()) {
    info.SetCCDTemp(cooler->GetCCDTemp());
    // sadly, ambient doesn't seem to be available
  }
  
  // Set Altitude and Azimuth
  ALT_AZ loc_alt_az(ScopePointsAt_altaz());
  info.SetAzEl(loc_alt_az);

  // Calculate and set airmass
  double airmass = loc_alt_az.airmass_of();
  info.SetAirmass(airmass);

  if (dec_axis_is_flipped()) {
    info.SetNorthIsUp(0);
    info.SetRotationAngle(0.0);
  } else {
    info.SetNorthIsUp(1);
    info.SetRotationAngle(M_PI);
  }    
  
  double ha = GetScopeHA();

  if(ha > M_PI) ha -= (M_PI*2.0);
  info.SetHourAngle(ha);
  
  DEC_RA nominal_position = ScopePointsAt();
  long focus_value = scope_focus(0);

  info.SetNominalDecRA(&nominal_position);
  info.SetFocus((double) focus_value);
  info.SetExposureStartTime(JULIAN(ExposureStartTime));
  
  info.WriteFITS();
}


void
do_expose_image(double exposure_time_seconds,
		exposure_flags &ExposureFlags,
		const char *host_FITS_filename,
		const char *purpose,
		Drifter *drifter) {
  // We only distinguish between shutter open and shutter
  // shut. Shutter open translates into a LIGHT exposures, shutter
  // shut translates into a DARK exposure. No other exposure type is
  // used.

  if (ExposureFlags.IsShutterShut()) {
    // Bias or Dark
    if (cfw->HasBlackFilter()) {
      Filter black("Dark");
      ExposureFlags.SetFilter(black);
    }
  }
    
  Filter this_filter = ExposureFlags.FilterRequested();
  int filter_slot = this_filter.PositionOf();
  if (filter_slot >= 0 and cfw)  {
    cfw->MoveFilterWheel(filter_slot, /*block=*/true);
  }

  if (drifter) {
    drifter->ExposureStart(exposure_time_seconds);
  }

  // start the exposure
  fprintf(stderr, "Sending StartExposure command (%.2f sec).\n",
	  exposure_time_seconds);
  ExposureStartTime = time(0);
  if (camera->ExposureStart(exposure_time_seconds,
			    purpose,
			    ExposureFlags) == 0) {
    if (drifter) {
      drifter->ExposureGuide(); // this will block for duration of exposure
    }
    camera->WaitForImage();
    const char *filename = camera->ReceiveImage(ExposureFlags,
						host_FITS_filename,
						purpose);
    Image NewImage(filename);
  
    exposure_flags::E_PixelFormat f = ExposureFlags.GetOutputFormat();
    if (f == exposure_flags::E_float) {
      NewImage.SetImageFormat(FLOAT_IMG);
    } else if (ExposureFlags.GetBinning() == 1 or
	       f == exposure_flags::E_uint16) {
      NewImage.SetImageFormat(USHORT_IMG);
    } else {
      NewImage.SetImageFormat(ULONG_IMG);
    }

    NewImage.WriteFITSAuto(filename, ExposureFlags.IsCompression());
    update_fits_data(filename, purpose);
    NotifyServiceProvider(filename);
  } else {
    // error starting exposure
    std::cerr << "camera_api: error starting exposure\n";
  }
}

char *expose_image(double exposure_time_seconds,
		   exposure_flags &ExposureFlags,
		   const char *purpose,
		   Drifter *drifter) {
  char *next_valid_filename = NextValidImageFilename();

  do_expose_image(exposure_time_seconds,
		  ExposureFlags,
		  next_valid_filename,
		  purpose,
		  drifter);
  return next_valid_filename;
}

void logfile_msg(const char *msg) {
  struct timeval Now;
  (void) gettimeofday(&Now, 0);
  fprintf(stderr, "[%ld.%06ld] %s\n",
	  Now.tv_sec, Now.tv_usec, msg);
}

int // return 0 on error, 1 on success
CCD_cooler_data(double *ambient_temp,
		double *ccd_temp,
		double *cooler_setpoint,
		int    *cooler_power,
		double *humidity,
		int    *mode,
		int    cooler_flags) {
  return cooler->GetCoolerData(ambient_temp,
			      ccd_temp,
			      cooler_setpoint,
			      cooler_power,
			      humidity,
			      mode,
			      cooler_flags);
}

exposure_flags::exposure_flags(const char *profile_name) {
  flag_word = (E_MAIN_CCD | E_SHUTTER_OPEN);
  
  if (profile_name == nullptr) return;

  // fetch flags from image_profiles.json
  ImageProfile profile(profile_name);

  if (profile.IsDefined("offset")) {
    SetOffset(profile.GetInt("offset"));
  }

  if (profile.IsDefined("gain")) {
    SetGain(profile.GetInt("gain"));
  }

  if (profile.IsDefined("mode")) {
    SetReadoutMode(profile.GetInt("mode"));
  }

  if (profile.IsDefined("binning")) {
    SetBinning(profile.GetInt("binning"));
  }

  if (profile.IsDefined("compress")) {
    SetCompression(profile.GetInt("compress"));
  }

  if (profile.IsDefined("usb_traffic")) {
    SetUSBTraffic(profile.GetInt("usb_traffic"));
  }

  if (profile.IsDefined("format")) {
    const char *format = profile.GetChar("format");
    if (strcmp(format, "UI16") == 0) {
      SetOutputFormat(E_uint16);
    } else if (strcmp(format, "UI32") == 0) {
      SetOutputFormat(E_uint32);
    } else if (strcmp(format, "FLOAT") == 0) {
      SetOutputFormat(E_float);
    } else {
      fprintf(stderr, "exposure_flags('%s').format(%s) undefined.\n",
	      profile_name, format);
    }
  }

  if (profile.IsDefined("box_bottom")) {
    subframe.box_bottom = profile.GetInt("box_bottom");
  }

  if (profile.IsDefined("box_top")) {
    subframe.box_top = profile.GetInt("box_top");
  }

  if (profile.IsDefined("box_left")) {
    subframe.box_left = profile.GetInt("box_left");
  }

  if (profile.IsDefined("box_right")) {
    subframe.box_right = profile.GetInt("box_right");
  }
}

