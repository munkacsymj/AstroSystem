/*  blocker.cc -- Implements INDI helper mutex/thread-coordination
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

#include <sys/time.h>
#include <iostream>
#include "blocker_indi.h"

int
Blocker::Wait(int milliseconds) {
  int retcode = 0; // 0 means no timeout
  struct timeval now;
  struct timespec timeout;

  pthread_mutex_lock(&mutex);
  if (not this->data_avail) {
    gettimeofday(&now, nullptr);
    int seconds = (int) milliseconds/1000;
    int microseconds = 1000*(milliseconds - seconds*1000);
    //std::cerr << "Now = " << now.tv_sec << "/" << now.tv_usec
    //	      << ", Blocker::Wait(): seconds = " << seconds
    //	      << ", usec = " << microseconds << std::endl;
    now.tv_sec += seconds;
    now.tv_usec += microseconds;
    if (now.tv_usec >= 1000000) {
      now.tv_sec++;
      now.tv_usec -= 1000000;
    }
    timeout.tv_sec = now.tv_sec;
    timeout.tv_nsec = now.tv_usec*1000;
    //std::cerr << "...tv_sec = " << timeout.tv_sec
    //	      << ", ...tv_nsec = " << timeout.tv_nsec << std::endl;

    while(retcode != ETIMEDOUT and not data_avail) {
      if (milliseconds) {
	retcode = pthread_cond_timedwait(&condition, &mutex, &timeout);
      } else {
	pthread_cond_wait(&condition, &mutex);
	retcode = 0;		// no timer, so retcode is always non-timeout value
      }
    }
  }
  pthread_mutex_unlock(&mutex);
  return retcode;
}

    

