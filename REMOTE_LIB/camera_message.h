/* This may look like C code, but it is really -*-c++-*- */
/*  camera_message.h -- Camera message to make an exposure
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
#include "gen_message.h"
#include <string>
#include <unordered_map>
using namespace std;

#define CMD_EXPOSE 1
#define CMD_COOLER 2
#define CMD_STATUS 3
#define CMD_FILTER_CONFIG 4
#define CMD_SHUTDOWN 5

#define COMBINE_AVERAGE 1
#define COMBINE_MEDIAN 2
#define COMBINE_MEDIANAVERAGE 3

#define PIXEL_UINT16 0
#define PIXEL_UINT32 1
#define PIXEL_FLOAT 2

class CameraMessage : public GenMessage {
public:
  CameraMessage(int Socket,
		unsigned char command);
  CameraMessage(GenMessage *message);
  ~CameraMessage(void) {;}
  void process(void);
  int send(void);

  void SetFullFrameMode(void);
  void SetSubFrameMode(unsigned int BoxBottom,
		       unsigned int BoxTop,
		       unsigned int BoxLeft,
		       unsigned int BoxRight);

  void GetSubFrameData(unsigned int *BoxBottom,
		       unsigned int *BoxTop,
		       unsigned int *BoxLeft,
		       unsigned int *BoxRight);

  // Messages from newworkshop to jellybean have automatic unique ID
  // assigned by the message constructor. Messages from jellybean back
  // to newworkshop will echo the unique ID of the corresponding
  // request message.
  int GetUniqueID(void) { return unique_id; }
  void SetUniqueID(int n) { unique_id = n; }

  ////////////////////////////////
  //        Set
  ////////////////////////////////
  void SetExposure(double time_secs);
  void SetFilter(char filter_letter);
  void SetBinning(int binning);
  void SetTransferEachImage(bool xfer_each);
  void SetLocalImageName(const char *filename);
  void SetCameraMode(int mode);
  void SetCameraGain(int gain);
  void SetSubtractImage(const char *filename);
  void SetScaleImage(const char *filename);
  void SetLinearize(bool linearize);
  void SetCoolerTemp(double temp_C); // measured temp
  void SetCoolerSetpoint(double temp_C); // desired temp
  void SetCoolerPower(double power_fraction); // 0..1
  void SetAmbientTemp(double temp_C);
  void SetRepeatCount(int repeat);
  void SetCombineType(int combine_type);
  void SetCompressImage(bool compress);
  void SetQuery(void) { SetKeywordValue("STATUS_QUERY", "1"); }
  void SetShutterOpen(bool shutter_open);
  void SetPixelFormat(int pixel_format); // use #defines above
  void SetOffset(int offset);
  void SetHumidity(double humidity);
  void SetUSBTraffic(double usb_traffic);

  void SetKeywordValue(const string keyword, const string value);

  ////////////////////////////////
  //        Test/Get
  ////////////////////////////////
  int GetCommand(void) { return command; }

  bool ExposureTimeAvail(void) { return KeywordPresent("EXPOSURE"); }
  double GetExposureTime(void) { return GetValueDouble("EXPOSURE"); }

  bool FilterAvail(void) { return KeywordPresent("FILTER"); }
  char GetFilterLetter(void) { return GetValueString("FILTER")[0]; }

  bool BinningAvail(void) { return KeywordPresent("BIN"); }
  int GetBinning(void) { return GetValueInt("BIN"); }

  bool TransferEachImageAvail(void) { return KeywordPresent("XFEREACH"); }
  bool GetTransferEachImage(void) { return GetValueBool("XFEREACH"); }

  bool LocalImageNameAvail(void) { return KeywordPresent("IMAGE"); }
  string GetLocalImageName(void) { return GetValueString("IMAGE"); }

  bool CameraOffsetAvail(void) { return KeywordPresent("OFFSET"); }
  int GetOffset(void) { return GetValueInt("OFFSET"); }

  bool CameraModeAvail(void) { return KeywordPresent("MODE"); }
  int GetCameraMode(void) { return GetValueInt("MODE"); }

  bool CameraGainAvail(void) { return KeywordPresent("GAIN"); }
  int GetCameraGain(void) { return GetValueInt("GAIN"); }

  bool SubtractImageAvail(void) { return KeywordPresent("SUBTRACT"); }
  string GetSubtractImage(void) { return GetValueString("SUBTRACT"); }

  bool ScaleImageAvail(void) { return KeywordPresent("SCALE"); }
  string GetScaleImage(void) { return GetValueString("SCALE"); }

  bool LinearizeAvail(void) { return KeywordPresent("LINEARIZE"); }
  bool GetLinearize(void) { return GetValueBool("LINEARIZE"); }

  bool CoolerTempAvail(void) { return KeywordPresent("CCD_TEMP"); }
  double GetCoolerTemp(void) { return GetValueDouble("CCD_TEMP"); }

  bool AmbientTempAvail(void) { return KeywordPresent("AMBIENT_TEMP"); }
  double GetAmbientTemp(void) { return GetValueDouble("AMBIENT_TEMP"); }

  bool CoolerPowerAvail(void) { return KeywordPresent("POWER"); }
  double GetCoolerPower(void) { return GetValueDouble("POWER"); }

  bool CoolerSetpointAvail(void) { return KeywordPresent("SETPOINT"); }
  double GetCoolerSetpoint(void) { return GetValueDouble("SETPOINT"); }

  bool HumidityAvail(void) { return KeywordPresent("HUMIDITY"); }
  double GetHumidity(void) { return GetValueDouble("HUMIDITY"); }

  bool CoolerModeAvail(void) { return KeywordPresent("COOLER_MODE"); }
  string GetCoolerMode(void) { return GetValueString("COOLER_MODE"); }

  bool RepeatCountAvail(void) { return KeywordPresent("REPEAT"); }
  int GetRepeatCount(void) { return GetValueInt("REPEAT"); }

  bool PixelFormatAvail(void) { return KeywordPresent("PIXEL_FORMAT"); }
  int GetPixelFormat(void) { return GetValueInt("PIXEL_FORMAT"); }

  bool CombineTypeAvail(void) { return KeywordPresent("COMBINE"); }
  int GetCombineType(void) { return GetValueInt("COMBINE"); }

  bool CompressAvail(void) { return KeywordPresent("COMPRESS"); }
  bool GetCompress(void) { return GetValueBool("COMPRESS"); }

  bool ShutterAvail(void) { return KeywordPresent("SHUTTER_OPEN"); }
  bool GetShutterOpen(void) { return GetValueBool("SHUTTER_OPEN"); }

  bool USBTrafficAvail(void) { return KeywordPresent("USBTRAFFIC"); }
  double GetUSBTraffic(void) { return GetValueDouble("USBTRAFFIC"); }

  bool IsQuery(void) { return (KeywordPresent("STATUS_QUERY") and
			       GetValueBool("STATUS_QUERY")); }

  bool IsFullFrame(void) { return (KeywordPresent("TOP") &&
				   KeywordPresent("BOTTOM") &&
				   KeywordPresent("LEFT") &&
				   KeywordPresent("RIGHT")); }
				   

  bool KeywordPresent(string keyword);
  string GetValueString(string keyword);
  double GetValueDouble(string keyword);
  int GetValueInt(string keyword);
  bool GetValueBool(string keyword);

private:
  std::unordered_map<string, string> key_values;
  int command;
  int unique_id;

  unsigned char *ResetHeader(void);
};

/* Must keep these definitions matching those found in the ioctl
   definitions found in the linux device driver (the ioctl
   definitions). 
   */

#define EM_BIN_378WIDE       1
#define EM_BIN_EXT3          2
#define EM_BIN_INT3          3
#define EM_BIN_INT3DBLSAMPLE 4

