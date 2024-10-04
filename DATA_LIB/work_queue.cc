/*  work_queue.cc -- managing a persistent FIFO queue
 *
 *  Copyright (C) 2020 Mark J. Munkacsy

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

#include "work_queue.h"
#include <Image.h>		// DateToDirname()
#include <unistd.h>
#include <string.h>		// memset()
#include <sys/types.h>
#include <sys/file.h>
#include <sys/inotify.h>

//****************************************************************
//        File Format
//  nnnnnn XXXXX<string>\n
//  nnnnnn = length of the record (line), including the trailing newline
//  XXXXX = UID of the record
//****************************************************************

WorkQueue::WorkQueue(const char *home_directory) {
  if (home_directory == nullptr) {
    home_directory = DateToDirname();
  }

  char *filename = (char *) malloc(strlen(home_directory)+32);
  sprintf(filename, "%s/work.queue", home_directory);
  queue_filename = (const char *) filename;

  // ensure that the file exists, but don't truncate an existing file
  fd = open(queue_filename, O_CREAT|O_RDWR|O_DSYNC|O_NONBLOCK, S_IRUSR | S_IWUSR);
  if (fd < 0) {
    perror("Error opening/creating work_queue:");
    return;
  }

  SyncFile();

  inotify_fd = inotify_init();
  inotify_wd = inotify_add_watch(inotify_fd, queue_filename, IN_MODIFY);
}

void
WorkQueue::GetLock(void) {
  int x = 0;

  while((x = flock(fd, LOCK_EX)) == EINTR) {
    ; // repeat
  }

  if (x) {
    perror("Error in WorkQueue::GetLock::flock()");
  }
}

void
WorkQueue::ReleaseLock(void) {
  int x = 0;
  x = flock(fd, LOCK_UN);
  if (x) {
    perror("Error in WorkQueue::ReleaseLock()");
  }
}

void
WorkQueue::SyncFile(void) {
  long bytes;
  char header[16];

  off_t current_start = lseek(fd, 0, SEEK_SET); // seek to start
  if (current_start < 0) {
    perror("SyncFile error: initial lseek:");
  }
  std::list<LineInfo *>::iterator it = all_lines.begin();

  do {
    bytes = read(fd, header, 12);
    if (bytes == 0) break;
    if (bytes != 12) {
      perror("Puzzling error in WorkQueue::SyncFile::read()");
    }
    header[12] = 0;

    int file_uid;
    int file_reclen;
    if (sscanf(header, "%d %d", &file_reclen, &file_uid) != 2) {
      fprintf(stderr, "Error parsing WorkQueue record header: %s\n",
	      header);
    }

    if (it == all_lines.end()) {
      // extending all_lines with new entry
      LineInfo *li = new LineInfo;
      li->uid = file_uid;
      li->line_start = current_start;
      li->line_length = file_reclen;
      all_lines.push_back(li);
    } else {
      LineInfo *xi = (*it);
      // verification
      if (file_uid != xi->uid) {
	fprintf(stderr, "WorkQueue::SyncFile(): Integrity check failed. UIDs %d and %d\n",
		file_uid, xi->uid);
      }
      it++;
    }

    current_start = lseek(fd, file_reclen-12, SEEK_CUR);
  } while (current_start >= 0);

  fprintf(stderr, "WorkQueue: sync() complete. %lu records.\n",
	  (unsigned long) all_lines.size());
}

WorkQueue::~WorkQueue(void) {
  close(fd);
  for (auto x : all_lines) {
    delete x;
  }
}

std::string
WorkQueue::GetLine(WQ_UID line_uid) {
  LineInfo *li = FindUID(line_uid);
  if (!li) {
    fprintf(stderr, "WorkQueue::GetLine(): Invalid UID lookup: %d\n",
	    line_uid);
    return std::string("");
  }

  lseek(fd, li->line_start, SEEK_SET); // seek to start
  const int line_length = li->line_length;
  if (line_length < 2000) {
    char buffer[line_length+1];
    int bytes = read(fd, buffer, line_length);
    if (bytes != line_length) {
      fprintf(stderr, "WorkQueue::GetLine:read() bad read: %d\n",
	      bytes);
    }
    buffer[line_length] = 0;

    return std::string(buffer+12);
  } else {
    fprintf(stderr, "WorkQueue::GetLine: line too long: %d\n", line_length);
  }
  return ""; // error return
}

LineInfo *
WorkQueue::FindUID(WQ_UID line_uid) {
  for (auto x : all_lines) {
    if (x->uid == line_uid) return x;
  }
  return nullptr;
}

void
WorkQueue::AddToQueue(const std::string &task) {
  GetLock();
  SyncFile();

  LineInfo *li = new LineInfo;
  li->uid = all_lines.size() * 7 + 1000;
  off_t where = lseek(fd, 0, SEEK_END);
  li->line_start = where;
  li->line_length = 13 + task.length();
  
  char buffer[li->line_length + 2];
  sprintf(buffer, "%06ld %05d%s\n",
	  li->line_length,
	  li->uid,
	  task.c_str());
  int bytes = write(fd, buffer, li->line_length);
  if (bytes != li->line_length) {
    fprintf(stderr, "WorkQueue::AddToQueue bad write: %d\n", bytes);
  }

  all_lines.push_back(li);
  ReleaseLock();
}

void
WorkQueue::DeleteLine(WQ_UID line_uid) {
  //GetLock();
  SyncFile();

  LineInfo *li = FindUID(line_uid);
  if (!li) {
    fprintf(stderr, "WorkQueue::DeleteLine(): Invalid UID lookup: %d\n",
	    line_uid);
  } else {
    lseek(fd, li->line_start+12, SEEK_SET); // seek to start of data
    int bytes = write(fd, "DONE", 4);
    if (bytes != 4) {
      fprintf(stderr, "WorkQueue::DeleteLine:write() bad write: %d\n",
	      bytes);
    }
  }
  //ReleaseLock();
}

WQ_UID
WorkQueue::GetFirstLineUID(void) {
  do {
    GetLock();
    SyncFile();

    if (all_lines.size() > 0) {
      return all_lines.front()->uid;
    }

    ReleaseAndWaitForChange();
  } while(1);
  /*NOTREACHED*/
}

WQ_UID
WorkQueue::NextUIDWait(WQ_UID uid) {
  do {
    GetLock();
    SyncFile();

    bool found = false;
    for (auto x : all_lines) {
      if (found) {
	return x->uid;
      }
    
      if (x->uid == uid) found = true;
    }
    if (not found) {
      fprintf(stderr, "WorkQueue::NextUIDWait(): bad UID: %d\n", uid);
    } else {
      ReleaseAndWaitForChange();
    }
  } while(1);
  /*NOTREACHED*/
}

void
WorkQueue::ReleaseAndWaitForChange(void) {
  off_t initial_length = lseek(fd, 0, SEEK_END);
  // now that we know that, we can release the lock
  ReleaseLock();
  do {
    struct inotify_event event;
    int bytes = read(inotify_fd, &event, sizeof(event));
    if (bytes != sizeof(event)) {
      fprintf(stderr, "WorkQueue::ReleaseAndWaitForChange: read() return err: %d\n",
	      bytes);
    }
  } while(lseek(fd, 0, SEEK_END) == initial_length);
}

