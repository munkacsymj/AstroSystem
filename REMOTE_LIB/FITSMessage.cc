/*  FITSMessage.cc -- Camera message carrying image from the camera
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
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>		// pick up fstat()
#include <fcntl.h>		// pick up open()
#include <unistd.h>		// pick up close()
#include <stdio.h>
#include "FITSMessage.h"

//
// Message format:
//
// bytes 0-3	size
//       4	message ID
//       5-end  contents of the file. 
//
//

////////////////////////////////////////////////////////////////
//        Constructors
////////////////////////////////////////////////////////////////

static int filelength(const char *filename) {
  struct stat file_status;

  if(stat(filename, &file_status) < 0) {
    return -1;
  }

  return file_status.st_size;
}

FITSMessage::FITSMessage(int Socket,
			 const char *filename) :
  GenMessage(Socket, filelength(filename)+5) {
	     
    content[4] = FITSMessageID;
    int fd;

    fd = open(filename, O_RDONLY, 0);
    if(fd < 0) {
      perror("FITSMessage");
      return;
    }
    fetch_bytes(fd, content+5, GenMessSize-5);
    close(fd);
  }


FITSMessage::FITSMessage(GenMessage *message) :
  GenMessage(message) {

    if(GenMessSize < 1000 ||
       MessageID() != FITSMessageID) {
      fprintf(stderr,
	      "FITSMessage: constructor reasonableness check failed.\n");
    }
  }

FITSMessage::FITSMessage(int Socket,
			 size_t filesize,
			 void *filepointer) :
  GenMessage(Socket, 5+filesize) {

  content[4] = FITSMessageID;
  memcpy(content+5, filepointer, filesize);
}

void
FITSMessage::GetFITSFile(size_t *filesize,
			 void **filepointer) {
  *filesize = GenMessSize-5;
  *filepointer = (void *) (content+5);
}
