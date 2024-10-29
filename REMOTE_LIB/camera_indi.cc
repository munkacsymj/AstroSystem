/*  camera_indi.cc -- Implements user view of camera
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

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "camera_indi.h"

#include <system_config.h>

//****************************************************************
//        Constructor, Destructor
//****************************************************************
CAMERA_INDI::CAMERA_INDI(AstroDevice *device, const char *connection_port, const char *local_devname) :
  LocalDevice(device, connection_port), dev(device) {
  //********************************
  // Set up camera_model
  //********************************
  if (strcmp(local_devname, "ST-10XME") == 0) {
    this->camera_model = CAM_ST10XME;
  } else if (strcmp(local_devname, "QHY268M") == 0) {
    this->camera_model = CAM_QHY268M;
  } else {
    std::cerr << "CAMERA_INDI: camera name '" << local_devname << "' not recognized.\n";
    INDIDisconnectINDI();
    exit(-2);
  }
  
  this->DoINDIRegistrations();
  dev->indi_device.watchProperty("CCD1",
				 [this](INDI::Property p) {
				   //this->FetchImage(p);
				   this->blob_blocker.Signal(); },
				 INDI::BaseDevice::WATCH_UPDATE);
#if 0
  dev->indi_device.watchProperty("CCD_EXPOSURE",
				 [this](INDI::Property p) {
				   std::cerr << "ccd_exposure update: "
					     << p.getNumber()->at(0)->getValue()
					     << ", state = "
					     << p.getNumber()->getStateAsString()
					     << '\n'; },
				 INDI::BaseDevice::WATCH_UPDATE);
  //std::cerr << "CAMERA_INDI: BLOB mode = "
  //	    << device->local_client->getBLOBMode(device->device_name, "CCD1")
  //	    << std::endl;
#endif
}

void
CAMERA_INDI::FetchImage(INDI::Property indi_prop) {
  //std::cerr << "Starting CAMERA_INDI::FetchImage()\n";
  auto indi_blob = indi_prop.getBLOB();
  auto indi_value = indi_blob->at(0);
  long image_size = indi_value->getSize();
  uid_t uid = geteuid();
  std::string pathname("/run/user/");
  pathname += (std::to_string(uid)+"/ASTRO");
  //std::cerr << "Making sure " << pathname << " exists.\n";

  int mkdir_res = mkdir(pathname.c_str(), S_IRUSR|S_IWUSR|S_IXUSR);
  if (mkdir_res == 0 or errno == EEXIST) {
    // directory exists
    pid_t pid = getpid();
    pathname += ("/" + std::to_string(pid) + "_image.fits");
    //std::cerr << "Copying BLOB to " << pathname << std::endl;
    
    int fd = open(pathname.c_str(), O_WRONLY|O_CREAT, S_IRUSR|S_IWUSR|S_IXUSR);
    if (fd < 0) {
      perror("ERROR creating temporary image file");
    } else {
      int bytes_written = write(fd, indi_value->getBlob(), image_size);
      if (bytes_written != image_size) {
	std::cerr << "ERROR: problem writing temp image to /run/user\n";
      }
      close(fd);
      this->new_image = make_unique<Image>(pathname.c_str());
      this->AddKeywords(this->new_image);

      //std::cerr << "Image received: "
      //	<< new_image->height << " x " << new_image->width
      //	<< std::endl;
    }
    unlink(pathname.c_str());
  } else {
    perror("Error creating /run/user temp dir");
  }
}

double
CAMERA_INDI::GetEGain(long gain_setting, int readoutmode) {
  // egain is system gain
  SystemConfig config;
  if (this->camera_model == CAM_QHY268M) {
    double egain = 0.0;
    switch(readoutmode) {
    case 0:
      if (gain_setting < 30) egain = 1.58-0.03667*gain_setting;
      else if (gain_setting < 65) egain = 0.8658-0.01286*gain_setting;
      else egain = 0.06705-0.00057*gain_setting;
      break;
    case 1:
      egain = 1.002-0.0098*gain_setting;
      break;
    case 2:
      egain = 1.543-0.0143*gain_setting;
      break;
    case 3:
      egain = 1.628-0.0153*gain_setting;
      break;
    }
    return egain;
  } else if (this->camera_model == CAM_ST10XME) {
    return 1.3; // This needs to be measured; the value here is taken from online documentation
  } else if (this->camera_model == CAM_ST9) {
    return 2.2;			// again, comes from Company7 online documentation, not measurement
  } else {
    std::cerr << "ERROR: CAMERA_INDI: camera type unknown.\n";
  }
  return 1.0; // maybe should just trigger an assertion fault here instead?
}
  
void
CAMERA_INDI::AddKeywords(unique_ptr<Image> &image) {
  SystemConfig config;		// fetches current info from system config file
  ImageInfo *info = image->GetImageInfo();
  info->SetFrameXY((int) (0.5 + this->cam_frame_x.getValue()),
		   (int) (0.5 + this->cam_frame_y.getValue()));
  info->SetExposureDuration(this->user_exp_time);
  const double cdelt = config.PixelScale() * user_flags.GetBinning();
  info->SetCdelt(cdelt, cdelt);
  info->SetFilter(Filter("None"));
  info->SetDatamax(user_flags.GetDataMax());
  info->SetInvalidADU(user_flags.GetInvalidADU());
  // Warning: this is the UNBINNED system gain. Probably misleading in a binned config
  info->SetEGain(this->GetEGain(user_flags.GetGain(), user_flags.GetReadoutMode()));
  JULIAN mid_time((time_t) (this->exposure_start_time + this->user_exp_time));
  info->SetExposureStartTime(mid_time);
  info->SetPurpose(this->user_purpose.c_str());
  info->SetBinning(user_flags.GetBinning());
  if (this->camera_model == CAM_QHY268M) {
    info->SetOffset(user_flags.GetOffset());
    info->SetReadmode(user_flags.GetReadoutMode());
    info->SetCamGain(user_flags.GetGain());
  }
}

int
CAMERA_INDI::ExposureStart(double exposure_time_seconds,
			   const char *purpose,
			   exposure_flags &ExposureFlags) {
  this->user_exp_time = (exposure_time_seconds < 0.1 ? 0.1 : exposure_time_seconds);
  this->user_flags = ExposureFlags;
  if (purpose) {
    this->user_purpose = std::string(purpose);
  } else {
    this->user_purpose = std::string("");
  }

  // enable logging if not already done
  if (this->cam_debug_enable.getState() == ISS_OFF) {
    //std::cerr << "CAMERA_INDI: enabling debug logging.\n";
    this->cam_debug_enable.setState(ISS_ON);
    this->cam_debug_disable.setState(ISS_OFF);
    this->dev->local_client->sendNewSwitch(this->cam_debug_enable.property->indi_property);
    sleep(1);
    this->cam_log_file.setState(ISS_ON);
    this->dev->local_client->sendNewSwitch(this->cam_log_file.property->indi_property);
    this->cam_log_debug.setState(ISS_ON);
    this->dev->local_client->sendNewSwitch(this->cam_log_debug.property->indi_property);
    sleep(1);
  }
  
  if (ExposureFlags.IsShutterShut()) {
    this->cam_type_light.setState(ISS_OFF);
    this->cam_type_bias.setState(ISS_OFF);
    this->cam_type_dark.setState(ISS_ON);
    this->cam_type_flat.setState(ISS_OFF);
  } else {
    this->cam_type_light.setState(ISS_ON);
    this->cam_type_bias.setState(ISS_OFF);
    this->cam_type_dark.setState(ISS_OFF);
    this->cam_type_flat.setState(ISS_OFF);
  }
  this->dev->local_client->sendNewSwitch(this->cam_type_flat.property->indi_property);
  
  this->cam_frame_x.setValue(ExposureFlags.subframe.box_left);
  this->cam_frame_y.setValue(ExposureFlags.subframe.box_bottom);
  this->cam_frame_width.setValue(ExposureFlags.subframe.box_width());
  this->cam_frame_height.setValue(ExposureFlags.subframe.box_height());
  this->dev->local_client->sendNewNumber(this->cam_frame_height.property->indi_property);
  //std::cerr << "Frame dims set to "
  //	    << ExposureFlags.subframe.box_left << " x "
  //	    << ExposureFlags.subframe.box_bottom << " x "
  //	    << ExposureFlags.subframe.box_width() << " x "
  //	    << ExposureFlags.subframe.box_height() << '\n';

  //cm.SetCameraMode(ExposureFlags.GetReadoutMode());
  //cm.SetCameraGain(ExposureFlags.GetGain());
  //cm.SetOffset(ExposureFlags.GetOffset());
  //cm.SetUSBTraffic(ExposureFlags.USBTraffic());

  if (this->camera_model == CAM_QHY268M) {
    this->cam_readoutmode.setValue(ExposureFlags.GetReadoutMode());
    this->dev->local_client->sendNewNumber(this->cam_readoutmode.property->indi_property);
    
    this->cam_gain_setting.setValue(ExposureFlags.GetGain());
    this->dev->local_client->sendNewNumber(this->cam_gain_setting.property->indi_property);
    
    this->cam_offset.setValue(ExposureFlags.GetOffset());
    this->dev->local_client->sendNewNumber(this->cam_offset.property->indi_property);
    
    this->cam_usbtraffic.setValue(ExposureFlags.USBTraffic());
    this->dev->local_client->sendNewNumber(this->cam_usbtraffic.property->indi_property);
  }
    
  this->blob_blocker.Setup(); // prep the blocker
  this->cam_exposure_time.setValue(this->user_exp_time); // starts the exposure
  this->dev->local_client->sendNewNumber(this->cam_exposure_time.property->indi_property);
  time(&this->exposure_start_time); // remember exposure start time

  return 0; // success
}

void CAMERA_INDI::WaitForImage(void) {
  // wait for data to arrive
  int ret_code = this->blob_blocker.Wait(this->user_exp_time*1000 + 3*60*1000);
  //std::cerr << "blob_blocker.Wait() returned with ret_code " << ret_code << "\n";
  if (ret_code) {
    std::cerr << "expose_image(): error: "
	      << strerror(ret_code) << std::endl;
  }
}

char *
CAMERA_INDI::ReceiveImage(exposure_flags &ExposureFlags,
			  const char *fits_filename,
			  const char *purpose) {
  this->FetchImage(cam_blob1.property->indi_property);
  ImageInfo *info = this->new_image->GetImageInfo();
  if (info == nullptr) {
    info = this->new_image->CreateImageInfo();
  }
  const int binning = ExposureFlags.GetBinning();
  const double DATAMAX = ExposureFlags.GetDataMax(); // max valid
  const double INVALID_ADU = ExposureFlags.GetInvalidADU();
  info->SetDatamax(DATAMAX);
  info->SetInvalidADU(INVALID_ADU);

  if (binning == 1) {
    for (int row=0; row<this->new_image->height; row++) {
      for (int col=0; col<this->new_image->width; col++) {
	if (this->new_image->pixel(col, row) > DATAMAX) {
	  this->new_image->pixel(col, row) = INVALID_ADU;
	}
      }
    }
    this->new_image->WriteFITS16(fits_filename);
  } else {
    // Need to bin the image: 32-bit output
    const int tgt_w = this->new_image->width/binning;
    const int tgt_h = this->new_image->height/binning;
    Image target(tgt_h, tgt_w);
    info = target.GetImageInfo();
    if (info == nullptr) {
      info = target.CreateImageInfo();
    }
    info->PullFrom(this->new_image->GetImageInfo());
    int num_saturated = 0;
    info->SetBinning(binning);

    // This count *output* row (i.e., row in the final file, not the blob)
    for (int row=0; row<tgt_h; row++) {
      // This, again, counts *output* column
      for (int col=0; col<tgt_w; col++) {
	int overflow = 0;
	uint32_t tgt = 0;
	for (int b=0; b<binning; b++) { // "b" adjusts the row
	  for (int bb=0; bb<binning; bb++) {
	    const uint16_t v (0.5 + this->new_image->pixel(col*binning+bb, row*binning+b));
	    overflow += (v > DATAMAX);
	    tgt += v;
	  }
	}
	if (overflow) {
	  tgt = INVALID_ADU;
	  num_saturated++;
	}
	target.pixel(col,row) = tgt;
      }
    }
    target.WriteFITS32(fits_filename);
  }
  return strdup(fits_filename);
}

void
CAMERA_INDI::DoINDIRegistrations(void) {
  cam_sim_xres.Initialize(2184.);
  cam_sim_xres.Initialize(1472.);
  cam_sim_xsize.Initialize(6.8);
  cam_sim_ysize.Initialize(6.8);
  cam_sim_maxval.Initialize(65535.);
  cam_sim_satur.Initialize(65535.);
  cam_sim_lim_mag.Initialize(18.);
  cam_sim_noise.Initialize(5.0);
  cam_sim_skyglow.Initialize(100.0);
  
}
