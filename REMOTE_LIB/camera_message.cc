/*  camera_message.cc -- Camera message carrying image from the camera
 *
 *  Copyright (C) 2020 Mark J. Munkacsy

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
#include <sys/types.h>
#include <sys/stat.h>		// pick up fstat()
#include <fcntl.h>		// pick up open()
#include <unistd.h>		// pick up close()
#include <stdio.h>
#include "camera_message.h"

#include <iostream>
#include <string>
using namespace std;

//
// Message format:
//
// bytes 0-3	size
//       4	message ID
//       5      command
//       6      uniqueID
//       7-end  contents of the file. 
//
//

////////////////////////////////////////////////////////////////
//        Constructors
////////////////////////////////////////////////////////////////

static unsigned char next_unique_id = 0;

CameraMessage::CameraMessage(int Socket,
			     unsigned char cmd) :
  GenMessage(Socket, 7) { // This message size will be overwritten later
  command = cmd;

  next_unique_id = (next_unique_id + 1) % 256;
  unique_id = next_unique_id;
  ResetHeader();
}

unsigned char *
CameraMessage::ResetHeader(void) {
  content[4] = CameraMessageID;
  content[5] = command;
  content[6] = unique_id;
  return content+7;
}

#define SCAN_OKAY 0
#define SCAN_QUIT 1
#define SCAN_ERR  2

static int start_of_entry(const char **s) {
  while(**s == '\n') (*s)++;
  if (**s != '*') return SCAN_ERR;
  (*s)++;
  if (**s == 'Q') return SCAN_QUIT;
  if (**s != 'K') return SCAN_ERR;
  (*s)++;
  if (**s != '/') return SCAN_ERR;
  (*s)++;
  return SCAN_OKAY;
}

CameraMessage::CameraMessage(GenMessage *message) :
  GenMessage(message) {

  // At this point, "content" is filled and we can start scanning it.
  command = content[5];
  unique_id = content[6];
  const char *s = (char *) content+7;
  int status;
  while((status = start_of_entry(&s)) != SCAN_QUIT) {
    if (status == SCAN_ERR) {
      fprintf(stderr, "CameraMessage: Invalid message format (a): %c.\n",
	      *s);
      break;
    }
    // extract the keyword; will have a trailing '/'
    int c_len = 0;
    const char *keyword_start = s;
    
    while (*s && *s != '/') {
      s++;
      c_len++;
    }
    const string keyword(keyword_start, c_len);
    if (*s != '/') {
      fprintf(stderr, "CameraMessage: Invalid message format (b).\n");
      break;
    }
    s++;
    const char *len_start = s;
    c_len = 0;
    while (*s && isdigit(*s)) {
      s++;
      c_len++;
    }
    const string len_string(len_start, c_len);
    const int val_len = stoi(len_string);
    if (val_len < 0 || val_len > 65535) {
      fprintf(stderr, "CameraMessage: Invalid message format (c).\n");
      break;
    }
    if (*s != 'V') {
      fprintf(stderr, "CameraMessage: Invalid message format (d).\n");
      break;
    }
    s++;
    if (*s != '/') {
      fprintf(stderr, "CameraMessage: Invalid message format (e).\n");
      break;
    }
    s++;
    const string value_string(s, val_len);
    s += val_len;
    if (*s != '/') {
      fprintf(stderr, "CameraMessage: Invalid message format (f).\n");
    }
    s++;

    // cerr << "camera_message: " << keyword << '/' << value_string << endl;
    SetKeywordValue(keyword, value_string);
  }
}

int
CameraMessage::send(void) {
  int total_size = 0;

  // Figure out how much space will be needed
  for (auto x : key_values) {
    total_size += (x.first.length() + 4);
    total_size += (x.second.length() + 7);
  }
  total_size += 19; // all the stuff in the front

  Resize(total_size); // resize the GenMessage

  unsigned char *d = ResetHeader();
  for (auto x : key_values) {
    *d++ = '\n';
    *d++ = '*';
    *d++ = 'K';
    *d++ = '/';
    for (const char &c : x.first) {
      *d++ = c;
    }
    *d++ = '/';
    string len_buf = to_string(x.second.length());
    for (const char &c : len_buf) {
      *d++ = c;
    }
    *d++ = 'V';
    *d++ = '/';
    for (const char &c : x.second) {
      *d++ = c;
    }
    *d++ = '/';
  }
  // very end
  *d++ = '\n';
  *d++ = '*';
  *d++ = 'Q';
  *d++ = 0;

  {
    FILE *clog = fopen("/tmp/camera_message.log", "a");
    if (clog) {
      fprintf(clog, "%02x%02x%02x%02x %02x %02x %02x %s\n",
	      content[0], content[1], content[2], content[3], (unsigned char) content[4],
	      (unsigned char) content[5], (unsigned char) content[6], content+7);
      fclose(clog);
    }
  }
  return GenMessage::send();
}
      
////////////////////////////////
//        Keyword/Value Methods
////////////////////////////////
bool
CameraMessage::KeywordPresent(string keyword) {
  return key_values.find(keyword) != key_values.end();
}
string
CameraMessage::GetValueString(string keyword) {
  return key_values[keyword];
}
double
CameraMessage::GetValueDouble(string keyword) {
  return stod(key_values[keyword]);
}
int
CameraMessage::GetValueInt(string keyword) {
  return stoi(key_values[keyword]);
}
bool
CameraMessage::GetValueBool(string keyword) {
  return stoi(key_values[keyword]);
}

////////////////////////////////
//        Set Methods
////////////////////////////////
void
CameraMessage::SetKeywordValue(const string keyword, const string value) {
  key_values[keyword] = value;
}

void
CameraMessage::SetExposure(double time_secs){
  SetKeywordValue("EXPOSURE", to_string(time_secs));
}
void
CameraMessage::SetFilter(char filter_letter){
  char filter_string[2];
  filter_string[0] = filter_letter;
  SetKeywordValue("FILTER", filter_string);
}
void
CameraMessage::SetBinning(int binning){
  SetKeywordValue("BIN", to_string(binning));
}
void
CameraMessage::SetTransferEachImage(bool xfer_each){
  SetKeywordValue("XFEREACH", to_string(xfer_each));
}
void
CameraMessage::SetLocalImageName(const char *filename){
  SetKeywordValue("IMAGE", filename);
}
void
CameraMessage::SetCameraMode(int mode){
  SetKeywordValue("MODE", to_string(mode));
}
void
CameraMessage::SetCameraGain(int gain){
  SetKeywordValue("GAIN", to_string(gain));
}
void
CameraMessage::SetSubtractImage(const char *filename){
  SetKeywordValue("SUBTRACT", filename);
}
void
CameraMessage::SetAmbientTemp(double temp_C) {
  SetKeywordValue("AMBIENT_TEMP", to_string(temp_C));
}
void
CameraMessage::SetScaleImage(const char *filename){
  SetKeywordValue("SCALE", filename);
}
void
CameraMessage::SetLinearize(bool linearize){
  SetKeywordValue("LINEARIZE", to_string(linearize));
}
void
CameraMessage::SetCoolerTemp(double temp_C){
  SetKeywordValue("CCD_TEMP", to_string(temp_C));
}
void
CameraMessage::SetHumidity(double humidity) {
  SetKeywordValue("HUMIDITY", to_string(humidity));
}
void
CameraMessage::SetCoolerSetpoint(double temp_C) {
  SetKeywordValue("SETPOINT", to_string(temp_C));
}
void
CameraMessage::SetCoolerPower(double power_fraction){
  SetKeywordValue("POWER", to_string(power_fraction));
}
void
CameraMessage::SetOffset(int offset) {
  SetKeywordValue("OFFSET", to_string(offset));
}
void
CameraMessage::SetRepeatCount(int repeat){
  SetKeywordValue("REPEAT", to_string(repeat));
}
void
CameraMessage::SetCombineType(int combine_type){
  SetKeywordValue("COMBINE", to_string(combine_type));
}
void
CameraMessage::SetPixelFormat(int pixel_format) {
  SetKeywordValue("PIXEL_FORMAT", to_string(pixel_format));
}
void
CameraMessage::SetCompressImage(bool compress){
  SetKeywordValue("COMPRESS", to_string(compress));
}
void
CameraMessage::SetUSBTraffic(double usb_traffic) {
  SetKeywordValue("USBTRAFFIC", to_string(usb_traffic));
}

void
CameraMessage::SetSubFrameMode(unsigned int BoxBottom,
			       unsigned int BoxTop,
			       unsigned int BoxLeft,
			       unsigned int BoxRight) {
  SetKeywordValue("LEFT", to_string(BoxLeft));
  SetKeywordValue("RIGHT", to_string(BoxRight));
  SetKeywordValue("TOP", to_string(BoxTop));
  SetKeywordValue("BOTTOM", to_string(BoxBottom));
}

void
CameraMessage::GetSubFrameData(unsigned int *BoxBottom,
			       unsigned int *BoxTop,
			       unsigned int *BoxLeft,
			       unsigned int *BoxRight) {
  if ((not KeywordPresent("BOTTOM")) or
      (not KeywordPresent("TOP")) or
      (not KeywordPresent("LEFT")) or
      (not KeywordPresent("RIGHT"))) {
    *BoxBottom = -1;
    *BoxTop = -1;
    *BoxLeft = -1;
    *BoxRight = -1;
  } else {
    *BoxBottom = GetValueInt("BOTTOM");
    *BoxTop = GetValueInt("TOP");
    *BoxLeft = GetValueInt("LEFT");
    *BoxRight = GetValueInt("RIGHT");
  }
}
