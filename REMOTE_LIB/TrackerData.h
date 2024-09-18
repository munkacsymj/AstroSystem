/* This may look like C code, but it is really -*-c++-*- */
/*  TrackerData.h -- Implements camera-side star tracker for guided exposures
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

#ifndef _TRACKER_DATA_H
#define _TRACKER_DATA_H

#include <sys/types.h>		// for key_t

class TrackerStatistics {
public:
  double track_quality;		// 0 -> 1.0
  int    track_acquired;	// 0, 1
};

class TrackerData {
private:
  TrackerData *shmem_addr;
  double suggested_track_exposure_time;
  TrackerStatistics tracker_statistics;
  int    tracking_is_optional;
  double seconds_to_track;
  int    tracker_startup_complete; // 0, 1
public:
  TrackerData(double SuggestedTrackExposureTime, int *shared_id);
  TrackerData(int shared_id);

  // Modifying methods
  void SetTrackingOptional(void);
  void SetTrackingRequired(void);
  void SetTrackerStatistics(const TrackerStatistics &data);
  void SetSecondsToTrack(double SecondsToTrack);

  // Querying methods
  double GetExposureTime(void) {
    return shmem_addr->suggested_track_exposure_time;
  }
  int GetTrackingOptional(void);
  const TrackerStatistics GetTrackerStatistics(void);
  double GetSecondsToTrack(void);

  // Control methods
  int IsConnected(void);
  void BlockUntilTrackAcquired(void);
  void TellClientToContinue(void);
};

#endif
