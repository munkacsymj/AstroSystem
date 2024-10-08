/*  ccd_message_handler.cc -- Server handles message sent for camera control
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
#include <unistd.h>		// read()
#include <stdio.h>
#include <string.h>		// strncpy()
#include <fitsio.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/time.h>		// struct timeval
#include <sys/stat.h>		// open()
#include <sys/mman.h>
#include <stdlib.h>
#include <time.h>		// gmtime()
#include <pthread.h>		// mutex
#include <signal.h>
#include <fcntl.h>
#include "gen_message.h"
#include "FITSMessage.h"
#include "camera_message.h"
#include "ccd_server.h"
#include "cooler.h"
#include "ccd_message_handler.h"
#include "StatusMessage.h"
#include <Filter.h>
#include "RequestStatusMessage.h"
#include <qhyccd.h>
#include <system_config.h>

#define USBCFW // or else, #define CFW2CAMERA
//#define CFW2CAMERA

#define OPTIC_BLACK_EDGE 4179

#ifdef USBCFW
#include "usb_cfw.h"
#endif

//***************************************************************
//        Forward References
//****************************************************************
void RefreshCFWPosition(void);
void RefreshCameraStatus(void);
void RefreshCoolerData(void);
void SDKInitResource(void);
void SDKScanCamera(void);
void SDKSetMode(void);
void SDKSetCameraDefaults(void);
void MoveFilterWheel(int position);
void InitializeCameraStatus(void);
void SetFullFrame(void);
  
/****************************************************************/
/*        VARIABLES						*/
/****************************************************************/

static int CCDCameraStatus = CAMERA_IDLE;
static int LastImageSeqNo  = -1;
static int ExposureTimeoutPending = 0;
qhyccd_handle *camhandle = nullptr;
static unsigned char *iBuffer = nullptr;
static CoolerData *cooler_data = nullptr;

enum ExposureState {
  Idle,				// no exposure right now
  ExposureRequested,
  FilterWheelMoving,		// filter wheel is rotating
  ReadyForExposure,		// Okay to start the exposure
  Exposing,			// exposure active
  ReadyForExposureToEnd,	// waiting for camera to end exposure
  ReadyForReadout};	

struct ccd_subframe {
  unsigned int SubframeTop;
  unsigned int SubframeBottom;
  unsigned int SubframeLeft;
  unsigned int SubframeRight;
};

static struct {
  int width;
  int height;
} CCD_Info = { 512, 512 };

enum FITSDepth { BITS_16, BITS_32, BITS_FLOAT };

class ExposureInfo {
public:
  ccd_subframe   SubFrameData;
  double         ExposureTimeSeconds;
  struct timeval StartTime;
  struct timeval EndTime;
  ExposureState  CurrentState;
  double         DesiredUSBTraffic;
  int            DesiredBinning;
  int            DesiredOffset;
  int            DesiredMode;
  FITSDepth      DesiredDepth;	// output file format
  bool           UseCompression; // output file
  int            DesiredCameraGain;
  int            FirstFilterWheelPos;
  int            FilterWheelTgtNum;
  int            DesiredFilterWheelPos;
  int            LastUserExposureID;
  char           last_filename[256];
  char           ExposureFilename[256];
  int            UserExposureIDNumber;
  int            UserSocketNumber;

  struct timeval t0, t1; // actual time (shutter start open,
                         // shutter fully open, shutter start
			 // close, shutter fully closed)

} MainExposure;

struct CCDInfo {
  int           NumberReadModes;             // init
  char          CameraModelName[64];         // init
  uint8_t       CameraFirmwareVersion[64];   // init
  double        CameraCurrenteGain;
  int           CameraCurrentGainSetting;    // refresh
  int           CameraOffsetSetting;         // refresh
  int           CameraReadMode;              // refresh
  double        CameraMinExpSpeeduSec;       // init
  double        CameraMaxExpSpeeduSec;       // init
  double        CameraExpSpeedStepSize;      // init
  int           CameraHasAmpGlowControl;     // init
  int           CurrentUSBSpeed;             // init
  int           CurrentUSBTrafficSetting;    // refresh
  int           CurrentCFWPosition;          // refresh
  bool          CoolerManualMode;	     // init
  int           CoolerPWMCommand;	     // refresh+init [0..255??]
  double        CoolerTempCommand;	     // refresh
  double        CurrentChipTemperature;      // refresh
  double        CurrentCoolerPWM;	     // refresh
  double        Humidity;                    // refresh_cooler
  double        Pressure;                    // refresh_cooler
  int           NumberCFWSlots;              // init
  double        ControlOffsetMin;
  double        ControlOffsetMax;
  double        ControlOffsetStep;
  int           MaxWidth;                    // init
  int           MaxHeight;                   // init
  int           OverscanX, OverscanY;        // init
  int           OverscanW, OverscanH;        // init
  double        ControlGainMin, ControlGainMax, ControlGainStep; //init
  double        OffsetMin, OffsetMax, OffsetStep;                //init
  double        USBTrafficMin, USBTrafficMax, USBTrafficStep;    //init

  bool          cache_invalid;
} CameraData;
  

void LogTag(const char *msg) {
  struct timeval Now;
  (void) gettimeofday(&Now, 0);

  fprintf(stderr, "[%ld.%06ld] %s\n",
	  Now.tv_sec, Now.tv_usec, msg);
}

void printerror( int status);
void FilterTimeout(long user_data);
void ExposureTimeout(long user_data);
void ExposureTimeoutWithLock(long user_data);
void show_error(const char *s, int err);
void send_status_message(int socket_fd, CameraMessage *request=0);
void UpdateCameraStatus(void);
void ProcessAll(void);
void ExposureFinished(void);
void FatalTimeout(void) {
  fprintf(stderr, "Fatal timeout.\n");
  exit(2);
}

// returns "1" if first time is "after" the second time
static inline int after(struct timeval &Now,
			struct timeval &TriggerTime) {
  return Now.tv_sec > TriggerTime.tv_sec ||
    (Now.tv_sec == TriggerTime.tv_sec &&
     Now.tv_usec >= TriggerTime.tv_usec);
}

double delta_t(struct timeval &first,
	       struct timeval &second) {
  struct timeval delta;
  timersub(&second, &first, &delta);
  //fprintf(stderr, "delta_t = (%ld, %ld)\n",
  //	  delta.tv_sec, delta.tv_usec);

  const double answer = delta.tv_sec + (delta.tv_usec/1000000.0);
  return answer;
}

static constexpr double CFW_TICK = 0.1; // 0.1 second ticks to check CFW status
static int filter_timeout_counter = -1;
static constexpr int CFW_MAX_TIMEOUTS = (25/CFW_TICK); // 25-sec maximum
void ScheduleFilterTimeout(bool first_time) {
  static struct timeval filter_time = { (int)(CFW_TICK), (int)(CFW_TICK*1000000.0+0.5) };
  if (first_time) filter_timeout_counter = CFW_MAX_TIMEOUTS;

  LogTag("ScheduleFilterTimeout()");
  SetTimeout(&filter_time,
	     0,			// userdata
	     FilterTimeout);
}
  

static constexpr double EXP_PREWIN = 0.1; // 100msec before scheduled end
static constexpr double EXP_POSTWIN = 5.0; // 5 sec after scheduled end
static constexpr double EXP_TICK = 0.1; // 10x per second

void ScheduleExposureTimeout(void) {
  static int timeout_counter = -1;
  
  struct timeval Now;
  (void) gettimeofday(&Now, 0);
  if(MainExposure.CurrentState == Exposing) {
    // First call for this exposure (at exposure start)
    struct timeval delta_time;
    delta_time.tv_sec = MainExposure.EndTime.tv_sec - Now.tv_sec;
    delta_time.tv_usec = MainExposure.EndTime.tv_usec - Now.tv_usec;
    delta_time.tv_usec -= (1000000*EXP_PREWIN);
    while(delta_time.tv_usec < 0) {
      delta_time.tv_usec += 1000000;
      delta_time.tv_sec -= 1;
    }
    timeout_counter = (EXP_POSTWIN+EXP_PREWIN)/EXP_TICK;
    if (delta_time.tv_sec < 0 ||
	(delta_time.tv_sec == 0 and delta_time.tv_usec < 0.11)) {
      ExposureTimeout(0);
    } else {
      fprintf(stderr, "Scheduling exposure timeout in %ld seconds (+%ld usec)\n",
	      delta_time.tv_sec, delta_time.tv_usec);
      SetTimeout(&delta_time,
		 0,			// userdata
		 ExposureTimeoutWithLock);
    }
  } else if (MainExposure.CurrentState == ReadyForExposureToEnd) {
    // Subsequent calls near expected exposure end time
    if (--timeout_counter <= 0) {
      fprintf(stderr, "Exposure timeout. Camera still not ready.\n");
      return;
    }
    struct timeval delta_time = { (int)(EXP_TICK), (int)(EXP_TICK*1000000.0+0.5) };
    SetTimeout(&delta_time, 0, ExposureTimeoutWithLock);
  } else {
    fprintf(stderr,
	    "ScheduleExposureTimeout(): CurrentState mismatch: %d\n",
	    MainExposure.CurrentState);
  }
}

void FilterTimeout(long user_data) {
  LogTag("FilterTimeout()");
  RefreshCFWPosition();		// update the current hardware state
				// of the CFW

  if (MainExposure.FilterWheelTgtNum == 0 and
      CameraData.CurrentCFWPosition == MainExposure.FirstFilterWheelPos) {
    fprintf(stderr, "Current CFW position(1) = %d ",
	    CameraData.CurrentCFWPosition);

    LogTag("CFW in staging position.");
    MainExposure.FilterWheelTgtNum = 1;
    sleep(1);			// Let filter wheel stop moving
    ProcessAll();
  } else if (MainExposure.FilterWheelTgtNum == 1 and
      CameraData.CurrentCFWPosition == MainExposure.DesiredFilterWheelPos) {
    fprintf(stderr, "Current CFW position(f) = %d ",
	    CameraData.CurrentCFWPosition);

    LogTag("CFW in desired position.");
    MainExposure.FilterWheelTgtNum = 0;
    sleep(1);			// Let filter wheel stop moving
    MainExposure.CurrentState = ReadyForExposure;
    ProcessAll();
  } else {
    if(--filter_timeout_counter <= 0) {
      // CFW timeout. Bad news.
      fprintf(stderr, "CFW timeout.\n");
      FatalTimeout();
    } else {
      ScheduleFilterTimeout(false); // not the first time for this motion
    }
  }
}

void ExposureTimeoutWithLock(long user_data) {
  GetCameraLock();
  fprintf(stderr, "Obtained ExposureTimeoutWithLock camera lock.\n");
  ExposureTimeout(user_data);
  ReleaseCameraLock();
}
  
void ExposureTimeout(long user_data) {
  LogTag("ExposureTimeout()");
  struct timeval Now;
  (void) gettimeofday(&Now, 0);

  MainExposure.CurrentState = ReadyForExposureToEnd;

  uint32_t remaining_time = GetQHYCCDExposureRemaining(camhandle);
  fprintf(stderr, "    Now = %ld, %ld; end_time = %ld, %ld; camera reports %d remaining.\n",
	  Now.tv_sec, Now.tv_usec,
	  MainExposure.EndTime.tv_sec, MainExposure.EndTime.tv_usec,
	  remaining_time);

  // It seems that remaining_time is probably in units of percent
  // (0..100) of original exposure time, but this seems to break down
  // for short exposure times (< 5sec)
  if (remaining_time <= 0) {
    ExposureFinished();
  } else {
    // ERROR: Should do something with remaining_time here
    ScheduleExposureTimeout();
  }
}

void UpdateCameraStatus(void) {
  RefreshCameraStatus();
}

void UpdateCoolerStatus(void) {
  RefreshCoolerData();
}

void ExposureFinished(void) {
  LogTag("ExposureFinished()");
  // Exposure time is finished. Start the
  // image readout process.
  if(MainExposure.CurrentState != ReadyForExposureToEnd) {
    fprintf(stderr, "ExposureFinished: Initial state not 'ReadyToEnd', instead: %d\n",
	    MainExposure.CurrentState);
  }
  // Tentatively put state to "Waitingforshutter". 
  MainExposure.CurrentState = ReadyForReadout;

  // Set t2
  (void) gettimeofday(&MainExposure.t1, 0);
  
  ProcessAll();
}

void StartExposure(void) {
  int result;

  //****************
  // BINNING (always external)
  //****************
  result = SetQHYCCDBinMode(camhandle, 1, 1); // always bin external
  if (result != QHYCCD_SUCCESS) {
    show_error("SetQHYCCDBinMode()", result);
    return;
  } else {
    fprintf(stderr, "SetQHYCCDBinMode(1)\n");
  }

  //****************
  // SUBFRAME
  //****************
  int subframe_width = MainExposure.SubFrameData.SubframeRight -
    MainExposure.SubFrameData.SubframeLeft+1;
  int subframe_height = MainExposure.SubFrameData.SubframeTop -
    MainExposure.SubFrameData.SubframeBottom+1;
  const int left_edge = MainExposure.SubFrameData.SubframeLeft+CameraData.OverscanW;
  const bool do_fullframe = (MainExposure.SubFrameData.SubframeLeft   == 0 and
			     MainExposure.SubFrameData.SubframeTop    == 0 and
			     MainExposure.SubFrameData.SubframeBottom == 0 and
			     MainExposure.SubFrameData.SubframeRight  == 0);

  if (left_edge+subframe_width > CameraData.MaxWidth) {
    subframe_width = CameraData.MaxWidth - left_edge;
  }

  if (MainExposure.SubFrameData.SubframeBottom + subframe_height > OPTIC_BLACK_EDGE) {
    subframe_height = OPTIC_BLACK_EDGE - MainExposure.SubFrameData.SubframeBottom;
  }
  
  if (do_fullframe or
      subframe_width <= 0 or
      subframe_height <= 0 or
      MainExposure.SubFrameData.SubframeLeft < 0 or
      MainExposure.SubFrameData.SubframeTop < 0 or
      MainExposure.SubFrameData.SubframeBottom < 0 or
      MainExposure.SubFrameData.SubframeRight < 0) {
    SetFullFrame();
  } else {
    fprintf(stderr, "subframe_left = %u\n", MainExposure.SubFrameData.SubframeLeft);
    fprintf(stderr, "subframe_top = %u\n", MainExposure.SubFrameData.SubframeTop);
    fprintf(stderr, "subframe_bottom = %u\n", MainExposure.SubFrameData.SubframeBottom);
    fprintf(stderr, "subframe_right = %u\n", MainExposure.SubFrameData.SubframeRight);
  
    result = SetQHYCCDResolution(camhandle,
				 left_edge,
				 MainExposure.SubFrameData.SubframeBottom,
				 subframe_width, subframe_height);
    if (result != QHYCCD_SUCCESS) {
      show_error("SetQHYCCDResolution()", result);
      return;
    } else {
      fprintf(stderr, "SetQHYCCDResolution()\n");
    }
  }

  //****************
  // EXPOSURE TIME (set in usecs)
  //****************
  result = SetQHYCCDParam(camhandle, CONTROL_EXPOSURE,
			  MainExposure.ExposureTimeSeconds * 1000000);
  if (result != QHYCCD_SUCCESS) {
    show_error("SetQHYCCDParam(EXPOSURE_TIME)", result);
    return;
  } else {
    fprintf(stderr, "SetQHYCCD Exposure Time(%.0lf) [usec]\n",
	    MainExposure.ExposureTimeSeconds * 1000000);
  }

#if 0
  //****************
  // USB TRAFFIC
  //****************
  result = SetQHYCCDParam(camhandle, CONTROL_USBTRAFFIC,
			  MainExposure.DesiredUSBTraffic);
  if (result != QHYCCD_SUCCESS) {
    show_error("SetQHYCCDParam(CONTROL_USBTRAFFIC)", result);
    return;
  } else {
    fprintf(stderr, "SetQHYCCDParam(USBTRAFFIC, %.0lf)\n",
	    MainExposure.DesiredUSBTraffic);
  }
#endif

  //****************
  // OFFSET
  //****************
  result = SetQHYCCDParam(camhandle, CONTROL_OFFSET, MainExposure.DesiredOffset);
  if (result != QHYCCD_SUCCESS) {
    show_error("SetQHYCCDParam(CONTROL_OFFSET)", result);
    return;
  } else {
    fprintf(stderr, "SetQHYCCDParam(CONTROL_OFFSET)\n");
  }

  //****************
  // READOUT MODE
  //****************
  result = SetQHYCCDReadMode(camhandle, MainExposure.DesiredMode);
  if (result != QHYCCD_SUCCESS) {
    show_error("SetQHYCCDReadMode()", result);
    return;
  } else {
    fprintf(stderr, "SetQHYCCDReadMode()\n");
  }

  //****************
  // CAMERA GAIN
  //****************
  result = SetQHYCCDParam(camhandle, CONTROL_GAIN, MainExposure.DesiredCameraGain);
  if (result != QHYCCD_SUCCESS) {
    show_error("SetQHYCCDParam(CONTROL_GAIN)", result);
    return;
  } else {
    fprintf(stderr, "SetQHYCCDParam(CONTROL_GAIN);\n");
  }

  LogTag("StartExposure()");

  MainExposure.CurrentState = Exposing;

  // Set t0
  (void) gettimeofday(&MainExposure.t0, 0);

  double readback = GetQHYCCDParam(camhandle, CONTROL_EXPOSURE);
  fprintf(stderr, "    Camera reports exposure time of %lf\n", readback);

  result = ExpQHYCCDSingleFrame(camhandle);
  if (result != QHYCCD_SUCCESS) {
    show_error("ExpQHYCCDSingleFrame()", result);
    return;
  }
  uint32_t remaining_time = GetQHYCCDExposureRemaining(camhandle);
  fprintf(stderr, "    Camera reports %d remaining.\n", remaining_time);

  LogTag("Return from ExpQHYCCDSingleFrame()");
  (void) gettimeofday(&MainExposure.StartTime, 0);
  {
    long exposure_secs = (long) MainExposure.ExposureTimeSeconds;
    long exposure_microsecs =
      (long) (0.5 + 1000000.0 * (MainExposure.ExposureTimeSeconds - (double)
				 exposure_secs));
    // ERROR: Need to back up the EndTime by the pre-check duration
    MainExposure.EndTime.tv_sec =
      MainExposure.StartTime.tv_sec + exposure_secs;
    MainExposure.EndTime.tv_usec =
      MainExposure.StartTime.tv_usec + exposure_microsecs;
    if(MainExposure.EndTime.tv_usec >= 1000000) {
      MainExposure.EndTime.tv_usec -= 1000000;
      MainExposure.EndTime.tv_sec += 1;
    }
  }

  ScheduleExposureTimeout();
}

// Fast and simple when there's no binning
void Readout16to16(uint8_t *buffer, int width, int height, fitsfile *fptr) {
  uint16_t framebuffer[6280];
  int status = 0;
#if 0
  int explainer = 32;
#endif

  /* now store the image */
  for(int row=0; row<height; row++) {
    uint16_t *d = framebuffer;
    const uint8_t *s = buffer + row*width*2;
    long k = width;
    while(k--) {
      const uint16_t v1 = *s;
      const uint16_t v2 = *(s+1);
      *d = (v2*256 + v1);
#if 0
      if (explainer > 0) {
	explainer--;
	fprintf(stderr, "v1 = %08x, v2 = %08x, *d = %08x\n",
		(uint32_t) v1, (uint32_t) v2, (uint32_t) *d);
      }
#endif
      s += 2;
      d++;
    }
  
    fits_write_img_usht(fptr,	      // file
			0,	      // group
			1+ row*width, // first element
			width,        // number of elements
			framebuffer,  // points to row
			&status);     // status
    //fprintf(stderr, "completed Readout16to16().\n");
  }
}

// The uint8_t "buffer" must have some extra space allocated to it. It
// is wise to allocate "bin" extra columns and "bin" extra rows in
// order to avoid segmentation faults. This might provide some garbage
// in the final row/column, but that's what you get if your image
// height/width aren't an exact multiple of the binning number.
void ReadoutBinTo16(uint8_t *buffer, int bin, int width, int height, fitsfile *fptr) {
  uint16_t framebuffer[6280];
  int status = 0;
  int num_saturated = 0;
  const int tgt_w = width/bin;
  const int tgt_h = height/bin;

  // This counts *output* row (i.e., row in the FITS file, not the camera)
  for(int row=0; row<tgt_h; row++) {
    // This, again, counts *output* column
    for(int col=0; col<tgt_w; col++) {
      int overflow = 0;
      uint32_t tgt = 0;
      for (int b=0; b<bin; b++) { // "b" adjusts the row
	const uint8_t *s = buffer + ((row*bin+b)*width + col*bin)*2;
	for (int bb=0; bb<bin; bb++) {
	  const uint16_t v1 = *(s+bb*2);
	  const uint16_t v2 = *(s+bb*2+1);
	  uint16_t v = (v2*256+v1);
	  overflow += (v > 65530);
	  tgt += v;
	}
      }
      uint16_t constexpr SATURATED=65535;
      if (overflow || tgt>(SATURATED)) {
	tgt = SATURATED;
	num_saturated++;
      }
      framebuffer[col] = (uint16_t) tgt;
    }

    fits_write_img_usht(fptr,	      // file
			0,	      // group
			1+ row*tgt_w, // first element
			tgt_w,	      // number of elements
			framebuffer,  // points to row
			&status);     // status
  }
  fprintf(stderr, "completed ReadoutBinTo16() with %d saturated.\n",
	  num_saturated);
}
  
void ReadoutBinTo32(uint8_t *buffer, int bin, int width, int height, fitsfile *fptr) {
  uint32_t framebuffer[6280];
  int status = 0;
  int num_saturated = 0;
  const int tgt_w = width/bin;
  const int tgt_h = height/bin;

  // This counts *output* row (i.e., row in the FITS file, not the camera)
  for(int row=0; row<tgt_h; row++) {
    // This, again, counts *output* column
    for(int col=0; col<tgt_w; col++) {
      int overflow = 0;
      uint32_t tgt = 0;
      for (int b=0; b<bin; b++) { // "b" adjusts the row
	const uint8_t *s = buffer + ((row*bin+b)*width + col*bin)*2;
	for (int bb=0; bb<bin; bb++) {
	  const uint16_t v1 = *(s+bb*2);
	  const uint16_t v2 = *(s+bb*2+1);
	  uint16_t v = (v2*256+v1);
	  overflow += (v > 65530);
	  tgt += v;
	}
      }
      uint16_t constexpr SATURATED=65535;
      if (overflow) {
	tgt = SATURATED*bin*bin;
	num_saturated++;
      }
      framebuffer[col] = tgt;
    }

    fits_write_img_uint(fptr,	      // file
			0,	      // group
			1+ row*tgt_w, // first element
			tgt_w,	      // number of elements
			framebuffer,  // points to row
			&status);     // status
  }
  fprintf(stderr, "completed ReadoutBinTo32() with %d saturated.\n",
	  num_saturated);
}
  
void ReadoutBinToFloat(uint8_t *buffer, int bin, int width, int height, fitsfile *fptr) {
  float framebuffer[6280];
  int status = 0;
  int num_saturated = 0;
  const int tgt_w = width/bin;
  const int tgt_h = height/bin;

  // This counts *output* row (i.e., row in the FITS file, not the camera)
  for(int row=0; row<tgt_h; row++) {
    // This, again, counts *output* column
    for(int col=0; col<tgt_w; col++) {
      int overflow = 0;
      uint32_t tgt = 0;
      for (int b=0; b<bin; b++) { // "b" adjusts the row
	const uint8_t *s = buffer + ((row*bin+b)*width + col*bin)*2;
	for (int bb=0; bb<bin; bb++) {
	  const uint16_t v1 = *(s+bb*2);
	  const uint16_t v2 = *(s+bb*2+1);
	  uint16_t v = (v2*256+v1);
	  overflow += (v > 65530);
	  tgt += v;
	}
      }
      uint16_t constexpr SATURATED=65535;
      if (overflow) {
	tgt = SATURATED*bin*bin;
	num_saturated++;
      }
      framebuffer[col] = (float) tgt;
    }

    fits_write_img_flt(fptr,	      // file
		       0,	      // group
		       1+ tgt_w,      // first element
		       tgt_w,	      // number of elements
		       framebuffer,   // points to row
		       &status);      // status
  }
  fprintf(stderr, "completed ReadoutBinTo32() with %d saturated.\n",
	  num_saturated);
}
  
void ReadoutExposure(void) {
  int result;
  LogTag("ReadoutExposure()");

  MainExposure.CurrentState = Idle;

  // warning: the SubFrameData counts from 0 at the bottom to "height"
  // at the top of the CCD.  This is different from the SBIG
  // convention that that "top" of the CCD is at 0.

  uint32_t w, h, bpp, channels;
  fprintf(stderr, "iBuffer = %p, \n", iBuffer);
  result = GetQHYCCDSingleFrame(camhandle, &w, &h, &bpp, &channels, iBuffer);

  if (bpp != 16) {
    fprintf(stderr, "GetQHYCCDSingleFrame(): wrong pixel depth: %d\n", bpp);
    return;
  }

  {
    char msg_buffer[256];
    sprintf(msg_buffer, "Readout finished, w = %d, h = %d", w, h);
    LogTag(msg_buffer);
  }

  fitsfile *fptr;
  long naxes[2] = { w/MainExposure.DesiredBinning,
		    h/MainExposure.DesiredBinning };
  int status = 0;
  const int in_memory_FITS_file = (MainExposure.ExposureFilename[0] == '-' &&
				   MainExposure.ExposureFilename[1] == 0);


  // Create a FITS file, either in memory or on the filesystem
  void *mem_file;
  size_t mem_size = 2880*200;
  const char *local_filename_clean = "/tmp/localfile.fits";
  char local_filename[128];
  if(in_memory_FITS_file) {
    // in memory (this is the normal case)
    sprintf(local_filename, "!%s%s",
	    local_filename_clean,
	    (MainExposure.UseCompression ? "[compress]" : ""));
    if(fits_create_file(&fptr, local_filename, &status)) {
      printerror(status);
      return;
    }
  } else {
    // filesystem
    char FITSFilename[300];

    // unlink the file if it previously existed
    (void) unlink(MainExposure.ExposureFilename);
    fprintf(stderr, "Unlinking '%s'\n", MainExposure.ExposureFilename);
    sprintf(FITSFilename, "!%s%s", MainExposure.ExposureFilename,
	    (MainExposure.UseCompression ? "[compress]" : ""));
    if(fits_create_file(&fptr,
			FITSFilename,
			&status)) {
      printerror(status);
      return;
    }
  }
  /* create an image */
  
  int fits_format = USHORT_IMG;
  if (MainExposure.DesiredBinning == 1 or MainExposure.DesiredDepth == BITS_16) {
    fits_format = USHORT_IMG;
  } else if (MainExposure.DesiredDepth == BITS_32) {
    fits_format = ULONG_IMG;
  } else if (MainExposure.DesiredDepth == BITS_FLOAT) {
    fits_format = FLOAT_IMG;
  } else {
    fprintf(stderr, "ReadoutExposure: invalid pixel format: %d\n",
	    MainExposure.DesiredDepth);
  }

  if(fits_create_img(fptr, fits_format, 2, naxes, &status)) {
    printerror(status);
    return;
  }
  if(fits_write_date(fptr, &status)) {
    printerror(status);
    return;
  }

  double data_max = 65530.0;
  if (MainExposure.DesiredBinning == 1) {
    Readout16to16(iBuffer, w, h, fptr);
  } else if (MainExposure.DesiredDepth == BITS_16) {
    ReadoutBinTo16(iBuffer, MainExposure.DesiredBinning, w, h, fptr);
  } else if (MainExposure.DesiredDepth == BITS_32) {
    ReadoutBinTo32(iBuffer, MainExposure.DesiredBinning, w, h, fptr);
    data_max *= (MainExposure.DesiredBinning*
		 MainExposure.DesiredBinning);
  } else if (MainExposure.DesiredDepth == BITS_FLOAT) {
    ReadoutBinToFloat(iBuffer, MainExposure.DesiredBinning, w, h, fptr);
    data_max *= (MainExposure.DesiredBinning*
		 MainExposure.DesiredBinning);
  } else {
    fprintf(stderr, "ReadoutExposure: invalid pixel format: %d\n",
	    MainExposure.DesiredDepth);
  }

  // Add as much FITS header data as we can
  char datamax_comment[] = "[ADU] Largest linear ADU value";
  if(fits_update_key(fptr,
		     TDOUBLE,
		     "DATAMAX",
		     &data_max,
		     datamax_comment,
		     &status)) {
    printerror(status);
    return;
  }

  char exposure_comment[] = "[Sec] Shutter open time";
  if(fits_update_key(fptr,
		     TDOUBLE,
		     "EXPOSURE",
		     &MainExposure.ExposureTimeSeconds,
		     exposure_comment,
		     &status)) {
    printerror(status);
    return;
  }

  SystemConfig config;
  double pixel_scale = config.PixelScale()*MainExposure.DesiredBinning; // arcsec/pixel for SCT
  char cdelt1_comment[] = "[arcsec/pixel] X axis pixel size";
  char cdelt2_comment[] = "[arcsec/pixel] Y axis pixel size";
  if(fits_update_key(fptr,
		     TDOUBLE,
		     "CDELT1",
		     &pixel_scale,
		     cdelt1_comment,
		     &status)) {
    printerror(status);
    return;
  }
  if(fits_update_key(fptr,
		     TDOUBLE,
		     "CDELT2",
		     &pixel_scale,
		     cdelt2_comment,
		     &status)) {
    printerror(status);
    return;
  }

  char t1_comment[] = "[seconds] actual exposure time";
  if(MainExposure.t0.tv_sec != 0 &&
     MainExposure.t1.tv_sec != 0) {
    double t1 = delta_t(MainExposure.t0, MainExposure.t1);
    if(fits_update_key(fptr,
		       TDOUBLE,
		       "EXP_T1",
		       &t1,
		       t1_comment,
		       &status)) {
      printerror(status);
      return;
    }
  }

  // Add the FILTER keyword to the FITS header
  {
    if(FilterWheelSlots() > 0) {
      const char *filter_name = InstalledFilters()[CameraData.CurrentCFWPosition].NameOf();
      //const char *filter_name = filter_slot_info[CameraData.CurrentCFWPosition].NameOf();
      fprintf(stderr, "current_filter_position = %d: %s\n",
	      CameraData.CurrentCFWPosition, filter_name);
      
      char filter_comment[] = "Filter used";
      if(filter_name) {
	if(fits_update_key(fptr,
			   TSTRING,
			   "FILTER",
			   (void *) filter_name,
			   filter_comment,
			   &status)) {
	  printerror(status);
	}
      }
    }
  }
    
  {
    const time_t start_time = MainExposure.StartTime.tv_sec;
    struct tm *gt = gmtime(&start_time);
    char date_time_string[FLEN_VALUE];

    if(fits_time2str(1900 + gt->tm_year,
		     1    + gt->tm_mon,
		     gt->tm_mday,
		     gt->tm_hour,
		     gt->tm_min,
		     (double) gt->tm_sec,
		     1,
		     date_time_string,
		     &status)) {
      printerror(status);
      return;
    }

    char date_comment[] = "Exposure start time";
    if(fits_update_key(fptr,
		       TSTRING,
		       "DATE-OBS",
		       date_time_string,
		       date_comment,
		       &status)) {
      printerror(status);
      return;
    }
  }

  // Camera Gain, Readout Mode, Binning
  // Subframe corner location
  {
    unsigned long gain = MainExposure.DesiredCameraGain;
    char cgain_comment[] = "Camera Gain Setting";
    if(fits_update_key(fptr,
		       TULONG,
		       "CAMGAIN",
		       &gain,
		       cgain_comment,
		       &status)) {
      printerror(status);
      return;
    }

    unsigned long mode = MainExposure.DesiredMode;
    char mode_comment[] = "Camera Readout Mode";
    if(fits_update_key(fptr,
		       TULONG,
		       "READMODE",
		       &mode,
		       mode_comment,
		       &status)) {
      printerror(status);
      return;
    }

    // egain is system gain
    double egain = 0.0;
    switch(mode) {
    case 0:
      if (gain < 30) egain = 1.58-0.03667*gain;
      else if (gain < 65) egain = 0.8658-0.01286*gain;
      else egain = 0.06705-0.00057*gain;
      break;
    case 1:
      egain = 1.002-0.0098*gain;
      break;
    case 2:
      egain = 1.543-0.0143*gain;
      break;
    case 3:
      egain = 1.628-0.0153*gain;
      break;
    }
    char gain_comment[] = "[e/ADU] CCD Gain";
    if(fits_update_key(fptr,
		       TDOUBLE,
		       "EGAIN",
		       &egain,
		       gain_comment,
		       &status)) {
      printerror(status);
      return;
    }

    unsigned long offset = MainExposure.DesiredOffset;
    char offset_comment[] = "Camera Offset";
    if(fits_update_key(fptr,
		       TULONG,
		       "OFFSET",
		       &offset,
		       offset_comment,
		       &status)) {
      printerror(status);
      return;
    }

    unsigned long binning = MainExposure.DesiredBinning;
    char bin_comment[] = "Binning (NxN)";
    if(fits_update_key(fptr,
		       TULONG,
		       "BINNING",
		       &binning,
		       bin_comment,
		       &status)) {
      printerror(status);
      return;
    }

    unsigned long ul_corner_x = MainExposure.SubFrameData.SubframeLeft;
    unsigned long ul_corner_y = MainExposure.SubFrameData.SubframeBottom;
    char x_comment[] = "[pixel] Subframe upper left corner X";
    char y_comment[] = "[pixel] Subframe upper left corner Y";
    if(fits_update_key(fptr,
		       TULONG,
		       "FRAMEX",
		       &ul_corner_x,
		       x_comment,
		       &status)) {
      printerror(status);
      return;
    }
    if(fits_update_key(fptr,
		       TULONG,
		       "FRAMEY",
		       &ul_corner_y,
		       y_comment,
		       &status)) {
      printerror(status);
      return;
    }

    SystemConfig config;
    char telescope_comment[] = " TELESCOPE";
    char camera_comment[] = " CAMERA";
    char efl_comment[] = "[mm] Effective Focal Length";
    double efl = config.EffectiveFocalLength();
    char telescope[64];
    char camera[64];

    if(fits_update_key(fptr,
		       TDOUBLE,
		       "FOCALLEN",
		       &efl,
		       efl_comment,
		       &status)) {
      printerror(status);
      return;
    }
    strncpy(telescope, config.Telescope().c_str(), sizeof(telescope));
    strncpy(camera, config.Camera().c_str(), sizeof(camera));
    if(fits_update_key(fptr,
		       TSTRING,
		       "CAMERA",
		       camera,
		       camera_comment,
		       &status)) {
      printerror(status);
      return;
    }
    if(fits_update_key(fptr,
		       TSTRING,
		       "TELESCOP",
		       telescope,
		       telescope_comment,
		       &status)) {
      printerror(status);
      return;
    }
  }
		       
  // flush() will trigger the actual compression to be done
  if(fits_flush_file(fptr, &status)) {
    printerror(status);
    status = 0;
  }
  
  // must close the file to flush internal cfitsio buffers to the
  // "real" file in memory
  if(fits_close_file(fptr, &status)) {
    printerror(status);
    return;
  }
  if(in_memory_FITS_file) {
    // in memory
    // find out the filesize
    off_t fits_filesize;

    struct stat st;
    if (stat(local_filename_clean, &st) == 0) {
      fits_filesize = st.st_size;
    }

    int fd = open(local_filename_clean, O_RDONLY);
    if (fd < 0) {
      perror("Cannot open FITS file:");
    } else {
      void *mem_file =
	mmap(nullptr, fits_filesize, PROT_READ, MAP_PRIVATE, fd, 0);

      FITSMessage response_message(MainExposure.UserSocketNumber,
				   fits_filesize,
				   mem_file);
      fprintf(stderr, "Sending FITSMessage, length = %ld\n",
	      fits_filesize);
      response_message.send();

      if (munmap(mem_file, fits_filesize)) {
	perror("error unmapping FITS file:");
      }
      close(fd);
    }
  } else {
    // in the filesystem
    if(fits_close_file(fptr, &status)) {
      printerror(status);
      return;
    }
    // Send the user a status message
    LastImageSeqNo++;
    strcpy(MainExposure.last_filename, MainExposure.ExposureFilename);
    MainExposure.LastUserExposureID = MainExposure.UserExposureIDNumber;

    send_status_message(MainExposure.UserSocketNumber);
  }
}

#ifdef USBCFW
void CompleteCFWInit(void) {
  if (USBCFWInitializationComplete()) return;
  CameraData.NumberCFWSlots = USBCFWInitializeEnd(); // will block for a while
  RefreshCFWPosition();
}
#endif

void ProcessAll(void) {
  // This decides what to do next
  switch (MainExposure.CurrentState) {
  case Idle:
    return;
  case ExposureRequested:
#ifdef USBCFW
    CompleteCFWInit(); // block; wait for init to complete
#endif
    if (MainExposure.DesiredFilterWheelPos != CameraData.CurrentCFWPosition) {
#ifdef USBCFW
      if (MainExposure.FilterWheelTgtNum == 0) {
	MainExposure.FirstFilterWheelPos =
	  (MainExposure.DesiredFilterWheelPos ?
	   MainExposure.DesiredFilterWheelPos-1 :
	   MainExposure.DesiredFilterWheelPos+1);
      USBMoveFilterWheel(MainExposure.FirstFilterWheelPos);
      } else {
	USBMoveFilterWheel(MainExposure.DesiredFilterWheelPos);
      }
	
      ScheduleFilterTimeout(true); // starting a new move command
#else
      MoveFilterWheel(MainExposure.DesiredFilterWheelPos);
#endif
    } else {
      MainExposure.CurrentState = ReadyForExposure;
      return ProcessAll();
    }
    break;
  case FilterWheelMoving:
    // How to get here???
    break;
  case ReadyForExposure:
    fprintf(stderr, "Starting main exposure.\n");
    StartExposure();
    break;
  case Exposing:
  case ReadyForExposureToEnd:
    fprintf(stderr, "Logic error: ProcessAll() w/state==Exposing\n");
    break;
  case ReadyForReadout:
    fprintf(stderr, "Starting main CCD readout.\n");
    ReadoutExposure();
    break;
  default:
    fprintf(stderr, "Logic error: invalid camera status state: %d\n",
	    MainExposure.CurrentState);
  }
}

/****************************************************************/
/*        PROCEDURES						*/
/****************************************************************/

void printerror( int status)
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

static const char *camera_names[] =
    {"ST-4", "ST-4X", "ST-5", "ST-6", "ST-7",
	"ST-8", "ST-5C", "TCE", "ST-237", "ST-K", "ST-9", "STV", "ST-10",
	"ST-1K"
    };


void initialize_ccd(void) {
  SDKInitResource();		// initialize the SDK
  SDKScanCamera();		// Find the camera and get its handle
  SDKSetMode();			// Set camera to single-frame mode
  SDKSetCameraDefaults();
  
  CameraLockInit();
  
  cooler_data = GetCoolerData();
  cooler_data->CoolerTempCommand = 0.0;
  cooler_data->CoolerPWMCommand = 0;
  cooler_data->CoolerModeDesired = COOLER_MAN;
  InitCooler();

  GetCameraLock();
  InitializeCameraStatus();

  MainExposure.CurrentState = Idle;
  MainExposure.LastUserExposureID = 0;
  MainExposure.last_filename[0] = 0;

  uint32_t mem_len = GetQHYCCDMemLength(camhandle);
  iBuffer = (unsigned char *) malloc(mem_len);
  fprintf(stderr, "iBuffer = %p\n", iBuffer);
  memset(iBuffer, 0, mem_len);
  printf("Allocated memory for frame: %d [uchar].\n", mem_len);
  ReleaseCameraLock();

#ifdef TEST_COOLER
  sleep(60*7);
  cooler_data->CoolerTempCommand = 4.0;

  sleep(60*8);
  cooler_data->CoolerTempCommand = -10.0;

  sleep(60*10);
  cooler_data->CoolerTempCommand = 10.0;
#endif // TEST_COOLER

  SetQHYCCDLogLevel(1); // LOG_TRACE
}
  
//****************************************************************
//        Outbound Status Message
//****************************************************************
void send_status_message(int socket_fd, CameraMessage *request) {
  int result;
  double seconds_remaining = 0.0;

  UpdateCameraStatus();		// refresh Camera & cooler Status

  if(MainExposure.CurrentState == Exposing) {
    struct timeval Now;
    (void) gettimeofday(&Now, 0);

    const int delta_secs = MainExposure.EndTime.tv_sec - Now.tv_sec;
    const int delta_usecs = MainExposure.EndTime.tv_usec - Now.tv_usec;

    seconds_remaining = (double) delta_secs +  delta_usecs / 1000000.0;

  }

  CameraMessage outbound(socket_fd, CMD_STATUS);
  if (CameraData.CurrentCoolerPWM == 0.0) {
    outbound.SetKeywordValue("COOLER_MODE", "OFF");
  } else {
    outbound.SetKeywordValue("COOLER_MODE",
			     CameraData.CoolerManualMode ? "MANUAL" : "SETPOINT");
  }

  fprintf(stderr, "CurrentChipTemp: %.1lf\n",
	  CameraData.CurrentChipTemperature);
  fprintf(stderr, "CurrentCoolerPWM: %.1lf\n",
	  CameraData.CurrentCoolerPWM);
  fprintf(stderr, "CoolerTempCommand: %.1lf\n",
	  CameraData.CoolerTempCommand);
  outbound.SetCoolerTemp(CameraData.CurrentChipTemperature);
  outbound.SetAmbientTemp(99.9);
  outbound.SetCoolerPower(CameraData.CurrentCoolerPWM/256.0);
  outbound.SetHumidity(CameraData.Humidity);
  outbound.SetKeywordValue("SETPOINT", to_string(CameraData.CoolerTempCommand));
  if (CCDCameraStatus == CAMERA_IDLE) {
    outbound.SetKeywordValue("CAMERA_STATUS", "IDLE");
  } else if (CCDCameraStatus == CAMERA_IO_BUSY) {
    outbound.SetKeywordValue("CAMERA_STATUS", "READOUT");
  } else if (CCDCameraStatus == CAMERA_SHUTTER_OPEN) {
    outbound.SetKeywordValue("CAMERA_STATUS", "EXPOSING");
  }

  if (request) {
    outbound.SetUniqueID(request->GetUniqueID());
  }
  outbound.send();
}

void handle_expose_message(CameraMessage *msg, int socket_fd) {
  fprintf(stderr, "Received expose message.\n");

  MainExposure.DesiredBinning = msg->GetBinning();
  MainExposure.ExposureTimeSeconds = msg->GetExposureTime();
  MainExposure.DesiredDepth = BITS_16;
  if (msg->PixelFormatAvail()) {
    int output_pixel_format = msg->GetPixelFormat();
    switch(output_pixel_format) {
    case PIXEL_UINT16:
      MainExposure.DesiredDepth = BITS_16;
      break;
    case PIXEL_UINT32:
      MainExposure.DesiredDepth = BITS_32;
      break;
    case PIXEL_FLOAT:
      MainExposure.DesiredDepth = BITS_FLOAT;
      break;
    default:
      fprintf(stderr, "handle_expose_message(): bad PixelFormat: %d\n",
	      output_pixel_format);
    }
  }

  MainExposure.DesiredUSBTraffic = 0;
  if (msg->USBTrafficAvail()) {
    MainExposure.DesiredUSBTraffic = msg->GetUSBTraffic();
  }

  MainExposure.UseCompression = msg->CompressAvail() and msg->GetCompress();
  MainExposure.DesiredMode = 0;
  if (msg->CameraModeAvail()) {
    MainExposure.DesiredMode = msg->GetCameraMode();
  }
  MainExposure.DesiredCameraGain = 0;
  if (msg->CameraGainAvail()) {
    MainExposure.DesiredCameraGain = msg->GetCameraGain();
  }
  MainExposure.DesiredOffset = 5;
  if (msg->CameraOffsetAvail()) {
    MainExposure.DesiredOffset = msg->GetOffset();
  }
  
  strncpy(MainExposure.ExposureFilename,
	  msg->GetLocalImageName().c_str(),
	  sizeof(MainExposure.ExposureFilename));
  fprintf(stderr, "Set output filename to '%s'\n",
	  MainExposure.ExposureFilename);

  MainExposure.UserSocketNumber = socket_fd;
  MainExposure.t0.tv_sec = 0;
  MainExposure.t0.tv_usec = 0;
  MainExposure.t1.tv_sec = 0;
  MainExposure.t1.tv_usec = 0;

  msg->GetSubFrameData(&MainExposure.SubFrameData.SubframeBottom,
		       &MainExposure.SubFrameData.SubframeTop,
		       &MainExposure.SubFrameData.SubframeLeft,
		       &MainExposure.SubFrameData.SubframeRight);

  // Handle the filter
  //fprintf(stderr, "filter_info_available = %d\n", filter_info_available);
  //fprintf(stderr, "num_filters = %d\n", num_filters);

  // only bother studying the filter flags if we've got a filter wheel.
  if(FilterWheelSlots() > 1) {
    char requested_filter_letter[2] { 0, 0 };
    requested_filter_letter[0] = (msg->FilterAvail() ? msg->GetFilterLetter() : 'V');
    if (msg->ShutterAvail() and msg->GetShutterOpen() == false) {
      requested_filter_letter[0] = 'D'; // dark
    }
    Filter requested_filter(requested_filter_letter);

    int desired_filter_pos = requested_filter.PositionOf();
    fprintf(stderr, "Exposure msg->desired_filter %s in CFW slot %d\n", 
    	    requested_filter_letter, desired_filter_pos);

    if (desired_filter_pos < 0) {
      fprintf(stderr, "Invalid filter request: '%s'", requested_filter_letter);
      desired_filter_pos = 0;
    }

    //fprintf(stderr, "Filter is in position %d\n", desired_filter_pos);
    MainExposure.DesiredFilterWheelPos = desired_filter_pos;
  }

  GetCameraLock();
  UpdateCameraStatus();		// get current CFW info
  MainExposure.CurrentState = ExposureRequested;
  ProcessAll();
  ReleaseCameraLock();
}

//****************************************************************
//        Inbound cooler message
//****************************************************************
void handle_cooler_message(CameraMessage *msg, int socket_fd) {
  if (msg->IsQuery()) {
    GetCameraLock();
    send_status_message(socket_fd, msg);
    ReleaseCameraLock();
    return;
  }

  const string mode = (msg->CoolerModeAvail() ?
		       msg->GetCoolerMode() : "");
  fprintf(stderr, "handle_cooler_message: mode = %s\n",
	  mode.c_str());
  
  if (mode == "") {
    fprintf(stderr, "Cooler Mode missing from CameraMessage.\n");
  } else if (mode == "OFF") {
    // cooler off
    CameraData.CoolerManualMode = true;
    CameraData.CoolerPWMCommand = 0;
    cooler_data->CoolerModeDesired = COOLER_OFF;
    
  } else if (mode == "SETPOINT") {
    // Temperature regulation mode. Setup setpoint.
    CameraData.CoolerManualMode = false;
    CameraData.CoolerTempCommand = msg->GetCoolerSetpoint();
    cooler_data->CoolerModeDesired = COOLER_AUTO;
    cooler_data->CoolerTempCommand = msg->GetCoolerSetpoint();
    
  } else if (mode == "MANUAL") {
    // Power-level override mode. Setup manual power level.
    CameraData.CoolerManualMode = true;
    // power-level arrives as a double in the range 0..0.99
    // convert to range 0..256
    CameraData.CoolerPWMCommand = 255.0 * msg->GetCoolerPower();
    fprintf(stderr, "Set ManualPWM: %lf\n",
	    (double) CameraData.CoolerPWMCommand);
    cooler_data->CoolerModeDesired = COOLER_MAN;
    cooler_data->CoolerPWMCommand = CameraData.CoolerPWMCommand;
  }
  
  // no response message at all
}

//****************************************************************
//        Send outbound filter configuration message
//****************************************************************
void send_filter_data_message(int socket_fd) {
#if 1
  fprintf(stderr, "ERROR: send_filter_data_message() invoked, but deprecated.\n");
#else
  if(filter_info_available == 0) ReadFilterData();
  if(!filter_info_available) {
    num_filters = 0; // ensures that outbound message will make sense
  }

  CameraMessage outbound(socket_fd, CMD_FILTER_CONFIG);
  for (int n=0; n<num_filters; n++) {
    char filter_name[2];
    filter_name[0] = filter_slot_info[n].NameOf()[0];
    filter_name[1] = 0;
    outbound.SetKeywordValue(string("FILTER_")+ to_string(n),
			     string(filter_name));
  }
  
  outbound.send();
#endif
}
    
void handle_filter_set_message(CameraMessage *msg, int socket_fd) {
#if 1
  fprintf(stderr, "ERROR: handle_filter_set_message() invoked, but deprecated.\n");
#else
  // inbound message with data about filters
  for(int n=0; n<9; n++) {
    string keyword = string("FILTER_") + to_string(n);
    if (msg->KeywordPresent(keyword)) {
      filter_slot_info[n] = Filter(msg->GetValueString(keyword).c_str());
    } else {
      num_filters = n;
      break;
    }
  }

  if(num_filters < 2) {
    current_filter_position = 0;
  }

  filter_info_available = 1;

  WriteFilterData();
  send_filter_data_message(socket_fd);
#endif
}

int handle_message(int socket_fd) {
  GenMessage *new_message = GenMessage::ReceiveMessage(socket_fd);

  if(new_message == 0) return -1;

  char buffer[132];
  sprintf(buffer, "    msg ID = %d (%d bytes)",
          new_message->MessageID(), new_message->MessageSize());
  LogTag(buffer);

  switch(new_message->MessageID()) {
  case CameraMessageID:
    {
      CameraMessage *cm = new CameraMessage(new_message);
      const int cmd = cm->GetCommand();
      if (cmd == CMD_COOLER) {
	// We're being told what to do with the cooler.
	handle_cooler_message(cm, socket_fd);
      } else if (cmd == CMD_FILTER_CONFIG) {
	if (cm->IsQuery()) {
	  send_filter_data_message(socket_fd);
	} else {
	  // Someone giving us info on installed filters
	  handle_filter_set_message(cm, socket_fd);
	}
      } else if (cmd == CMD_STATUS) {
	// Someone wants status. We're always ready to provide status!
	GetCameraLock();
	send_status_message(socket_fd, cm);
	ReleaseCameraLock();
      } else if (cmd == CMD_EXPOSE) {
	handle_expose_message(cm, socket_fd);
      } else if (cmd == CMD_SHUTDOWN) {
	fprintf(stderr, "CMD_SHUTDOWN not yet implemented.\n");
      } else {
	fprintf(stderr, "ccd_message_handler: unrecognized CameraMessage command: %d\n",
		cmd);
      }
      delete cm;
    }
    break;

  case StatusMessageID:
  case FITSMessageID:
  default:
    // We aren't allowed to receive a status message, only to
    // originate one.
    fprintf(stderr, "scope_server: bad inbound message type\n");
  }

  // All done with the message. Kill it!
  delete new_message;
  LogTag("Finished with message.");
  return 0;			// zero means success
}

void MoveFilterWheel(int position) {
  int result;

  if(position < 0 || position > CameraData.NumberCFWSlots-1) {
    fprintf(stderr, "scope_server: invalid filter wheel index = %d\n",
	    position);
    return;
  }
  fprintf(stderr, "Moving filter wheel to position %d\n", position);

  char position_char[2]{0};
  position_char[0] = '0' + position;
  result = SendOrder2QHYCCDCFW(camhandle, position_char, 1);
  if (result != QHYCCD_SUCCESS) {
    fprintf(stderr, "CFW Move() command failed.\n");
  } else {
    ScheduleFilterTimeout(true); // starting a new move command
  }
}

void MoveFilterWheelAndWait(int position) {
  int result;

  fprintf(stderr, "Moving filter wheel to position %d\n", position);

  char position_char[2]{0};
  position_char[0] = '0' + position;
  result = SendOrder2QHYCCDCFW(camhandle, position_char, 1);
  if (result != QHYCCD_SUCCESS) {
    fprintf(stderr, "CFW Move() command failed.\n");
  } else {
    for (int t=0; t<30; t++) {
      char current_pos[64];
      int ret = GetQHYCCDCFWStatus(camhandle, current_pos);
      if (ret != QHYCCD_SUCCESS) {
	fprintf(stderr, "GetCFWStatus: error response.\n");
	sleep(1);
	continue;
      }
      CameraData.CurrentCFWPosition = current_pos[0] - '0'; // 0..15
      if (CameraData.CurrentCFWPosition == position) {
	fprintf(stderr, "Position match.\n");
	break;
      } else {
	fprintf(stderr, "Current position mismatch (%d vs %d.\n",
		position, CameraData.CurrentCFWPosition);
	sleep(1);
	if (t == 15) {
	  result = SendOrder2QHYCCDCFW(camhandle, position_char, 1);
	  fprintf(stderr, "Resent command.\n");
	}
      }
    }
  }
}

void PrintSDKVersion(void) {
  uint32_t year, month, day, subday;
  int ret = GetQHYCCDSDKVersion(&year, &month, &day, &subday);
  if (ret == QHYCCD_SUCCESS) {
    fprintf(stderr, "SDK Version: %d-%d-%d,%d\n",
	    year, month, day, subday);
  } else {
    fprintf(stderr, "Get SDK version failed.\n");
  }
}

void SDKInitResource(void) {
  int ret = InitQHYCCDResource();
  if (ret == QHYCCD_SUCCESS) {
    fprintf(stderr, "InitQHYCCDResource() completed okay.\n");
  } else {
    fprintf(stderr, "InitQHYCCDResource() failed.\n");
  }
}

void SDKReleaseResource(void) {
}

void SDKScanCamera(void) {
  int num = ScanQHYCCD();
  fprintf(stderr, "Found %d camera(s).\n", num);
  if (num == 0) {
    fprintf(stderr, "No camera found. Give up.\n");
    exit(3);
  }
  if (num > 1) {
    fprintf(stderr, "Multiple cameras found. Give up.\n");
    exit(3);
  }

  char id[32];
  int ret = GetQHYCCDId(0, id);
  if (ret == QHYCCD_SUCCESS) {
    fprintf(stderr, "GetQHYCCDId() returned %s\n", id);
    ret = GetQHYCCDModel(id, CameraData.CameraModelName);
    if (ret == QHYCCD_SUCCESS) {
      fprintf(stderr, "GetQHYCCDModel() returned %s\n", CameraData.CameraModelName);
    } else {
      fprintf(stderr, "GetQHYCCDModel() failed.\n");
    }
  } else {
    fprintf(stderr, "GetQHYCCDId() failed.\n");
  }
  
  camhandle = OpenQHYCCD(id);
  if (camhandle != nullptr) {
    fprintf(stderr, "OpenQHYCCD() successful.\n");
  } else {
    fprintf(stderr, "OpenQHYCCD() failed.\n");
  }
}

void InitializeCameraStatus(void) {
  CameraData.cache_invalid = true;

  int ret;

  //        Cooler
  {
#if 0
    ret = SetQHYCCDParam(camhandle, CONTROL_COOLER, 30.0);
    if (ret != QHYCCD_SUCCESS) {
      fprintf(stderr, "SetQHYCCDParam(CONTROL_COOLER) failed.\n");
    }
    ret = SetQHYCCDParam(camhandle, CONTROL_MANULPWM, 3.0);
    if (ret != QHYCCD_SUCCESS) {
      fprintf(stderr, "SetQHYCCDParam(CONTROL_MANUALPWM) failed.\n");
    }
#endif
  
#ifdef USBCFW
  MainExposure.FilterWheelTgtNum = 0;
  USBCFWInitializeStart();
  CameraData.NumberCFWSlots = 7;
#else
  //        Filter Wheel
  {
    ret = IsQHYCCDControlAvailable(camhandle, CONTROL_CFWPORT);
    
    sleep(26);
    MoveFilterWheelAndWait(2);
    //int ret2 = IsQHYCCDCFWPlugged(camhandle);
    if (ret == QHYCCD_SUCCESS) { // and ret2 == QHYCCD_SUCCESS) {
      // Good. We assume that the CFW is connected!
      double max_filter_count = GetQHYCCDParam(camhandle, CONTROL_CFWSLOTSNUM);
      fprintf(stderr, "First try, max_filter_count = %lf\n", max_filter_count);
      if (max_filter_count > 16) {
	usleep(500000);
	max_filter_count = GetQHYCCDParam(camhandle, CONTROL_CFWSLOTSNUM);
	fprintf(stderr, "Second try, max_filter_count = %lf\n", max_filter_count);
	fprintf(stderr, "CFWSlots: worked on 2nd try.\n");
      }
      if (max_filter_count > 16) {
	usleep(500000);
	max_filter_count = GetQHYCCDParam(camhandle, CONTROL_CFWSLOTSNUM);
	fprintf(stderr, "Third try, max_filter_count = %lf\n", max_filter_count);
	fprintf(stderr, "CFWSlots: worked on 3rd try.\n");
	fprintf(stderr, "Camera can support CFW, but no filters are present.\n");
	CameraData.NumberCFWSlots = 0;
      } else {
	CameraData.NumberCFWSlots = (int) (0.5 + max_filter_count);
	fprintf(stderr, "Number of CFW Slots = %d\n",
		CameraData.NumberCFWSlots);
      }
    } else {
      fprintf(stderr, "No CFW found. Status = %d\n", ret);
      //sleep (1);
      //int ret3 = IsQHYCCDCFWPlugged(camhandle);
      //fprintf(stderr, "Second try: %d\n", ret3);
    }
  }
#endif	

    double cmd_temp = GetQHYCCDParam(camhandle, CONTROL_COOLER);
    fprintf(stderr, "Readback of commanded cooler temp = %lf\n", cmd_temp);
    double cmd_pwm = GetQHYCCDParam(camhandle, CONTROL_MANULPWM);
    fprintf(stderr, "Readback of commanded cooler PWM = %lf\n", cmd_pwm);

    CameraData.CoolerManualMode = true;
    CameraData.CoolerPWMCommand = 0;
    CameraData.CoolerTempCommand = 30.0;

    CoolerData *cooler = GetCoolerData();
    cooler->CoolerModeDesired = COOLER_AUTO;
    cooler->CoolerTempCommand = 10.0;
    cooler->CoolerPWMCommand = 3;

  }

  //        ReadModes
  uint32_t num_modes = 0;
  ret = GetQHYCCDNumberOfReadModes(camhandle, &num_modes);
  if (ret == QHYCCD_SUCCESS) {
    static bool mode_list_printed = false;
    if (mode_list_printed == false) {
      mode_list_printed = true;
      fprintf(stderr, "Camera has %d modes:\n", num_modes);
      for(unsigned int i=0; i<num_modes; i++) {
	char modename[64];
	ret = GetQHYCCDReadModeName(camhandle, i, modename);
	if (ret == QHYCCD_SUCCESS) {
	  fprintf(stderr, "   Mode %d = %s\n",
		  i, modename);
	} else {
	  fprintf(stderr, "Fetch of name of mode %d failed.\n", i);
	}
      }
    } // end for all modes
  } else {
    fprintf(stderr, "GetNumberOfReadModes() failed.\n");
  }
  
  //        ChipSize
  {
    double chip_w, chip_h, pixel_w, pixel_h;
    uint32_t bpp, image_w, image_h;
    ret = GetQHYCCDChipInfo(camhandle,
			    &chip_w, &chip_h,
			    &image_w, &image_h,
			    &pixel_w, &pixel_h,
			    &bpp);
    if (ret == QHYCCD_SUCCESS) {
      CameraData.MaxWidth = image_w;
      CameraData.MaxHeight = image_h;
      fprintf(stderr, "Camera chip size = %lf(w) x %lf(h)\n", chip_w, chip_h);
      fprintf(stderr, "Camera pixel size = %.3lf x %.3lf\n", pixel_w, pixel_h);
      fprintf(stderr, "Image size = %d(w) x %d(h)\n", image_w, image_h);
    } else {
      fprintf(stderr, "GetQHYCCDChipInfo() failed.\n");
    }
  }

  //        Overscan Region
  {
    uint32_t x, y, width, height;
    ret = GetQHYCCDOverScanArea(camhandle, &x, &y, &width, &height);
    if (ret == QHYCCD_SUCCESS) {
      CameraData.OverscanX = x;
      CameraData.OverscanY = y;
      CameraData.OverscanW = width;
      CameraData.OverscanH = height;
      fprintf(stderr, "Overscan area starts at %d(x), %d(y)\n", x, y);
      fprintf(stderr, "  Overscan area size = %d(w) x %d(h)\n", width, height);
    } else {
      fprintf(stderr, "GetQHYCCDOverScanArea() failed.\n");
    }
  }

  //        Firmware Version
  {
    ret = GetQHYCCDFWVersion(camhandle, CameraData.CameraFirmwareVersion);
    if (ret == QHYCCD_SUCCESS) {
      fprintf(stderr, "Camera firmware version = %s\n", CameraData.CameraFirmwareVersion);
    } else {
      fprintf(stderr, "GetQHYCCDFWVersion() failed.\n");
    }
  }

  //        Gain Settings
  {
    ret = GetQHYCCDParamMinMaxStep(camhandle, CONTROL_GAIN,
				   &CameraData.ControlGainMin,
				   &CameraData.ControlGainMax,
				   &CameraData.ControlGainStep);
    if (ret == QHYCCD_SUCCESS) {
      fprintf(stderr, "Camera control gain setting min = %.1lf, max = %.1lf, step = %.1lf\n",
	      CameraData.ControlGainMin,
	      CameraData.ControlGainMax,
	      CameraData.ControlGainStep);
    } else {
      fprintf(stderr, "GetQHYCCDParamMinMaxStep(CONTROL_GAIN) failed.\n");
    }
  }

  //        Cooler Settings
  {
    double pwm_min, pwm_max, pwm_step;
    ret = GetQHYCCDParamMinMaxStep(camhandle, CONTROL_MANULPWM,
				   &pwm_min, &pwm_max, &pwm_step);
    if (ret == QHYCCD_SUCCESS) {
      fprintf(stderr, "Camera cooler PWM setting min = %.1lf, max = %.1lf, step = %.1lf\n",
	      pwm_min, pwm_max, pwm_step);
    } else {
      fprintf(stderr, "GetQHYCCDParamMinMaxStep(CONTROL_MANULPWM) failed.\n");
    }
  }

  //        Offset Settings
  {
    ret = GetQHYCCDParamMinMaxStep(camhandle, CONTROL_OFFSET,
				   &CameraData.ControlOffsetMin,
				   &CameraData.ControlOffsetMax,
				   &CameraData.ControlOffsetStep);
    if (ret == QHYCCD_SUCCESS) {
      fprintf(stderr, "Camera offset setting min = %.1lf, max = %.1lf, step = %.1lf\n",
	      CameraData.ControlOffsetMin,
	      CameraData.ControlOffsetMax,
	      CameraData.ControlOffsetStep);
    } else {
      fprintf(stderr, "GetQHYCCDParamMinMaxStep(CONTROL_OFFSET) failed.\n");
    }
  }

  //        USB Traffic Settings
  {
    ret = GetQHYCCDParamMinMaxStep(camhandle, CONTROL_USBTRAFFIC,
				   &CameraData.USBTrafficMin,
				   &CameraData.USBTrafficMax,
				   &CameraData.USBTrafficStep);
    if (ret == QHYCCD_SUCCESS) {
      fprintf(stderr, "Camera USB Traffic setting min = %.1lf, max = %.1lf, step = %.1lf\n",
	      CameraData.USBTrafficMin,
	      CameraData.USBTrafficMax,
	      CameraData.USBTrafficStep);
    } else {
      fprintf(stderr, "GetQHYCCDParamMinMaxStep(CONTROL_USBTRAFFIC) failed.\n");
    }
  }

  //        Amplifier Glow Control
  {
    ret = IsQHYCCDControlAvailable(camhandle, CONTROL_AMPV);
    CameraData.CameraHasAmpGlowControl = (ret == QHYCCD_SUCCESS);
    fprintf(stderr, "Camera has amplifier glow control: %s\n",
	    CameraData.CameraHasAmpGlowControl ? "true" : "false");
  }

  //        Exposure Times
  {
    double min, max, step;
    ret = GetQHYCCDParamMinMaxStep(camhandle, CONTROL_EXPOSURE, &min, &max, &step);
    if (ret == QHYCCD_SUCCESS) {
      CameraData.CameraMinExpSpeeduSec = min;
      CameraData.CameraMaxExpSpeeduSec = max;
      CameraData.CameraExpSpeedStepSize = step;
      fprintf(stderr, "Camera exposure times min = %.6lf, max = %.2lf, step = %.6lf\n",
	      min/1000000.0, max/1000000.0, step/1000000.0);
    } else {
      fprintf(stderr, "GetQHYCCDParamMinMaxStep(CONTROL_EXPOSURE) failed.\n");
    }
  }

  //        USB Speed
  {
    ret = IsQHYCCDControlAvailable(camhandle, CONTROL_SPEED);
    if (ret == QHYCCD_SUCCESS) {
      double usb_speed = GetQHYCCDParam(camhandle, CONTROL_SPEED);
      fprintf(stderr, "Camera USB speed = %lf\n", usb_speed);
      CameraData.CurrentUSBSpeed = (long) (0.5 + usb_speed);
    } else {
      fprintf(stderr, "Camera USB speed control = false.\n");
    }
  }

  CameraData.cache_invalid = true;
  RefreshCameraStatus();
}
	    
void RefreshCoolerData(void) {
  // Cooler
  CameraData.CurrentChipTemperature = GetQHYCCDParam(camhandle, CONTROL_CURTEMP);
  CameraData.CurrentCoolerPWM = GetQHYCCDParam(camhandle, CONTROL_CURPWM);
  fprintf(stderr, "Current chip temp = %lf, current cooler PWM = %lf\n",
	  CameraData.CurrentChipTemperature,
	  CameraData.CurrentCoolerPWM);
  
  //        Humidy/Pressure
  {
    double humidity, pressure;
    int ret = GetQHYCCDPressure(camhandle, &pressure);
    if (ret == QHYCCD_SUCCESS) {
      ret = GetQHYCCDHumidity(camhandle, &humidity);
      if (ret == QHYCCD_SUCCESS) {
	fprintf(stderr, "Camera chamber pressure = %.1lf mbar, humidity = %lf\n",
		pressure, humidity);
	CameraData.Humidity = humidity;
	CameraData.Pressure = pressure;
      } else {
	fprintf(stderr, "GetQHYCCDHumidity() failed.\n");
      }
    } else {
      fprintf(stderr, "GetQHYCCDPressure() failed.\n");
    }
  }

}

void RefreshCFWPosition(void) {
#ifdef USBCFW
  CameraData.CurrentCFWPosition = USBCFWCurrentPosition();
#else
  char current_pos[64];
  int ret = GetQHYCCDCFWStatus(camhandle, current_pos);
  CameraData.CurrentCFWPosition = current_pos[0] - '0'; // 0..15
#endif
}

void RefreshCameraStatus(void) {
  //if (CameraData.cache_invalid == false) return;

  //CameraData.cache_invalid = false;

  int ret;

  // Control Gain
  CameraData.CameraCurrentGainSetting = GetQHYCCDParam(camhandle, CONTROL_GAIN);
  // Offset
  CameraData.CameraOffsetSetting = GetQHYCCDParam(camhandle, CONTROL_OFFSET);
  // USB Traffic Setting
  CameraData.CurrentUSBTrafficSetting = GetQHYCCDParam(camhandle, CONTROL_USBTRAFFIC);
  // Read Mode
  {
    uint32_t read_mode;
    ret = GetQHYCCDReadMode(camhandle, &read_mode);
    if (ret == QHYCCD_SUCCESS) {
      CameraData.CameraReadMode = read_mode;
    } else {
      fprintf(stderr, "GetQHYCCDReadMode() failed.\n");
    }
  }
  // FilterWheel
  RefreshCFWPosition();
  // Cooler
  RefreshCoolerData();
}

#define SINGLE_FRAME_MODE 0x00

void SDKSetReadMode(int mode_number) {
  int ret = SetQHYCCDReadMode(camhandle, 0);
  if (ret == QHYCCD_SUCCESS) {
    fprintf(stderr, "Set camera to mode 0: success.\n");
  } else {
    fprintf(stderr, "SetReadMode(0) failed.\n");
  }
}

void SDKSetMode(void) {
  int ret = SetQHYCCDStreamMode(camhandle, SINGLE_FRAME_MODE);
  if (ret == QHYCCD_SUCCESS) {
    fprintf(stderr, "Mode successfully set to SingleFrameMode.\n");
  } else {
    fprintf(stderr, "SetQHYCCDStreamMode() failed.\n");
  }

  ret = InitQHYCCD(camhandle);
  if (ret == QHYCCD_SUCCESS) {
    fprintf(stderr, "Camera init completed: success.\n");
  } else {
    fprintf(stderr, "Camera init failed.\n");
  }

}

struct {
  const char *msg;
  int  err_val;
} qhy_error_codes[] = {
		       { "QHYCCD_PCIE",  9 },
		       { "QHYCCD_WINPCAP", 8 },
		       { "QHYCCD_QGIGAE", 7 },
		       { "QHYCCD_USBSYNC", 6 },
		       { "QHYCCD_USBASYNC", 5 },
		       { "QHYCCD_COLOR", 4 },
		       { "QHYCCD_MONO", 3 },
		       { "QHYCCD_COOL", 2 },
		       { "QHYCCD_NOTCOOL", 1 },
		       { "QHYCCD_SUCCESS", 0 },
		       { "QHYCCD_ERROR", -1 },
		       { "QHYCCD_ERROR_NO_DEVICE", -2 },
		       { "QHYCCD_ERROR", -3 },
		       { "QHYCCD_ERROR_SETPARAMS", -4 },
		       { "QHYCCD_ERROR_GETPARAMS", -5 },
		       { "QHYCCD_ERROR_EXPOSING", -6 },
		       { "QHYCCD_ERROR_EXPFAILED", -7 },
		       { "QHYCCD_ERROR_GETTINGDATA", -8 },
		       { "QHYCCD_ERROR_GETTINGFAILED", -9 },
		       { "QHYCCD_ERROR_INITCAMERA", -10 },
		       { "QHYCCD_ERROR_RELEASERESOURCE", -11 },
		       { "QHYCCD_ERROR_INITRESOURCE", -12 },
		       { "QHYCCD_ERROR_NO_MATCH_CAMERA", -13 },
		       { "QHYCCD_ERROR_OPENCAM", -14 },
		       { "QHYCCD_ERROR_INITCLASS", -15 },
		       { "QHYCCD_ERROR_SETRES", -16 },
		       { "QHYCCD_ERROR_USBTRAFFIC", -17 },
		       { "QHYCCD_ERROR_USBSPEED", -18 },
		       { "QHYCCD_ERROR_SETEXPOSE", -19 },
		       { "QHYCCD_ERROR_SETGAIN", -20 },
		       { "QHYCCD_ERROR_SETRED", -21 },
		       { "QHYCCD_ERROR_SETBLUE", -22 },
		       { "QHYCCD_ERROR_EVTCMOS", -23 },
		       { "QHYCCD_ERROR_EVTUSB", -24 },
		       { "QHYCCD_ERROR_25", -25 },
};
void show_error(const char *s, int err) {
  const char *code = nullptr;

  for (int i=0; i<sizeof(qhy_error_codes)/sizeof(qhy_error_codes[0]); i++) {
    if (qhy_error_codes[i].err_val == err) {
      code = qhy_error_codes[i].msg;
      break;
    }
  }

  if (code == nullptr) code = "<not available>";
  
  fprintf(stderr, "ERROR: %s [%d: %s]\n",
	  s, err, code);
}

void SDKSetCameraDefaults(void) {
  int result;

  //****************
  // BINNING (default to 1)
  //****************
  result = SetQHYCCDBinMode(camhandle, 1, 1); // always bin external
  if (result != QHYCCD_SUCCESS) {
    show_error("SetQHYCCDBinMode()", result);
    return;
  } else {
    fprintf(stderr, "Bin = 1\n");
  }

  //****************
  // USB TRAFFIC
  //****************
  result = SetQHYCCDParam(camhandle, CONTROL_USBTRAFFIC, 0);
  if (result != QHYCCD_SUCCESS) {
    show_error("SetQHY USB Traffic()", result);
    return;
  } else {
    fprintf(stderr, "USB Traffic = 0\n");
  }

  //****************
  // OFFSET
  //****************
  result = SetQHYCCDParam(camhandle, CONTROL_OFFSET, 5);
  if (result != QHYCCD_SUCCESS) {
    show_error("SetQHY Offset()", result);
    return;
  } else {
    fprintf(stderr, "Offset = 5\n");
  }
}

void SetFullFrame(void) {
  fprintf(stderr, "SetFullFrame(%d, %d, %d, %d)\n",
	  CameraData.OverscanW,
	  0,
	  CameraData.MaxWidth - CameraData.OverscanW,
	  OPTIC_BLACK_EDGE);
	  
  int result = SetQHYCCDResolution(camhandle,
				   CameraData.OverscanW,
				   0,
				   CameraData.MaxWidth - CameraData.OverscanW,
				   OPTIC_BLACK_EDGE);
  if (result != QHYCCD_SUCCESS) {
    show_error("FullFrame:SetQHYCCDResolution()", result);
    return;
  } else {
    fprintf(stderr, "FullFrame:SetQHYCCDResolution()\n");
  }
}
  
double GetCurrentChipTemp(void) {
  RefreshCoolerData();
  return CameraData.CurrentChipTemperature;
}

double GetCurrentCoolerPWM(void) {
  // WARNING: missing call here to RefreshCoolerData()
  return CameraData.CurrentCoolerPWM;
}

// Pair of functions to mediate cross-thread access to the camera
static pthread_mutex_t camera_mutex;
void GetCameraLock(void) {
  if (pthread_mutex_lock(&camera_mutex)) {
    perror("pthread_mutex_lock(camera): ");
  }
  fprintf(stderr, "Camera Lock successful.\n");
}

void ReleaseCameraLock(void) {
  if (pthread_mutex_unlock(&camera_mutex)) {
    perror("pthread_mutex_unlock(camera): ");
  }
  fprintf(stderr, "Camera Lock released.\n");
}

void CameraLockInit(void) {
  if (pthread_mutex_init(&camera_mutex,
			 nullptr)) { // pthread_mutex_attr
    perror("pthread_mutex_init(camera): ");
  }
}
