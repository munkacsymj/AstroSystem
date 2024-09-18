/*  TrackerData.cc -- Implements camera-side star tracker for guided exposures
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
#include <stdio.h>
#include <unistd.h>		// sleep()
#include <sys/shm.h>		// shmget()
#include <TrackerData.h>

static const int shared_mem_size = sizeof(TrackerData);

TrackerData::TrackerData(double SuggestedTrackExposureTime, int *shared_id) {
  // need to allocated shared memory
  *shared_id = shmget(IPC_PRIVATE,
		      sizeof(TrackerData),
		      IPC_CREAT);
  if(*shared_id < 0) {
    perror("Cannot allocated shared memory for tracking");
    return;
  }

  shmem_addr = (TrackerData *) shmat(*shared_id, 0, 0);

  shmem_addr->shmem_addr = 0;
  shmem_addr->suggested_track_exposure_time = SuggestedTrackExposureTime;
  shmem_addr->tracker_statistics.track_quality = 0.0;
  shmem_addr->tracker_statistics.track_acquired = 0;
  shmem_addr->tracker_startup_complete = 0;
  shmem_addr->tracking_is_optional = 1;
  shmem_addr->seconds_to_track = 0.0;

}

TrackerData::TrackerData(int shared_id) {
  shmem_addr = (TrackerData *) shmat(shared_id, 0, 0);
}
  
// Modifying methods
void
TrackerData::SetTrackingOptional(void) {
  shmem_addr->tracking_is_optional = 1;
}

void
TrackerData::SetTrackingRequired(void) {
  shmem_addr->tracking_is_optional = 0;
}

void
TrackerData::SetTrackerStatistics(const TrackerStatistics &data) {
  shmem_addr->tracker_statistics = data;
}

void
TrackerData::SetSecondsToTrack(double SecondsToTrack) {
  shmem_addr->seconds_to_track = SecondsToTrack;
}

// Querying methods
int
TrackerData::GetTrackingOptional(void) {
  return shmem_addr->tracking_is_optional;
}

const TrackerStatistics 
TrackerData::GetTrackerStatistics(void) {
  return shmem_addr->tracker_statistics;
}

double
TrackerData::GetSecondsToTrack(void) {
  return shmem_addr->seconds_to_track;
}

// Control methods
void
TrackerData::BlockUntilTrackAcquired(void) {
  while(shmem_addr->tracker_startup_complete == 0) {
    sleep(1);
  }
}

void
TrackerData::TellClientToContinue(void) {
  shmem_addr->tracker_startup_complete = 1;
}
