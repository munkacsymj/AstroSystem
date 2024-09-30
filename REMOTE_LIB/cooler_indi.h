/* This may look like C code, but it is really -*-c++-*- */
/*  cooler_indi.h -- Implements user view of cooler
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

#include "blocker_indi.h"
#include "astro_indi.h"

class CCDCooler : public LocalDevice {
public:
  CCDCooler(AstroDevice *device, const char *connection_port);
  ~CCDCooler(void);

  enum CCDState { OFF, MANUAL, REGULATING };
  CCDState GetState(void);
  void SetState(CCDState state);
  
  bool PowerAvail(void) { return cooler_power.available; }
  double GetPower(void);
  void SetPower(double power);
  
  bool TempAvail(void) { return ccd_temp.available; }
  double GetSetpoint(void) { return commanded_setpoint; }
  double GetCCDTemp(void) { return ccd_temp.getValue(); }

  bool HumidityAvail(void) { return ccd_humidity.available; }
  double GetHumidity(void) { return ccd_humidity.getValue(); }

  //void DoINDIRegistrations(void);

  // return 0 on error, 1 on success
  int GetCoolerData(double *ambient_temp,
		    double *ccd_temp,
		    double *cooler_setpoint,
		    int    *cooler_power,
		    double *humidity,
		    int    *mode,
		    int    cooler_flags);

private:
  double commanded_setpoint;
  enum { C_OFF, C_MANUAL, C_SETPONT } commanded_mode;
  Blocker cfw_blocker;
  AstroDevice *dev {nullptr};
  
  AstroValueNumber ccd_temp{AstroValueNumber(this,"CCD_TEMPERATURE", "CCD_TEMPERATURE_VALUE")};
  AstroValueNumber ccd_ramp_slope{AstroValueNumber(this,"CCD_TEMP_RAMP", "RAMP_SLOPE")};
  AstroValueNumber ccd_ramp_threshold{AstroValueNumber(this,"CCD_TEMP_RAMP", "RAMP_THRESHOLD")};
  AstroValueSwitch cooler_on{AstroValueSwitch(this,"CCD_COOLER", "COOLER_ON")};
  AstroValueSwitch cooler_off{AstroValueSwitch(this,"CCD_COOLER", "COOLER_OFF")};
  AstroValueNumber cooler_power{AstroValueNumber(this,"CCD_COOLER_POWER", "CCD_COOLER_VALUE")}; // 0..100
  AstroValueNumber ccd_humidity{AstroValueNumber(this,"CCD_HUMIDITY", "HUMIDITY")}; // 0..100
  AstroValueSwitch cooler_auto{AstroValueSwitch(this,"CCD_COOLER_MODE", "COOLER_AUTOMATIC")};
  AstroValueSwitch cooler_manual{AstroValueSwitch(this,"CCD_COOLER_MODE", "COOLER_MANUAL")};

  friend class CoolerCommand;
};

extern CCDCooler *cooler;

