/* This may look like C code, but it is really -*-c++-*- */
/*  lx_TrackMessage.h -- Mount message with small guiding command
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

// Response to this message is ALWAYS a StatusMessage

class lxTrackMessage : public lxGenMessage {
public:
  lxTrackMessage(int Socket,
		 int NorthMSec,
		 int EastMSec);
  lxTrackMessage(lxGenMessage *message);
  int GetTrackNorthTimeInMsec(void);
  int GetTrackEastTimeInMsec(void);
};
  
