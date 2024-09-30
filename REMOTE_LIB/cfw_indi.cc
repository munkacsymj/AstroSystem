/*  cfw_indi.cc -- Implements user view of color filter wheel
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

#include <iostream>

#include "astro_indi.h"
#include "cfw_indi.h"

// Remember: "device" might be nullptr.
CFW_INDI::CFW_INDI(AstroDevice *device, const char *connection_port) :
  LocalDevice(device, connection_port), dev(device) {
  if (device) {
    this->DoINDIRegistrations();
    dev->indi_device.watchProperty(cfw_slot.property_name,
				   [this](INDI::Property p) {
				     std::cerr << "CFW Property changed.\n";
				     this->blocker.Signal();
				   },
				   INDI::BaseDevice::WATCH_UPDATE);
  }
}

int
CFW_INDI::NumCFWPositions(void) {
  if (this->dev == nullptr) {
    return 0;
  }
  const int low = (int) (0.5 + cfw_slot.getMin());
  const int high = (int) (0.5 + cfw_slot.getMax());
  return 1+high-low;
}

int
CFW_INDI::CurrentPosition(void) {
  if (this->dev == nullptr) return 0;
  return (int) (0.5 + cfw_slot.getValue());
}

void
CFW_INDI::WaitForFilterWheel(void) {
  if (this->dev == nullptr) return;
  do {
    blocker.Wait(10*1000); // wait for position update
    if (CurrentPosition() == PositionLastRequested()) {
      return;
    } else {
      // is there an opportunity for a race problem here?
      blocker.Setup();
    }
  } while(1); // needs a timeout
}

void
CFW_INDI::MoveFilterWheel(int position, bool block) {
  this->commanded_position = position;
  if (this->dev == nullptr) return;
  
  if (block) {
    blocker.Setup(); // should return immediately
  }

  cfw_slot.setValue(position);
  this->dev->local_client->sendNewNumber(this->cfw_slot.property->indi_property);
  if (block) {
    int retval = blocker.Wait(10/*seconds*/*1000/*milliseconds*/);
    if (retval) {
      std::cerr << "CFW::MoveFilterWheel: "
		<< strerror(retval) << std::endl;
    }
  }
}

void CFW_INDI::DoINDIRegistrations(void) {
  if (this->dev != nullptr) {
    this->dev->lookups.push_back(&this->cfw_slot);
  }
}
