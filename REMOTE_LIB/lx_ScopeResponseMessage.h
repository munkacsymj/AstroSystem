/* This may look like C code, but it is really -*-c++-*- */
/*  lx_ScopeResponseMessage.h -- Mount message with mount's response to a
 * general-purpose command
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
#ifndef _LXSCOPERESPONSEMESSAGE_H
#define _LXSCOPERESPONSEMESSAGE_H

#include "lx_gen_message.h"

enum ScopeResponseStatus {
  Okay,				// normal response
  TimeOut,			// response never completed
  Aborted,			// some other, non-descript error
};

class lxScopeResponseMessage : public lxGenMessage {
 public:
  lxScopeResponseMessage(int Socket,
			 char *MessageString,
			 ScopeResponseStatus Status);
 
  lxScopeResponseMessage(lxGenMessage *message);

  char                *GetMessageString(void);
  ScopeResponseStatus GetStatus(void);
};

#endif
