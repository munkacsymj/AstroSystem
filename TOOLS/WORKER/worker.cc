/*  worker.cc -- Handle analysis for simple_session
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

#include <unistd.h>
#include <sys/inotify.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <poll.h>
#include <limits.h>
#include <iostream>
#include <string>
#include <list>
#include <filesystem>

struct PrereqPath {
  std::filesystem::path full_path;
  std::string parent_directory;
  bool satisfied {false};
  int watch_fd;

  void check(void) {
    int r = access(full_path.c_str(), R_OK);
    satisfied = (r == 0);
    //std::cerr << "check() returns " << satisfied << " for "
    //	      << full_path << std::endl;
  }
};

static int inotify_fd = inotify_init();

std::string TimeString(void) {
  struct tm *time_data;
  time_t now = time(0);
  time_data = localtime(&now);
  char buffer[64];
  sprintf(buffer, "%02d:%02d:%02d",
	  time_data->tm_hour, time_data->tm_min, time_data->tm_sec);
  return std::string(buffer);
}
  

void FlushNotificationQueue(void) {
  // flush the notification event queue
  constexpr int buffer_size = sizeof(struct inotify_event) + NAME_MAX + 1;
  char buffer[buffer_size];
  struct inotify_event *ev = (struct inotify_event *) buffer;
  struct pollfd pfd;
  pfd.fd = inotify_fd;
  pfd.events = POLLIN;

  do {
    int poll_ret = poll(&pfd, 1, 0); // non-blocking check
    if (poll_ret == 0) {
      return;
    }
    int bytes = read(inotify_fd, ev, buffer_size);
    if (bytes < 0) {
      std::cerr << "FlushNotificationQueue::read() error return."
		<< std::endl;
      break;
    }
  } while(1);
  /*NOTREACHED*/
}

class Prerequisites {
public:
  bool Satisfied(void);
  void AddPrerequisite(const std::string &file);
private:
  std::list<PrereqPath *> all_prerequisites;
  bool satisfied {false};
  void UpdateSatisfied(void);
};

std::string ReadOneInputLine(WorkQueue &wq,
			     Prerequisites &pq,
			     bool &finished);

void DoTask(std::string task) {
  // fix a bug that results in tasks beginning with the word
  // TASKS. Once this bug in strategy.cc is fixed, this can be removed.
  if (task.substr(0,4) == "TASK") {
    task = task.substr(4);
  }
  std::cerr << TimeString() << " DoTask(\"" << task << "\")" << std::endl;
  if (task == "") return;
  
  int x = system(task.c_str());
  if (x) {
    std::cerr << TimeString() << "    ***Task returned error." << std::endl;
  }
}
  
void usage(void) {
  std::cerr << "usage: worker [-d /home/IMAGES/9-25-2020]" << std::endl;
  exit(-1);
}

//****************************************************************
//        main()
//****************************************************************
int main(int argc, char **argv) {
  int ch;
  const char *home_directory = nullptr;

  while((ch = getopt(argc, argv, "d:")) != -1) {
    switch(ch) {
    case 'd':
      home_directory = optarg;
      break;

    default:
      usage();
    }
  }

  WorkQueue wq(home_directory);
  Prerequisites pq;
  bool finished = false;
  std::cerr << TimeString() << " worker started." << std::endl;

  // main loop
  do {
    std::string task = ReadOneInputLine(wq, pq, finished);
    if (pq.Satisfied()) {
      DoTask(task);
    }
  } while(not finished);

  std::cerr << TimeString() << " worker received FINI message." << std::endl;
  return 0;
}

void
Prerequisites::UpdateSatisfied(void) {
  bool all_satisfied = true;
  for (auto x : all_prerequisites) {
    if (not x->satisfied) {
      x->check();
      if (not x->satisfied) {
	all_satisfied = false;
	break;
      }
    }
  }
  satisfied = all_satisfied;
}


bool Prerequisites::Satisfied(void) {
  // block until all existing prerequisites are satisfied
  FlushNotificationQueue();  

  UpdateSatisfied();
  while (not satisfied) {
    constexpr int buffer_size = sizeof(struct inotify_event) + NAME_MAX + 1;
    char buffer[buffer_size];
    struct inotify_event *ev = (struct inotify_event *) buffer;

    int bytes = read(inotify_fd, ev, buffer_size); // might block for a very long time
    if (bytes < 0) {
      std::cerr << "Prerequisites::Satisfied()::read() error return: "
		<< bytes << std::endl;
    }
    UpdateSatisfied();
  }
  std::cerr << TimeString() << " Prerequisites satisfied." << std::endl;
  return true;
}

void
Prerequisites::AddPrerequisite(const std::string &file) {
  PrereqPath *pp = new PrereqPath;
  pp->full_path = std::filesystem::path(file);
  std::cerr << "Adding new prerequisite file: " << file << std::endl;
  pp->parent_directory = pp->full_path.parent_path().string();
  pp->watch_fd = -1;
  pp->check();
  if (!pp->satisfied) {
    // is this directory already being watched?
    
    bool already_watched = false;
    for (auto x : all_prerequisites) {
      std::cerr << "Comparing " << x->parent_directory << " and "
		<< pp->parent_directory << std::endl;
      if (x->parent_directory == pp->parent_directory and
	  x->watch_fd >= 0) {
	already_watched = true;
	break;
      }
    }
    if (not already_watched) {
      std::cerr << TimeString()
		<< "Adding watch in directory " << pp->parent_directory << std::endl;
      pp->watch_fd = inotify_add_watch(inotify_fd, pp->parent_directory.c_str(), IN_MODIFY|IN_CREATE);
    }
  }
  all_prerequisites.push_back(pp);
}

//****************************************************************
//        ReadOneInputLine()
//    Locking: Locking/unlocking is completely embedded inside this
//    function. Queue starts off unlocked and finishes unlocked.
//****************************************************************
std::string
ReadOneInputLine(WorkQueue &wq,
		 Prerequisites &pq,
		 bool &finished) {
  static WQ_UID current_uid = WQ_None;
  finished = false;
  std::string ret_value;

  // No matter which way this "if" goes, the queue will be locked at
  // the end of the compound "if".
  if (current_uid == WQ_None) {
    current_uid = wq.GetFirstLineUID();
  } else {
    current_uid = wq.NextUIDWait(current_uid);
  }

  std::string current_line = wq.GetLine(current_uid);
  std::string keyword = current_line.substr(0, 4);
  if (keyword == "FINI") {
    finished = true;
    ret_value = "";
  } else if (keyword == "PREQ") {
    // prerequisite filename
    const size_t s_start = current_line.find_first_not_of(" \n", 4);
    const size_t s_end = current_line.find_last_not_of(" \n");
    const std::string prereq_filename = current_line.substr(s_start, s_end-s_start+1);
    pq.AddPrerequisite(prereq_filename);
    ret_value = "";
  } else if (keyword == "TASK") {
    wq.DeleteLine(current_uid);
    ret_value = current_line.substr(4);
  } else if (keyword == "DONE" || keyword == "" ||
	     keyword[0] == '\n' || keyword == "    ") {
    ret_value = "";
  } else {
    std::cerr << "ERROR: ReadOneInputLine(): invalid keyword: "
	      << keyword << std::endl;
    ret_value = "";
  }
  wq.UnlockQueue();
  return ret_value;
}


