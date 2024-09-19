/*  prb.cc -- Manages shared memory ring buffer
 *
 *  Copyright (C) 2022 Mark J. Munkacsy

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

#include "prb.h"
#include <semaphore.h>

PRB::PRB(int size) : buflen(size), ring_start(0), ring_next(0), buffer(new unsigned int[size]) {
#if 0
  int res = sem_init(&write_protect_semaphore,
		     0, // shared across threads
		     1); // initialized to unlocked state
  if (res) {
    perror("sem_init(): initialization error: ");
    exit(-2);
  }
#endif
  ;
}

PRB::~PRB(void) {
  delete [] buffer;
#if 0
  sem_destroy(&write_protect_semaphore);
#endif
}

unsigned int PRB::NumPoints(void) {
  if (ring_start > ring_next) {
    return buflen - (ring_start - ring_next);
  } else {
    return ring_next - ring_start;
  }
}

void PRB::AddNewData(unsigned int value) {
  buffer[ring_next] = value;
  ring_next = (ring_next+1)%buflen;
}

#define PRB_EMPTY 0xffff;
unsigned int PRB::PopData(void) {
  if (ring_start == ring_next) return PRB_EMPTY;
  const unsigned int value = buffer[ring_start];
  ring_start = (ring_start+1)%buflen;
  return value;
}
				   

//****************************************************************
//        end of PRB
//****************************************************************

