/* This may look like C, but it's really -*-c++-*- */
/*  lx_StatusMessage.h -- Mount message with mount's current status
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
#include "lx_gen_message.h"

extern long most_recent_focuser_position;

class lxStatusMessage : public lxGenMessage {
public:
  lxStatusMessage(int Socket,
		int ServerStatus,
		int ScopeStatus);
  lxStatusMessage(lxGenMessage *message);
  void process(void);

  int            GetServerStatus(void);
  int            GetScopeStatus(void);

  void           SetFocusPositionC14(long FocusData);
  long           GetFocusPositionC14(void);
  void           SetFocusPositionEsatto(long FocusData);
  long           GetFocusPositionEsatto(void);
};

// Allowed values of ServerStatus
#define LX_SERVER_READY		0x00
#define LX_SERVER_BUSY		0x14
#define LX_SERVER_BAD_COMMAND	0x15 // couldn't handle last command

// allowed values of CameraStatus
#define SCOPE_SHUTTER_OPEN	0x23
#define SCOPE_IO_BUSY		0x24
#define SCOPE_IDLE		0x25
