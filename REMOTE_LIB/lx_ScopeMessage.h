/* This may look like C code, but it is really -*-c++-*- */
/*  lx_ScopeMessage.h -- Mount message with general-purpose command
 *  to be passed directly to the mount controller 
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
#ifndef _LXSCOPEMESSAGE_H
#define _LXSCOPEMESSAGE_H

#include "lx_gen_message.h"

// Response to this message is ALWAYS a ScopeResponseMessage

enum ExecutionChoices {
  RunFast,
  RunMedium,
  RunSlow };

// FixedLength => the response from the mount is always the same
// number of characters.
//
// StringResponse => the response from the mount is always a string
// that is terminated with a "#"
//
// Nothing => The mount will not respond
//
// MixedModeResponse => the mount will either respond with a
// single-character response or will respond with a string terminated
// with a "#". The array SingleCharacterResponses contains the list of
// responses that are a single character without any trailing "#". If
// the first character received from the mount is *not* one of the
// SingleCharacterResponses, then that character is the first of the
// string. 
//

enum ResponseTypeChoices {
  FixedLength,
  StringResponse,
  MixedModeResponse,
  Nothing };

class lxScopeMessage : public lxGenMessage {
 public:
  lxScopeMessage(int Socket,
		 const char *MessageString,
		 ExecutionChoices ExecutionTime,
		 ResponseTypeChoices ResponseType,
		 int ResponseCharCount,
		 const char *SingleCharResponseArray = 0);

  lxScopeMessage(lxGenMessage *message);

  const char         *GetMessageString(void);
  ExecutionChoices    GetExecutionTime(void);
  ResponseTypeChoices GetResponseType(void);
  int                 GetResponseCharCount(void);
  // the array of single character responses (terminated with a
  // trailing null) will be put into the character array pointed to by
  // "buffer".
  void                GetSingleCharacterResponses(char *buffer);
};

#endif
