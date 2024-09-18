/*  StatusMessage.cc -- Camera message providing status of camera
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
#include <string.h>
#include <stdio.h>
#include "StatusMessage.h"
#include "RequestStatusMessage.h"

//
// Message format: (total length >= 20)
//
// bytes 0-3	size
//       4	message ID
//       5      Unique ID of Request
//       6      Server Status
//       7	Camera Status
//       8-11   Remaining exposure time in msec (lsb in byte 5)
//      12-15   LastImageSequenceNo (lsb in byte 11)
//      16-19   LastUserExposureID (lsb in byte 15)
//      20-23   OrderedExposureTime1 in msec (lsb in byte 19)
//      24-27   OrderedExposureTime2 in msec (lsb in byte 23)
//      28-31   CoolerSetpoint in 1/10 of a degree with -100 offset
//      32-35   CCDTemp in 1/10 of a degree with -100 offset
//      36-39   AmbientTemp in 1/10 of a degree with -100 offset
//      40      CoolerPower (percent)
//      41      ShutterPosition
//      42      CoolerFlags
//      43-end  LastImageFilename (null-terminated, null is present)
// 
// There is no response to this message.
//

#define REQUEST_UID_BYTE            5
#define SERVERSTATUS_BYTE           (REQUEST_UID_BYTE+1)
#define CAMERASTATUS_BYTE           (SERVERSTATUS_BYTE+1)
#define REMAININGEXPOSURE_BYTE_LOW  (CAMERASTATUS_BYTE+1)
#define SEQNO_BYTE_LOW              (REMAININGEXPOSURE_BYTE_LOW+4)
#define USERID_BYTE_LOW             (SEQNO_BYTE_LOW+4)
#define ORDEREDEXPOSURE1_BYTE       (USERID_BYTE_LOW+4)
#define ORDEREDEXPOSURE2_BYTE       (ORDEREDEXPOSURE1_BYTE+4)
#define COOLERSETPOINT_BYTE         (ORDEREDEXPOSURE2_BYTE+4)
#define CCDTEMP_BYTE                (COOLERSETPOINT_BYTE+4)
#define AMBIENTTEMP_BYTE            (CCDTEMP_BYTE+4)
#define COOLERPOWER_BYTE            (AMBIENTTEMP_BYTE+4)
#define SHUTTERPOSITION_BYTE        (COOLERPOWER_BYTE+1)
#define COOLERFLAGS_BYTE            (SHUTTERPOSITION_BYTE+1)
#define FILENAME_BYTE_START         (COOLERFLAGS_BYTE+1)

#define MINIMUM_SIZE (FILENAME_BYTE_START+2)
#define BYTES_NOT_COUNTING_FILENAME (FILENAME_BYTE_START+1)

////////////////////////////////////////////////////////////////
//        Constructors
////////////////////////////////////////////////////////////////
StatusMessage::StatusMessage(int    Socket,
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
			     const char *LastImageFilename) :
  GenMessage(Socket,
	     1+strlen(LastImageFilename)+BYTES_NOT_COUNTING_FILENAME) {
	     
    content[4] = StatusMessageID;
    content[REQUEST_UID_BYTE]       = 0;
    content[SERVERSTATUS_BYTE]      = ServerStatus;
    content[CAMERASTATUS_BYTE]      = CameraStatus;
    content[SHUTTERPOSITION_BYTE]   = ShutterPosition;
    content[COOLERFLAGS_BYTE]       = CoolerFlags;
    content[COOLERPOWER_BYTE]       = CoolerPower;

    pack_4byte_int(content+SEQNO_BYTE_LOW,  LastImageSequenceNo);
    pack_4byte_int(content+USERID_BYTE_LOW, LastUserExposureID);

    pack_4byte_int(content+REMAININGEXPOSURE_BYTE_LOW,
		   (int)(1000.0*SecondsLeftInExposure));
    pack_4byte_int(content+ORDEREDEXPOSURE1_BYTE,
		   (int)(1000.0*OrderedExposure1));
    pack_4byte_int(content+ORDEREDEXPOSURE2_BYTE,
		   (int)(1000.0*OrderedExposure2));
    pack_4byte_int(content+CCDTEMP_BYTE,
		   (int)(100.0*(CCDTemp+100.0)));
    pack_4byte_int(content+COOLERSETPOINT_BYTE,
		   (int)(100.0*(CoolerSetpoint+100.0)));
    pack_4byte_int(content+AMBIENTTEMP_BYTE,
		   (int)(100.0*(AmbientTemp+100.0)));

    strcpy((char *) (content+FILENAME_BYTE_START), LastImageFilename);
  }


StatusMessage::StatusMessage(GenMessage *message) :
  GenMessage(message) {

    if(GenMessSize < (BYTES_NOT_COUNTING_FILENAME+1) ||
       MessageID() != StatusMessageID) {
      fprintf(stderr,
	      "StatusMessage: constructor reasonableness check failed (%d vs %d).\n",
	      GenMessSize, 1+BYTES_NOT_COUNTING_FILENAME);
    }
  }

void
StatusMessage::SetUniqueID(char id) {
  content[REQUEST_UID_BYTE] = id;
}

bool
StatusMessage::MatchesUniqueID(RequestStatusMessage *msg) {
  return content[REQUEST_UID_BYTE] == msg->GetUniqueID();
}

bool
StatusMessage::MatchesUniqueID(char id) {
  return content[REQUEST_UID_BYTE] == id;
}

int
StatusMessage::GetServerStatus(void) {
  return content[SERVERSTATUS_BYTE];
}

int
StatusMessage::GetCameraStatus(void) {
  return content[CAMERASTATUS_BYTE];
}

int
StatusMessage::LastImageFilenameLength(void) {
  return GenMessSize - BYTES_NOT_COUNTING_FILENAME - 1;
}

int
StatusMessage::GetLastImageSequenceNo(void) {
  return get_4byte_int(content + SEQNO_BYTE_LOW);
}
int
StatusMessage::GetLastUserExposureID(void) {
  return get_4byte_int(content + USERID_BYTE_LOW);
}

char *
StatusMessage::GetLastImageFilename(void) {
  return (char *) (content + FILENAME_BYTE_START);
}

int
StatusMessage::GetShutterPosition(void) {
  return content[SHUTTERPOSITION_BYTE];
}

double
StatusMessage::GetSecondsLeftInExposure(void) {
  return ((double) get_4byte_int(content + REMAININGEXPOSURE_BYTE_LOW))/1000.0;
}

double         
StatusMessage::GetOrderedExposure1(void) { /* main CCD */
  return ((double) get_4byte_int(content + ORDEREDEXPOSURE1_BYTE))/1000.0;
}

double         
StatusMessage::GetOrderedExposure2(void) { /* tracking CCD */
  return ((double) get_4byte_int(content + ORDEREDEXPOSURE2_BYTE))/1000.0;
}

int            
StatusMessage::GetCoolerFlags(void) {
  return content[COOLERFLAGS_BYTE];
}

double         
StatusMessage::GetCoolerSetpoint(void) { /* degrees F */
  return ((double) get_4byte_int(content + COOLERSETPOINT_BYTE))/100.0 - 100.0;
}

double         
  StatusMessage::GetCCDTemp(void) {
  return ((double) get_4byte_int(content + CCDTEMP_BYTE))/100.0 - 100.0;
}

double         
  StatusMessage::GetAmbientTemp(void) {
  return ((double) get_4byte_int(content + AMBIENTTEMP_BYTE))/100.0 - 100.0;
}

int            
StatusMessage::GetCoolerPower(void) { /* percent */
  return content[COOLERPOWER_BYTE];
}
