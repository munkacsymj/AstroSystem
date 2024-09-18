// This may look like C code, but it is really -*- C++ -*-
/*  work_queue.h -- managing a persistent FIFO queue
 *
 *  Copyright (C) 2020 Mark J. Munkacsy
 *
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
#ifndef _WORK_QUEUE_H
#define _WORK_QUEUE_H

#include <string>
#include <list>

typedef int WQ_UID;
constexpr WQ_UID WQ_None = -1;

struct LineInfo {
  WQ_UID uid;
  long line_start;		// first char of the UID
  long line_length;		// all chars, incl header & newline
};

class WorkQueue {
public:
  WorkQueue(const char *home_directory = nullptr);
  ~WorkQueue(void);

  WQ_UID GetFirstLineUID(void); // may block to wait for data
  WQ_UID NextUIDWait(WQ_UID uid); // may block to wait for data

  void AddToQueue(const std::string &task); 
  void LockQueue(void) { GetLock(); }
  void UnlockQueue(void) { ReleaseLock(); }

  std::string GetLine(WQ_UID line_uid);
  void DeleteLine(WQ_UID line_uid);

private:
  const char *queue_filename;
  
  std::list<LineInfo *> all_lines;

  void GetLock(void);
  void ReleaseLock(void);
  void SyncFile(void); // must already be locked!
  void ReleaseAndWaitForChange(void);

  LineInfo *FindUID(WQ_UID line_uid);

  int fd;
  int inotify_fd;
  int inotify_wd; // watch descriptor
};

#endif
