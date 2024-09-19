/* This may look like C code, but it is really -*-c++-*- */
/*  cooler.h -- Manages the QHY268M cooler
 *
 *  Copyright (C) 2021 Mark J. Munkacsy

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

#ifndef _COOLER_H
#define _COOLER_H

enum CoolerModeRequest { COOLER_OFF, COOLER_MAN, COOLER_AUTO, COOLER_TERMINATE };
enum CoolerMode {
  //COOLER_RAMPUP,
  //COOLER_RAMPDOWN,
  COOLER_POWEROFF,
  COOLER_MANPWM,
  COOLER_REGULATING,
  COOLER_ERROR,
  COOLER_TERMINATED,
};

struct CoolerData {
  // Commands flowing to the cooler controller
  double CoolerTempCommand;
  int    CoolerPWMCommand; // 0..255
  CoolerModeRequest CoolerModeDesired;

  // Status flowing back from the cooler controller
  int CoolerCurrentPWM;
  double CoolerCurrentChipTemp;
  bool ambient_avail;
  double CoolerCurrentAmbient;
  double CurrentHumidity;
  double CurrentPressure;
  CoolerMode CoolerCurrentMode;
};

void InitCooler(void);
CoolerData *GetCoolerData(void);

#endif
