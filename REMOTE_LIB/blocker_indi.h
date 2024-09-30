/* This may look like C code, but it is really -*-c++-*- */
/*  blocker_indi.h -- Wrapper for pthread_mutex_lock()
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

#include <errno.h>		// defines ETIMEDOUT return value from Wait()
#include <pthread.h>

//****************************************************************
//        Helper class: Blocker
//****************************************************************
class Blocker {
public:
  Blocker(void) {
    mutex = PTHREAD_MUTEX_INITIALIZER;
    condition = PTHREAD_COND_INITIALIZER; }
  ~Blocker(void) { pthread_cond_destroy(&condition); }

  void Setup(void) {
    pthread_mutex_lock(&mutex);
    data_avail = false;
    pthread_mutex_unlock(&mutex); }

  // if milliseconds == 0, will block forever
  int Wait(int milliseconds = 0);
    
  void Signal(void) {
    pthread_mutex_lock(&mutex);
    data_avail = true;
    pthread_cond_broadcast(&condition);
    pthread_mutex_unlock(&mutex); }

protected:
  bool data_avail;
  pthread_mutex_t mutex;
  pthread_cond_t condition;
};

