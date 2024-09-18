/* This may look like C code, but it is really -*-c++-*- */
/*  FITSMessage.h -- Camera message carrying image from the camera
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

#ifndef _FITSMESSAGE_H
#define _FITSMESSAGE_H

#include <string.h>
#include "gen_message.h"

// There is no response to this message.
// This message is generated in response to an ExposeMessage if the
// filename in the ExposeMessage was "-".

class FITSMessage : public GenMessage {
public:
  FITSMessage(int Socket,
	      const char *filename);
  FITSMessage(GenMessage *message);
  FITSMessage(int Socket,
	      size_t filesize,
	      void *filepointer);
  void process(void);
  void GetFITSFile(size_t *filesize,
		   void **filepointer);

  static void NextFilenameToUse(const char *filename);
  static char *NextFilename;
  static int  NextFilename_Length;
};

#endif
