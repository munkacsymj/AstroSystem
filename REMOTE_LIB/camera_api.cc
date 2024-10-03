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

#ifdef INDI
#error INDI env variable is set but NATIVE camera_api.cc being compiled.
#endif

using namespace std;

#define COMM_UNINITIALIZED -2
static int comm_socket = COMM_UNINITIALIZED; // file descriptor of socket with
				// scope server
int camera_socket(void) {
  if(comm_socket == COMM_UNINITIALIZED) connect_to_camera();

  return comm_socket;
}

static time_t ExposureStartTime;

// trial_connect_to_camera() will establish a connection to the camera server
// process running on the camera computer.  It will block for as long
// as necessary to establish the connection. If unable to establish a
// connection (for whatever reason), it will print an error message to
// stderr and will return -1; otherwise a +1 will be returned (success).
int trial_connect_to_camera(void) {
  struct hostent *jellybean;
  struct sockaddr_in my_address;

  memset(&my_address, 0, sizeof(my_address));
  jellybean = gethostbyname(CAMERA_HOST); // defined in ports.h
  if(jellybean == 0) {
    herror("Cannot lookup jellybean host name:");
    return -1;
  } else {
    my_address.sin_addr = *((struct in_addr *)(jellybean->h_addr_list[0]));
    my_address.sin_port = htons(CAMERA_PORT); // port number, see ports.h
    my_address.sin_family = AF_INET;
    fprintf(stderr, "Connecting to %s for camera\n",
	    inet_ntoa(my_address.sin_addr));
  }

  comm_socket = socket(PF_INET, SOCK_STREAM, 0);
  if(comm_socket < 0) {
    perror("Error creating camera socket");
    return -1;
  }

  if(connect(comm_socket,
	     (struct sockaddr *) &my_address,
	     sizeof(my_address)) < 0) {
    perror("Error connecting to camera socket");
    return -1;
  }
  return 1; // success!
}

// camera_is_available() returns 1 if a connection can be established;
// a 0 if something goes wrong
int camera_is_available(void) {
  return (trial_connect_to_camera() < 0 ? 0 : 1);
}

void disconnect_camera(void) {
  ; // noop for NATIVE interface
}

// connect_to_camera() will establish a connection to the camera server
// process running on the camera computer.  It will block for as long
// as necessary to establish the connection. If unable to establish a
// connection (for whatever reason), it will print an error message to
// stderr and will exit.
void connect_to_camera(void) {
  if(trial_connect_to_camera() < 0) exit(-2);
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
  {
    double ambient_temp;
    double ccd_temp;
    double cooler_setpoint;
    int cooler_power;
    double humidity;
    int mode;

    if(CCD_cooler_data(&ambient_temp,
		       &ccd_temp,
		       &cooler_setpoint,
		       &cooler_power,
		       &humidity,
		       &mode)) {
      info.SetCCDTemp(ccd_temp);
      info.SetAmbientTemp(ambient_temp);
    }
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

// This command (host_expose_image) is the same as the previous
// expose_image() function, except that this command *must* be issued
// on the host machine where the camera is.  The resulting image file
// will be kept on the host machine. (This command can also be used
// from another machine, but the resulting image file will not be sent
// out from the host; it will remain in the host machine's
// filesystem. This may be useful if a filename is chosen that is NFS
// mounted so as to be visible to the machine issuing this function.)
void
do_expose_image(double exposure_time_seconds,
		Image **NewImage,
		exposure_flags &ExposureFlags,
		const char *host_FITS_filename,
		Drifter *drifter = 0) {
  GenMessage    *inbound_message;
  FITSMessage  *FITSimage;

  if (drifter) {
    drifter->ExposureStart(exposure_time_seconds);
  }

  CameraMessage cm(comm_socket, CMD_EXPOSE);

  cm.SetKeywordValue("SHUTTER_OPEN", ExposureFlags.IsShutterShut() ? "0" : "1");

  if(NewImage) {
    cm.SetLocalImageName("-"); // forces response with a FITSMessage
  }

  Filter this_filter = ExposureFlags.FilterRequested();
  cm.SetFilter(this_filter.NameOf()[0]);

  cm.SetExposure(exposure_time_seconds);
  cm.SetBinning(ExposureFlags.GetBinning());

  cm.SetSubFrameMode(ExposureFlags.subframe.box_bottom,
		     ExposureFlags.subframe.box_top,
		     ExposureFlags.subframe.box_left,
		     ExposureFlags.subframe.box_right);

  cm.SetCameraMode(ExposureFlags.GetReadoutMode());
  cm.SetCameraGain(ExposureFlags.GetGain());
  cm.SetCompressImage(ExposureFlags.IsCompression());
  cm.SetOffset(ExposureFlags.GetOffset());
  cm.SetUSBTraffic(ExposureFlags.USBTraffic());

  switch (ExposureFlags.GetOutputFormat()) {
  case exposure_flags::E_uint16:
    cm.SetPixelFormat(PIXEL_UINT16); break;

  case exposure_flags::E_uint32:
    cm.SetPixelFormat(PIXEL_UINT32); break;

  case exposure_flags::E_float:
    cm.SetPixelFormat(PIXEL_FLOAT); break;
  }

  // start the exposure
  fprintf(stderr, "Sending StartExposure command (%.2f sec).\n",
	  exposure_time_seconds);
  cm.send();

  if (drifter) {
    drifter->ExposureGuide(); // this will block for duration of exposure
  }

  // now wait for a response
  //repeat:
  inbound_message = GenMessage::ReceiveMessage(comm_socket);
  if(inbound_message == 0) {
    fprintf(stderr, "camera_api: connection failed; exposure terminated.\n");
    return;
  }
  
  CameraMessage *status = 0;
  switch(inbound_message->MessageID()) {

    /********************************/
    /*    StatusMessage             */
    /********************************/
  case CameraMessageID:
    status = (CameraMessage *) inbound_message;
    if (status->GetCommand() != CMD_STATUS) {
      fprintf(stderr, "camera_api: wrong response to exposure command.\n");
      return;
    }
    
    // exposure is done
    break;

    /********************************/
    /*    FITS  Message             */
    /********************************/
  case FITSMessageID:
    {
      void *FITSfileImage;
      size_t filesize;

      if(NewImage == 0) {
	fprintf(stderr, "camera_api: FITSMessage with nil image pointer.\n");
	break;
      }

      FITSimage = (FITSMessage *) inbound_message;
      FITSimage->GetFITSFile(&filesize, &FITSfileImage);
      *NewImage = new Image(FITSfileImage, filesize);
      exposure_flags::E_PixelFormat f = ExposureFlags.GetOutputFormat();
      if (f == exposure_flags::E_float) {
	(*NewImage)->SetImageFormat(FLOAT_IMG);
      } else if (ExposureFlags.GetBinning() == 1 or
		 f == exposure_flags::E_uint16) {
	(*NewImage)->SetImageFormat(USHORT_IMG);
      } else {
	(*NewImage)->SetImageFormat(ULONG_IMG);
      }
    }
    // exposure is done
    break;
      
    /********************************/
    /*    All other messages        */
    /********************************/
  case ExposeMessageID:
  case RequestStatusMessageID:
  default:
    // Makes absolutely no sense for us to receive these.
    fprintf(stderr, "Illegal message received by camera_api (%d).\n",
	    inbound_message->MessageID());
    break;
  }

  delete inbound_message;
  
}

// This invocation of expose_image is used when we have a specific
// local filename into which the exposure is to be put. The USE_RCP
// preprocessor directive is used to distinguish between the original
// method of moving images between machines, and the newer way, which
// uses the FITSMessage as the only technique to move images between
// machines. 
#ifdef USE_RCP // as of 6/17/2020, USE_RCP is not defined
void
expose_image_local(double exposure_time_seconds,
	     exposure_flags &ExposureFlags,
	     const char *local_FITS_filename,
	     const char *purpose,
	     Drifter *drifter) {
  char          remote_filename[256];
  sprintf(remote_filename, "/home/exposures/image%03d", exposure_number);

  time(&ExposureStartTime);	// remember when this starts

  do_expose_image(exposure_time_seconds,
		  0,
		  ExposureFlags,
		  remote_filename,
		  drifter);

  // Now need to get the file
  {
    char remote_command[256];

    sprintf(remote_command,
	    "rcp %s:%s %s",
	    CAMERA_HOST,
	    remote_filename,
	    local_FITS_filename);
    
    int rcp_result = system(remote_command);
    if(rcp_result != 0) {
      fprintf(stderr, "rcp from %s failed: %d\n",
	      CAMERA_HOST,
	      rcp_result);
    }
  }
  update_fits_data(local_FITS_filename, purpose);
  NotifyServiceProvider(local_FITS_filename);
}
#else // non-RCP-technique
// this is the approach that doesn't use RCP
void
expose_image_local(double exposure_time_seconds,
	     exposure_flags &ExposureFlags,
	     const char *local_FITS_filename,
	     const char *purpose,
	     Drifter *drifter) {
  Image *new_image;
  
  time(&ExposureStartTime);	// remember when this starts

  // invoke do_expose_image with a filename of "-" to force the image
  // to come back as a FITSMessage, which will be used to create an
  // Image with a pointer in new_image.
  do_expose_image(exposure_time_seconds,
		  &new_image,
		  ExposureFlags,
		  "-",
		  drifter);

  // put the file onto disk
  new_image->WriteFITSAuto(local_FITS_filename);
  delete new_image; // all further activity uses the filesystem version

  update_fits_data(local_FITS_filename, purpose);
  NotifyServiceProvider(local_FITS_filename);
}
#endif // RCP

char *expose_image(double exposure_time_seconds,
		   exposure_flags &ExposureFlags,
		   const char *purpose,
		   Drifter *drifter) {
  char *next_valid_filename = NextValidImageFilename();

  expose_image_local(exposure_time_seconds,
		     ExposureFlags,
		     next_valid_filename,
		     purpose,
		     drifter);
  return next_valid_filename;
}

void
host_expose_image(double exposure_time_seconds,
		  exposure_flags &ExposureFlags,
		  char *host_FITS_filename) {
  do_expose_image(exposure_time_seconds,
		  (Image **) nullptr,
		  ExposureFlags,
		  host_FITS_filename,
		  0 /*Drifter*/);
}

void logfile_msg(const char *msg) {
  struct timeval Now;
  (void) gettimeofday(&Now, 0);
  fprintf(stderr, "[%ld.%06ld] %s\n",
	  Now.tv_sec, Now.tv_usec, msg);
}

// This handles the inbound info on the cooler's status

int // return 0 on error, 1 on success
CCD_cooler_data(double *ambient_temp,
		double *ccd_temp,
		double *cooler_setpoint,
		int    *cooler_power,
		double *humidity,
		int    *mode,
		int    cooler_flags) {
  static int last_unique_id = 0;
  
  if(comm_socket == COMM_UNINITIALIZED) {
    connect_to_camera();
  }
  if(comm_socket < 0) return 0;

  GenMessage    *inbound_message;
  CameraMessage *stat;

  if(!(cooler_flags & COOLER_NO_SEND)) {
    //logfile_msg("send StatusRequestCommand()");
    CameraMessage cm(comm_socket, CMD_COOLER);
    cm.SetQuery();
    cm.send();
    last_unique_id = cm.GetUniqueID();
  }

 repeat_read:
  if(!(cooler_flags & COOLER_NO_WAIT)) {
    // now wait for a response
    //logfile_msg("cooler: waiting for inbound message");
    inbound_message = GenMessage::ReceiveMessage(comm_socket);
    //logfile_msg("cooler: message received");
    if(inbound_message == 0) {
      fprintf(stderr,
	      "camera_api: connection failed; cooler query terminated.\n");
      return 0;
    }
  
    switch(inbound_message->MessageID()) {

      /********************************/
      /*    StatusMessage             */
      /********************************/
    case CameraMessageID:
      {
	stat = (CameraMessage *) inbound_message;
	if (stat->GetCommand() != CMD_STATUS) {
	  fprintf(stderr, "camera_api: wrong response to cooler status request.\n");
	  return 0;
	}
	if (stat->GetUniqueID() != last_unique_id) {
	  // there must be more messages in the inbound queue
	  goto repeat_read;
	}

	if (stat->CoolerTempAvail()) {
	  *ccd_temp        = stat->GetCoolerTemp();
	} else {
	  *ccd_temp = 0.0;
	  fprintf(stderr, "camera_api: cooler response missing CCD temp keyword.\n");
	}

	if (stat->AmbientTempAvail()) {
	  *ambient_temp    = stat->GetAmbientTemp();
	} else {
	  *ambient_temp = 0.0;
	  fprintf(stderr, "camera_api: cooler response missing ambient temp keyword.\n");
	}

	if (stat->CoolerPowerAvail()) {
	  *cooler_power    = (int) (0.5 + 100.0*stat->GetCoolerPower());
	} else {
	  *cooler_power = 0;
	  fprintf(stderr, "camera_api: cooler response missing cooler power keyword.\n");
	}

	if (stat->CoolerSetpointAvail()) {
	  *cooler_setpoint = stat->GetCoolerSetpoint();
	} else {
	  *cooler_setpoint = 0.0;
	  fprintf(stderr, "camera_api: cooler response missing cooler setpoint keyword.\n");
	}

	if (stat->HumidityAvail()) {
	  *humidity        = stat->GetHumidity();
	} else {
	  *humidity = 0.0;
	  // no error message
	}

	string mode_string = (stat->CoolerModeAvail() ?
			      stat->GetCoolerMode() :
			      "MANUAL");
	if (mode_string == "OFF") {
	  *mode = 0;
	} else if (mode_string == "MANUAL") {
	  *mode = CCD_COOLER_ON;
	} else if (mode_string == "SETPOINT") {
	  *mode = (CCD_COOLER_ON | CCD_COOLER_REGULATING);
	} else {
	  cerr << "CameraMessage: Invalid cooler mode string: "
	       << mode_string << endl;
	  *mode = 0;
	}
      }
      // We have what we wanted
      break;

      /********************************/
      /*    All other messages        */
      /********************************/
    case ExposeMessageID:
    case RequestStatusMessageID:
    default:
      // Makes absolutely no sense for us to receive these.
      fprintf(stderr, "Illegal message received by camera_api (%d).\n",
	      inbound_message->MessageID());
      break;
    }

    delete inbound_message;
    //logfile_msg("cooler: return(1)");
    return 1;
  } else {
    //logfile_msg("cooler: return(0)");
    return 0;
  }
}


CoolerCommand::CoolerCommand(void) {
  mode = NO_COMMAND;
}

void
CoolerCommand::SetCoolerOff(void) {
  mode = COOLER_OFF;
}

void
CoolerCommand::SetCoolerManual(double PowerLevel) { // 0->1.0
  mode = MANUAL;
  Power = PowerLevel;
}

void
CoolerCommand::SetCoolerSetpoint(double TempC) {
  mode = SETPOINT;
  Setpoint = TempC;
}

int // return 0 on error, return 1 on success
CoolerCommand::Send(void) {
  if(comm_socket == COMM_UNINITIALIZED) {
    connect_to_camera();
  }
  if(comm_socket < 0) return 0;

  const char *CoolerMode = "";
  double CoolerSetpoint = 0.0;
  double CoolerPower = 0.0;

  switch (mode) {
  case NO_COMMAND:
    fprintf(stderr, "CoolerCommand: usage error: send() without setup.\n");
    return 0;

  case COOLER_OFF:
    CoolerMode = "OFF";
    break;

  case MANUAL:
    CoolerMode = "MANUAL";
    CoolerPower = Power; // double range 0..1
    break;

  case SETPOINT:
    CoolerMode = "SETPOINT";
    CoolerSetpoint = Setpoint;
    break;
  }
  
  CameraMessage cm(comm_socket, CMD_COOLER);
  cm.SetCoolerSetpoint(CoolerSetpoint);
  cm.SetCoolerPower(CoolerPower);
  cm.SetKeywordValue("COOLER_MODE", string(CoolerMode));

  // ask for cooler data
  // fprintf(stderr, "Sending cooler command.\n");
  cm.send();
  
  return 1;
}
  
void
expose_image(double exposure_time_seconds,
	     Image **NewImage,
	     exposure_flags &ExposureFlags) {
  do_expose_image(exposure_time_seconds,
		  NewImage,
		  ExposureFlags,
		  "-");
}

int // return 0 on failure, 1 on success
FilterCommand::Send(void) {
  GenMessage *inbound_message;
  int response = 0;

  if(comm_socket == COMM_UNINITIALIZED) {
    connect_to_camera();
  }
  if(comm_socket < 0) return 0;

  CameraMessage cm(comm_socket, CMD_FILTER_CONFIG);
  if(JustQuery) {
    cm.SetKeywordValue("STATUS_QUERY", to_string(true));
    cm.send();
  } else {
    for (int n=0; n<NumFilters; n++) {
      cm.SetKeywordValue(string("FILTER_")+to_string(n),
			 installed_filters[n].NameOf());
    }
    cm.send();
  }
  delete [] installed_filters; // still valid even if <nil>
  installed_filters = 0;

  // now wait for a response
  inbound_message = GenMessage::ReceiveMessage(comm_socket);
  if(inbound_message == 0) {
    fprintf(stderr,
	    "camera_api: connection filed; filter config terminated.\n");
    return 0;
  }

  switch(inbound_message->MessageID()) {

  case CameraMessageID:
    {
      CameraMessage *in = (CameraMessage *) inbound_message;
      if (in->GetCommand() != CMD_FILTER_CONFIG) {
	fprintf(stderr,
		"camera_api: invalid inbound CameraMessage: %d\n",
		in->GetCommand());
	return 0;
      }
      {
	std::list<string> filter_names;
	for (int n=0; n<9; n++) {
	  string keyword = string("FILTER_") + to_string(n);
	  if (in->KeywordPresent(keyword)) {
	    filter_names.push_back(in->GetValueString(keyword));
	  } else {
	    NumFilters = n;
	    break;
	  }
	}
      
	if(NumFilters != 0) {
	  installed_filters = new Filter [NumFilters];
	  int n=0;
	  for(auto f : filter_names) {
	    installed_filters[n] = Filter(f.c_str());
	    n++;
	  }
	}
      }
      response = 1;
      break;
    }

  default:
    // Makes absolutely no sense for us to receive these.
    fprintf(stderr, "Illega mesage received by camera_api/filter (%d).\n",
	    inbound_message->MessageID());
    break;
  }

  delete inbound_message;
  return response;
}

FilterCommand::FilterCommand(void) {
  JustQuery = 1;		// may be overridden later
  NumFilters = 0;
  installed_filters = 0;
}

void
FilterCommand::SetNoFilter(void) {
  if(installed_filters) delete [] installed_filters;
  NumFilters = 0;
  installed_filters = 0;
  JustQuery = 0;
}

void
FilterCommand::SetFixedFilter(Filter filter) {
  if(installed_filters) delete [] installed_filters;
  NumFilters = 1;
  installed_filters = new Filter[1];
  installed_filters[0] = filter;
  JustQuery = 0;
}

void
FilterCommand::SetWheelFilters(int num_filters, const Filter *filters) {
  if(installed_filters) delete [] installed_filters;
  NumFilters = num_filters;
  installed_filters = new Filter[num_filters];

  int n;
  for(n=0; n<num_filters; n++) {
    installed_filters[n] = filters[n];
  }
  JustQuery = 0;
}

int
FilterCommand::GetNumFilters(void) {
  return NumFilters;
}

Filter *
FilterCommand::GetFilters(void) {
  if(NumFilters == 0) return 0;

  Filter *answer = new Filter[NumFilters];
  int n;
  for(n=0; n<NumFilters; n++) {
    answer[n] = installed_filters[n];
  }
  return answer;
}

void do_qhy_test(void) {
  GenMessage    *inbound_message;
  FITSMessage  *FITSimage;

  CameraMessage cm(comm_socket, CMD_EXPOSE);

  cm.SetKeywordValue("SHUTTER_OPEN", "1");

  cm.SetLocalImageName("-"); // forces response with a FITSMessage

  cm.SetFilter('V');

  constexpr double exp_time = 0.00001;

  cm.SetExposure(exp_time);
  cm.SetBinning(1);
  const int box_bottom = 10;
  const int box_top = 6279;
  const int box_left = 0;
  const int box_right = 4209;
  cm.SetSubFrameMode(box_bottom,
  		     box_top,
  		     box_left,
  		     box_right);

  // start the exposure
  fprintf(stderr, "Sending StartExposure command (%.2f sec).\n",
	  exp_time);
  cm.send();

  // now wait for a response
  //repeat:
  inbound_message = GenMessage::ReceiveMessage(comm_socket);
  if(inbound_message == 0) {
    fprintf(stderr, "camera_api: connection failed; exposure terminated.\n");
    return;
  } else {
    fprintf(stderr, "camera_api: response message received.\n");
  }
  
  CameraMessage *status = 0;
  switch(inbound_message->MessageID()) {

    /********************************/
    /*    StatusMessage             */
    /********************************/
  case CameraMessageID:
    fprintf(stderr, "camera_api: received CameraMessage\n");
    status = (CameraMessage *) inbound_message;
    if (status->GetCommand() != CMD_STATUS) {
      fprintf(stderr, "camera_api: wrong response to exposure command.\n");
      return;
    }
    
    // exposure is done
    break;

    /********************************/
    /*    FITS  Message             */
    /********************************/
  case FITSMessageID:
    fprintf(stderr, "camera_api: received FITSMessage\n");
    {
      void *FITSfileImage;
      size_t filesize;

      FITSimage = (FITSMessage *) inbound_message;
      FITSimage->GetFITSFile(&filesize, &FITSfileImage);

      fprintf(stderr, "FITS filesize = %ld\n", (long) filesize);
      Image image(FITSfileImage, filesize);
      fprintf(stderr, "Writing FITS file to /tmp/image.fits\n");
      image.WriteFITS("/tmp/image.fits");
    }
    // exposure is done
    break;
      
    /********************************/
    /*    All other messages        */
    /********************************/
  case ExposeMessageID:
  case RequestStatusMessageID:
  default:
    // Makes absolutely no sense for us to receive these.
    fprintf(stderr, "Illegal message received by camera_api (%d).\n",
	    inbound_message->MessageID());
    break;
  }

  delete inbound_message;
  
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

