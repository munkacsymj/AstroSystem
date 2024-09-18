/*  StatusMessage.h -- Camera message providing status of camera
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
#include "gen_message.h"

class RequestStatusMessage;
class StatusMessage : public GenMessage {
public:
  StatusMessage(int    Socket,
		int    ServerStatus,
		int    CameraStatus,
		int    LastImageSequenceNo,
		int    LastUserExposureID,
		int    ShutterPosition,
		double SecondsLeftInExposure,
		double OrderedExposure1, /* main CCD */
		double OrderedExposure2, /* tracking CCD */
		int    CoolerFlags,
		double CoolerSetpoint, /* degrees F */
		double CCDTemp,
		double AmbientTemp,
		int    CoolerPower, /* percent */
		const char *LastImageFilename);
  StatusMessage(GenMessage *message);
  void SetUniqueID(char id);

  bool MatchesUniqueID(RequestStatusMessage *msg);
  bool MatchesUniqueID(char id);
  
  void process(void);

  char          *GetLastImageFilename(void);
  int            GetLastImageSequenceNo(void);
  int            GetLastUserExposureID(void);
  int            GetServerStatus(void);
  int            GetCameraStatus(void);
  int            LastImageFilenameLength(void);

  int            GetShutterPosition(void);
  double         GetSecondsLeftInExposure(void);
  double         GetOrderedExposure1(void); /* main CCD */
  double         GetOrderedExposure2(void); /* tracking CCD */
  int            GetCoolerFlags(void);
  double         GetCoolerSetpoint(void); /* degrees F */
  double         GetCCDTemp(void);
  double         GetAmbientTemp(void);
  int            GetCoolerPower(void); /* percent */
};

// Allowed values of ServerStatus
#define SERVER_READY		0x00
#define SERVER_BUSY		0x14
#define SERVER_BAD_COMMAND	0x15 // couldn't handle last command

// allowed values of CameraStatus
#define CAMERA_SHUTTER_OPEN	0x23
#define CAMERA_IO_BUSY		0x24
#define CAMERA_IDLE		0x25

// Allowed values of ShutterPosition
#define CAMERA_SHUTTER_OPEN	0x23
#define CAMERA_SHUTTER_SHUT     0x26

// Allowed values of CoolerFlags
#define COOLER_ON               0x01
#define COOLER_REGULATING       0x02
