/*  cooler_indi.cc -- Implements user view of cooler inside the camera
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
#include "camera_api.h"
#include "scope_api.h"		// to get CameraPointsAt()
#include <iostream>
#include <libindi/indiapi.h>
#include <libindi/baseclient.h>
#include "astro_indi.h"
#include "cooler_indi.h"

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

int // return 1 if successful
CoolerCommand::Send(void) {
  if (!cooler) {
    static bool warning_already_issued {false};
    if (not warning_already_issued) {
      std::cerr << "CoolerCommand: No cooler found. Ignoring cooler command.\n";
      warning_already_issued = true;
    }
    return 0;
  }
  // cooler exists
  int return_code = 1;
  switch(this->mode) {
  case NO_COMMAND:
    std::cerr << "CoolerCommand::Send(): command contains NO_COMMAND. Logic err.\n";
    return_code = 0;
    break;

    //********************************
    //        MANUAL mode
    //********************************
  case MANUAL:
    if (cooler->cooler_on.getState() != ISS_ON) {
      cooler->cooler_off.setState(ISS_OFF);
      cooler->cooler_on.setState(ISS_ON);
      cooler->cooler_off.SendINDIUpdate(); // sends both values
    }
    if (cooler->cooler_manual.available and
	cooler->cooler_manual.getState() != ISS_ON) {
      cooler->cooler_manual.setState(ISS_ON);
      cooler->cooler_auto.setState(ISS_OFF);
      cooler->cooler_manual.SendINDIUpdate();
    }
    cooler->cooler_power.setValue(this->Power); // range 0..1
    cooler->cooler_power.SendINDIUpdate();
      
    break;

    //********************************
    //        SETPOINT mode
    //********************************
  case SETPOINT:
    if (cooler->cooler_on.getState() != ISS_ON) {
      cooler->cooler_off.setState(ISS_OFF);
      cooler->cooler_on.setState(ISS_ON);
      cooler->cooler_off.SendINDIUpdate(); // sends both values
    }
    if (cooler->cooler_auto.available and
	cooler->cooler_auto.getState() != ISS_ON) {
      cooler->cooler_auto.setState(ISS_ON);
      cooler->cooler_manual.setState(ISS_OFF);
      cooler->cooler_manual.SendINDIUpdate();
    }
    cooler->commanded_setpoint = this->Setpoint;
    cooler->ccd_temp.setValue(this->Setpoint);
    cooler->ccd_temp.SendINDIUpdate();
    break;

    //********************************
    //        TURN OFF
    //********************************
  case COOLER_OFF:
    if (not cooler->cooler_off.available) {
      std::cerr << "CoolerCommand: INDI Cooler is missing COOLER_OFF property.\n";
      return_code = 0;
    } else {
      cooler->cooler_off.setState(ISS_ON);
      cooler->cooler_on.setState(ISS_OFF);
      cooler->cooler_off.SendINDIUpdate(); // sends both values
      break;
    }
  }
  return return_code;
}

CCDCooler::CCDCooler(AstroDevice *device, const char *connection_port) :
  LocalDevice(device, connection_port), dev(device) {
  ; // this->DoINDIRegistrations();
}

CCDCooler::~CCDCooler(void) {
  ;
}

int // return 0 on error, 1 on success
CCDCooler::GetCoolerData(double *ambient_temp,
			 double *ccd_temp,
			 double *cooler_setpoint,
			 int    *cooler_power,
			 double *humidity,
			 int    *mode,
			 int    cooler_flags) {

  int timeout = 6;
  while (not this->cooler_off.available) {
    if (timeout-- <= 0) {
      INDIDisconnectINDI();
      std::cerr << "GetCoolerData: forced exit. cooler_off.available timeout\n";
      exit(-2);
    }
    sleep(1);
  }

  *ccd_temp = this->GetCCDTemp();
  *ambient_temp = 0.0;
  *cooler_setpoint = this->GetSetpoint();
  *cooler_power = this->GetPower();
  *humidity = (this->ccd_humidity.available ? this->ccd_humidity.getValue() : 0.0);
  if (this->cooler_off.getState() == ISS_ON) {
    *mode = 0; // off
  } else if (this->cooler_manual.available and this->cooler_manual.getState() == ISS_ON) {
    *mode = CCD_COOLER_ON;
  } else {
    *mode = (CCD_COOLER_ON | CCD_COOLER_REGULATING);
  }

  std::cerr << "GetCoolerData(): "
	    << "Cooler = ON="
	    << (this->cooler_on.getState() == ISS_ON ? "true" : "false")
	    << "/OFF="
	    << (this->cooler_off.getState() == ISS_ON ? "true" : "false")
	    << '\n';
  std::cerr << "    ccd_temp = " << *ccd_temp
	    << ", setpoint = " << *cooler_setpoint
    //<< ", manual_mode = " << ((this->cooler_manual.getState() == ISS_ON) ? "Man" : "off")
	    << "\n";
    
  return 1; // success
}

double
CCDCooler::GetPower(void) {
  std::cerr << "GetPower: cooler_power.avail = "
	    << this->cooler_power.available;
  if (this->cooler_power.available) {
    std::cerr << ", value = "
	      << this->cooler_power.getValue();
  }
  std::cerr << '\n';
  return (this->cooler_power.available ? this->cooler_power.getValue() : 0.0);
}

void
CCDCooler::SetPower(double power) {
  this->cooler_power.setValue(power);
  this->dev->local_client->sendNewNumber(this->cooler_power.property->indi_property);
}



