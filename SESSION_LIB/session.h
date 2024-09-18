// This may look like C code, but it is really -*- C++ -*-
/*  session.h -- manages overall behavior of a night-long observing session
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
#ifndef _SESSION_H
#define _SESSION_H

#include <julian.h>
#include <Image.h>
#include <Filter.h>
#include <astro_db.h>
#include <system_config.h>
#include "schedule.h"
#include "work_queue.h"
#include <string>
#include <list>

using namespace std;

struct GroupInfo {
  string groupname;
  double priority;
};

struct SessionOptions {
  int do_focus;			// 1=focus needed during session
  int leave_cooler_off;		// 1=don't turn on cooler
  int keep_cooler_running;	// 1=don't shutdown cooler at session end
  int default_dark_count;	// typical qty of darks to combine
  int update_mount_model;	// 1=update mount model each time finder succeeds
  int trust_focus_star_position; // 1=trust (no need for finder exp)
  int no_session_file;		// 1=don't read session file
  int use_pec;			// 1=load PEC tables into mount
  int park_at_end;		// 1=park at end of session
  int use_work_queue;
};

void SetDefaultOptions(SessionOptions &s);

class Session {
public:
  // normal session constructor
  Session(JULIAN start_time,
	  const char *session_file,
	  const SessionOptions &options);
  // "simple" session constructor that needs no session file
  Session(JULIAN start_time,
	  JULIAN end_time,
	  const char *logfile_name,
	  const SessionOptions &options);

  ~Session(void);

  const char *mount_error_file;
  const SystemConfig configuration;

  // Prints stuff into the session log file
  void PrintSessionTimes(void);

  // This method may take a long time to execute.  It will check to
  // see if a dark Image is available for the specified exposure
  // time.  If not, it will create a dark image.  This may involve
  // multiple exposures lasting several times as long as the specified
  // exposure time. You can fetch the resulting dark image using the
  // dark() method.
  void verify_dark_available(double exposure_time_secs, int num_exposures);

  // Provide a pointer to a dark image. This may take a long time to
  // execute if you didn't previously "verify_dark_available()".
  Image *dark(double exposure_time_secs, int num_exposures);
  const char *dark_name(double exposure_time_secs, int num_exposures, bool defer_exposure);

  // Provide a pointer to a flat.
  const Image *flat(void);
  const char *flat_filename(void) { return flatfilename; }
  const Image *flat(Filter *filter);
  const char *flat_filename(Filter *filter);

  const char *focus_log(void) { return focuslogfilename; }

  // Execute a session.  Start up the cooler. Focus. Align. Get
  // stuff. Shutdown.
  // During execution of the session it is wise to perform a
  // StatusCheck() fairly frequently. 
  void execute(void);

  enum TaskInfo {
    TASK_BUSY,			// task is busy (and happy)
    TASK_TROUBLED,		// task is busy (and unhappy)
    TASK_RESCHEDULING,		// task is rebuilding a schedule
    TASK_OVER,			// between tasks
    TASK_READY_TO_SLEEP,	// task getting ready to sleep
    TASK_QUIT_AS_REQUESTED,	// task was asked to quit and has done so
  };

  enum SessionInfo {
    SESSION_OKAY,		// session is busy (and happy)
    QUIT_TASK,			// quit the current task & pass
				// control back to the session
  };

  SessionInfo StatusCheck(TaskInfo t, int sleep_time_in_seconds=0);

  // Enter a message into the session log. Same format stuff as
  // fprintf.
#define LOG_INFO 0
#define LOG_ERROR 1
  
  void log(int level, const char *format, ...);
  void PutFileIntoLog(int level, const char *filename);

  double FocusCheckMinutes(void) { return focus_check_periodicity_minutes; }

  void EveningDate(int &day, int &month, int &year);
  void MorningDate(int &day, int &month, int &year);

  JULIAN SchedulingStartTime(void) const { return session_start_time; }
  JULIAN SchedulingEndTime(void) const { return termination_time; }

  // "synchronous" is either 0 or 1.  If 0, the background tasks are
  // run asynchronously, with all tasks kicked off in parallel.  If 1,
  // the background tasks are run one at a time until it is finished,
  // then the next one is kicked off.
  void StartBackgroundTask(int synchronous);
  void FlushBackgroundTasks(void);
  void RunTaskInBackground(const char *shell_command);

  const char *Session_Directory(void) const { return session_dir; }

  Schedule *SessionSchedule(void) const { return session_schedule; }

  // prints info into the session log about the CCD temperature.
  void SessionPrintStatus(void);

  // This helps with maintaining focus
  void check_focus_using_temp(void);

  // Query to find out original options
  const SessionOptions *GetOptions(void) const { return &UserOptions; }

  std::list<GroupInfo> &GetGroups(void) { return groups; }

  void SubmitWorkTask(std::string command) { work_queue.AddToQueue("TASK" + command); }

  // used for putting date into the session's ASTRO_DB database:
  AstroDB *astro_db;

private:
  SessionOptions UserOptions;
  struct dark_data;
  dark_data *first_dark;

  std::list<GroupInfo> groups;

  WorkQueue work_queue;

  Image *flatfile;
  char *flatfilename;

  char *focuslogfilename;

  static const int NUM_SESSION_FILTERS = 4;
  Image *flatfile_filter[NUM_SESSION_FILTERS];
  char *flatfilename_filter[NUM_SESSION_FILTERS];
  Filter filter_id[NUM_SESSION_FILTERS];

  Schedule *session_schedule;
  
  FILE *logfile;

  double focus_check_periodicity_minutes;

  char *session_dir;		// "/usr/tmp/IMAGES/8-14-2002/"

  char *obs_spreadsheet;	// comma-separated variables for reporting

  char *shutdown_task;		// command to issue when done

  JULIAN termination_time;
  JULIAN session_start_time;
  struct tm evening_time_info;

  // Whenever we see a strategy that needs a dark, we immediately
  // create a dark_data that is_composite, with a count of 0. This
  // serves as a placeholder for putting a "real" dark in.
  struct dark_data {
    dark_data *next;
    unsigned long dark_time_in_msec;
    char *dark_filename;
    int is_composite;
    int composite_count;
  };

  // This helps with the various constructors
  void SessionDefaultSetup(const SessionOptions &options,
			   JULIAN start_time);
};

#endif
