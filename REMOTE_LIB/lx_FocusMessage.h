#include "lx_gen_message.h"
/*  lx_FocusMessage.h -- Mount message to control focus motor
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

// Response to this message is ALWAYS a StatusMessage

#define FOCUS_FLAG_RELATIVE 0x00
#define FOCUS_FLAG_ABSOLUTE 0x01

#define FOCUS_FLAG_C14      0x02
#define FOCUS_FLAG_ESATTO   0x04

class lxFocusMessage : public lxGenMessage {
public:
  lxFocusMessage(int Socket,
		 int FocusFlags,
		 int FocusTravelInMsec);
  lxFocusMessage(lxGenMessage *message);
  int GetFocusTravelInMsec(void);
  bool FocusTravelIsAbsolute(void);
  bool FocuserIsC14(void);
};
  
