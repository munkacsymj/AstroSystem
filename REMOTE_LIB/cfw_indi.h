/* This may look like C code, but it is really -*-c++-*- */
/*  cfw_indi.h -- Implements user view of color filter wheel
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

class CFW_INDI : public LocalDevice {
public:
  // It's okay to create a CFW_INDI with no corresponding
  // AstroDevice. (This is what happens when no CFW is present.) In
  // that case, CFW_INDI will revert to "dumb" mode.
  CFW_INDI(AstroDevice *device, const char *connection_port);
  ~CFW_INDI(void);

  bool CFWPresent(void) const { return cfw_slot.available; }

  int NumCFWPositions(void);

  int PositionLastRequested(void) const { return commanded_position; }

  int CurrentPosition(void);

  bool HasBlackFilter(void) { return false; } // for ST-10XME w/shutter

  void MoveFilterWheel(int position, bool block=false);
  void WaitForFilterWheel(void);

  void DoINDIRegistrations(void);

private:
  int commanded_position;
  Blocker blocker;
  AstroDevice *dev {nullptr};
  
  AstroValueNumber cfw_slot{AstroValueNumber(this,"FILTER_SLOT", "FILTER_SLOT_VALUE")};

  friend void CFWPropertyUpdate(INDI::Property property);

};

extern CFW_INDI *cfw;

