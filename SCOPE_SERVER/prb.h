/* This may look like C code, but it is really -*-c++-*- */
/*  prb.h -- Manages shared memory ring buffer
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

#ifndef _PRB_H
#define _PRB_H

// Protected Ring Buffer
class PRB {
public:
  PRB(int size);
  ~PRB(void);

  unsigned int buflen;

  // "i" should be incremented with
  // i = (i+1) % this->buflen;
  unsigned int &Get(unsigned int i) { return buffer[i]; }

  unsigned int NumPoints(void);

  void AddNewData(unsigned int value);
  unsigned int PopData(void);

private:
  unsigned int *buffer;
  unsigned int ring_start; // index of first valid entry
  unsigned int ring_next; // index of next valid value
  //sem_t write_protect_semaphore;
};

#endif
