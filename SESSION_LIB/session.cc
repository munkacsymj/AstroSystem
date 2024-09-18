/*  session.cc -- manages overall behavior of a night-long observing session
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
#include <sys/types.h>		// fork()
#include <sys/wait.h>		// waitpid()
#include <errno.h>
#include <signal.h>		// SIGCHLD
#include <unistd.h>		// fork()
#include <stdarg.h>		// varargs
#include <stdlib.h>		// system()
#include <string.h>		// strdup()
#include <ctype.h>
#include "session.h"
#include "strategy.h"
#include "work_queue.h"
#include "plan_exposure.h"
#include "obs_spreadsheet.h"
#include <camera_api.h>
#include <scope_api.h>
#include <StatusMessage.h>	// cooler mode flags
#include <gendefs.h>
#include <astro_db.h>

void SetDefaultOptions(SessionOptions &s) {
  s.do_focus = 0;
  s.leave_cooler_off = 0;
  s.keep_cooler_running = 1;
  s.default_dark_count = 1;
  s.trust_focus_star_position = 1; // usually trustworthy
  s.update_mount_model = 0;
  s.no_session_file = 0;
  s.park_at_end = 0;
  s.use_pec = 0;
  s.use_work_queue = 0;
}

Session::~Session(void) {
  delete session_schedule;
  fclose(logfile);
  delete flatfile;
  if(flatfilename)
    free(flatfilename);
}
		 
void Session::SessionDefaultSetup(const SessionOptions &options,
				  JULIAN start_time) {
  UserOptions = options;
  session_start_time = start_time;

  // 6pm EST is about JD xxx.4
  // 8am EST is about JD xxx.999
  // Hence, whatever "day" we extract from the JD is the "morning"
  // day. Subtract one to get the prior "evening" day.

  JULIAN evening_date = JULIAN((double) (long) start_time.day());
  time_t evening_time = evening_date.to_unix();
  localtime_r(&evening_time, &evening_time_info);
    
  mount_error_file = 0;
  flatfile = 0;
  flatfilename = 0;
  session_schedule = 0;
  logfile = 0;
  shutdown_task = nullptr;
  termination_time = JULIAN();
  focus_check_periodicity_minutes = 0;

  static char session_dir_name[132];
  sprintf(session_dir_name, "%s/%d-%d-%d/",
	  IMAGE_DIR,
	  evening_time_info.tm_mon+1,
	  evening_time_info.tm_mday,
	  evening_time_info.tm_year + 1900);
  session_dir = session_dir_name;
  obs_spreadsheet = 0;

  focuslogfilename = (char *) malloc(strlen(session_dir_name) + 32);
  sprintf(focuslogfilename, "%sfocus.log", session_dir_name);

  filter_id[0] = Filter("Vc");
  filter_id[1] = Filter("Rc");
  filter_id[2] = Filter("Bc");
  filter_id[3] = Filter("Ic");
  for (int i=0; i<NUM_SESSION_FILTERS; i++) {
    flatfile_filter[i] = 0;
    flatfilename_filter[i] = 0;
  }
}

  // "simple" session constructor that needs no session file
Session::Session(JULIAN start_time,
		 JULIAN end_time,
		 const char *logfile_name,
		 const SessionOptions &options) : configuration() {
  SessionDefaultSetup(options, start_time);
  session_schedule = new Schedule(this);
  termination_time = end_time;
  logfile = fopen(logfile_name, "w");
  setlinebuf(logfile);
  PrintSessionTimes();
}  

Session::Session(JULIAN start_time,
		 const char *session_file,
		 const SessionOptions &options) : configuration() {
  SessionDefaultSetup(options, start_time);
  int session_error = 0;	// set to 1 if unrecoverable user error

  FILE *fp = fopen(session_file, "r");

  if(!fp) {
    char err_msg[80];

    sprintf(err_msg, "Session: Cannot open session file %s\n",
	    session_file);
    perror(err_msg);
    session_error = 1;
  } else {
    char orig_line[132];
    char buffer[132];
    char value_with_spaces[132];

    while(fgets(orig_line, sizeof(orig_line), fp)) {
      strcpy(buffer, orig_line);

      // delete comments
      char *s;

      for(s=buffer; *s; s++)
	if(*s == '#') {
	  *s = 0;
	  break;
	}

      // save a copy of the value with spaces preserved
      {
	char *p = value_with_spaces;
	*p = 0;
	
	for (s=buffer; *s && *s != '='; s++)
	  ;
	if (*s == '=') {
	  s++;
	  while(*s and isspace(*s)) s++;
	  
	  while(*s and *s != '\n')
	    *p++ = *s++;
	  *p = 0;
	}
      }

      // now delete spaces
      {
	char *p = buffer;

	for(s=buffer; *s; s++) {
	  if(!isspace(*s))
	    *p++ = *s;
	}
	*p = 0;
      }

      if(buffer[0] == 0) continue; // ignore blank lines

      // break line into two pieces. First piece is keyword. Second
      // piece follows an '=' and is the keyword's value.
      char *keyword, *value = 0;

      keyword = buffer;

      for(s=buffer; *s; s++) {
	if(*s == '=') {
	  *s = 0;
	  value = s+1;
	  break;
	}
      }

      // convert keyword to uppercase
      for(s=keyword; *s; s++) {
	*s = toupper(*s);
      }

      // what's the keyword?

      // FLAT
      if(strcmp(keyword, "FLAT") == 0) {
	if(!value) {
	  fprintf(stderr, "session file: no flatfile name specified.\n");
	  session_error = 1;
	} else {
	  fprintf(stderr, "Using flat image %s\n", value);
	  flatfilename = strdup(value);
	  flatfile = new Image(value);
	}
      } else if(keyword[0] == 'F' &&
		keyword[1] == 'L' &&
		keyword[2] == 'A' &&
		keyword[3] == 'T' &&
		keyword[4] == '_') {
	Filter selected_filter(keyword+5);
	if (!value) {
	  fprintf(stderr, "session file: no flatfile name specified for %s\n",
		  keyword);
	  session_error = 1;
	} else {
	  int filter_index = -1;
	  for (int k = 0; k < NUM_SESSION_FILTERS; k++) {
	    if (filter_id[k] == selected_filter) {
	      filter_index = k;
	      break;
	    }
	  }
	  if (filter_index < 0) {
	    fprintf(stderr, "Filter name '%s' not recognized\n",
		    keyword+5);
	    session_error = 1;
	  } else {
	    fprintf(stderr, "Using flat image %s for filter %s\n",
		    value, selected_filter.NameOf());
	    flatfilename_filter[filter_index] = strdup(value);
	    flatfile_filter[filter_index] = new Image(value);
	  }
	}
	// LOGFILE
      } else if(strcmp(keyword, "LOGFILE") == 0) {
	logfile = fopen(value, "w");
	setlinebuf(logfile);
	if(!logfile) {
	  perror("session: cannot create logfile");
	  session_error = 1;
	} else {
	  fprintf(stderr, "Putting session log into %s\n", value);
	}

	// FOCUS (time in minutes)
      } else if(strcmp(keyword, "FOCUS") == 0) {
	long focus_value;
	int num_conv = sscanf(value, "%ld", &focus_value);
	if (num_conv != 1 || focus_value < -1 || focus_value > 500) {
	  fprintf(stderr, "Invalid FOCUS value: %s (time in mins)\n",
		  value);
	} else {
	  focus_check_periodicity_minutes = (double) focus_value;
	}

	// SHUTDOWN (time)
      } else if(strcmp(keyword, "SHUTDOWN") == 0) {
	// need date to convert this to a JULIAN.
	char date_buffer[80];

	// we don't know if the hh:mm stored in "value" is tomorrow
	// morning or is tonight.  Try using "tonight" and if it
	// doesn't work, shift to tomorrow.
	sprintf(date_buffer, "%s %d/%d/%d",
		value,
		evening_time_info.tm_mon+1,
		evening_time_info.tm_mday,
		evening_time_info.tm_year + 1900);

	termination_time = JULIAN(date_buffer);
	if(termination_time < session_start_time) {
	  // time is sometime tomorrow morning
	  sprintf(date_buffer, "%s %d/%d/%d",
		  value,
		  evening_time_info.tm_mon+1,
		  evening_time_info.tm_mday+1,
		  evening_time_info.tm_year + 1900);

	  termination_time = JULIAN(date_buffer);
	}

	// SHUTDOWNTASK (command)
      } else if(strcmp(keyword, "SHUTDOWNTASK") == 0) {
	shutdown_task = strdup(value_with_spaces);
	fprintf(stderr, "SHUTDOWNTASK='%s'\n", shutdown_task);

	// TRUSTFOCUSSTARPOSITION
      } else if(strcmp(keyword, "TRUSTFOCUSSTARPOSITION") == 0) {
	if (strcmp(value, "TRUE") == 0) {
	  UserOptions.trust_focus_star_position = 1;
	} else if(strcmp(value, "FALSE") == 0) {
	  UserOptions.trust_focus_star_position = 0;
	} else {
	  fprintf(stderr, "Invalid value for TRUSTFOCUSSTARPOSITION: %s\n",
		  value);
	}
	
	// USE_WORKQUEUE = TRUE/FALSE
      } else if(strcmp(keyword, "USE_WORKQUEUE") == 0) {
	if (strcmp(value, "TRUE") == 0) {
	  UserOptions.use_work_queue = 1;
	} else if(strcmp(value, "FALSE") == 0) {
	  UserOptions.use_work_queue = 0;
	} else {
	  fprintf(stderr, "Invalid value for USE_WORKQUEUE: %s\n",
		  value);
	}

	// ANALY_PREREQ=filename
      } else if(strcmp(keyword, "ANALY_PREREQ") == 0) {
	work_queue.AddToQueue("PREQ" + string(value));

	// SPREADSHEET
      } else if(strcmp(keyword, "SPREADSHEET") == 0) {
	obs_spreadsheet = strdup(value);

	// PEC
      } else if(strcmp(keyword, "PEC") == 0) {
	UserOptions.use_pec = 1;

	// SCHED_INCLUDE
      } else if(strcmp(keyword, "SCHED_INCLUDE") == 0) {
	GroupInfo info;
	std::string vs(value);
	size_t pos_comma = vs.find(',');
	if (pos_comma == std::string::npos) {
	  // no comma, implied priority
	  info.groupname = vs;
	  info.priority = 1.0;
	} else {
	  info.groupname = vs.substr(0, pos_comma);
	  sscanf(value+pos_comma+1, "%lf", &info.priority);
	}
	groups.push_back(info);

	// PARK
      } else if(strcmp(keyword, "PARK") == 0) {
	UserOptions.park_at_end = 1;
	UserOptions.keep_cooler_running = 0;

      } else if(strcmp(keyword, "COOLERSHUTDOWN") == 0) {
	UserOptions.park_at_end = 0;
	UserOptions.keep_cooler_running = 0;

	// MOUNT_ERROR
      } else if(strcmp(keyword, "MOUNT_ERROR") == 0) {
	mount_error_file = strdup(value);

	// UPDATE_MOUNT_MODEL
      } else if(strcmp(keyword, "UPDATE_MOUNT_MODEL") == 0) {
	UserOptions.update_mount_model = 1;

	// INVALID KEYWORDS
      } else {
	fprintf(stderr, "Session: invalid keyword: %s\n", keyword);
	session_error = 1;
      }
    }

    session_schedule = new Schedule(this);

    if(!termination_time.is_valid()) {
      fprintf(stderr, "Session: no SHUTDOWN time specified.\n");
      session_error = 1;
    }

    if(!logfile) {
      fprintf(stderr, "Session: no valid logfile.\n");
      session_error = 1;
    }
  }    

  if(session_error) exit(-2);
  PrintSessionTimes();

  if (UserOptions.use_work_queue) {
    RunTaskInBackground(COMMAND_DIR "/worker");
  }

}

void
Session::verify_dark_available(double exposure_time_secs,
			       int num_exposures) {
  char dark_command[256];

  sprintf(dark_command, COMMAND_DIR "/dark_manager -n %d -t %lf -d %s > /tmp/darkfilename",
	  num_exposures, exposure_time_secs, session_dir);
  if (system(dark_command) == -1) {
    fprintf(stderr, "session: cannot invoke dark_manager\n");
  }
}

void
Session::execute(void) {
  log(LOG_INFO, "S E S S I O N : starting.");

  Strategy::FindAllStrategies(this);
  Strategy::BuildObservingActions(this);

  if(!UserOptions.leave_cooler_off) {
    double ambient_temp, ccd_temp, cooler_setpoint, humidity;
    int cooler_power, mode;
    
    if(!CCD_cooler_data(&ambient_temp,
			&ccd_temp,
			&cooler_setpoint,
			&cooler_power,
			&humidity,
			&mode)) {
      log(LOG_ERROR, "Unable to query camera cooler. Session giving up.");
      return;
    }
    if((mode & COOLER_REGULATING) == 0) {
      log(LOG_INFO, "session starting cooler");
      if(system(COMMAND_DIR "/cooler startup") == -1) {
	perror("Unable to execute cooler startup command");
      }
    } else {
      log(LOG_INFO, "session: cooler already running");
    }
  }

  if(obs_spreadsheet && *obs_spreadsheet)
    Initialize_Spreadsheet(obs_spreadsheet);

  log(LOG_INFO, "session setting up schedule.");
  session_schedule->set_start_time(session_start_time);
  session_schedule->set_finish_time(termination_time);
  session_schedule->initialize_schedule();
  session_schedule->create_schedule();
  InitializeExposurePlanner(session_dir);

  AstroDB astro_database(JSON_READWRITE);
  this->astro_db = &astro_database;

  log(LOG_INFO, "session passing control to schedule.");
  int sched_result = session_schedule->Execute_Schedule();

  if(sched_result == SCHED_ABORT || UserOptions.keep_cooler_running) {
    log(LOG_INFO, "session leaving cooler running.");
  } else {
    log(LOG_INFO, "session shutting down cooler.");
    if(system(COMMAND_DIR "/cooler shutdown") == -1) {
      perror("Unable to execute cooler shutdown command");
    }
  }
  if(sched_result == SCHED_NORMAL && UserOptions.park_at_end) {
    log(LOG_INFO, "session parking telescope.");
    if(system(COMMAND_DIR "/park") == -1) {
      perror("Unable to execute mount park command");
    }
  }

  if (UserOptions.use_work_queue) {
    work_queue.AddToQueue("FINI");
  }
  log(LOG_INFO, "session: done.");
  if (shutdown_task && shutdown_task[0]) {
    log(LOG_INFO, "Starting shutdown_task.");
    int ret = system(shutdown_task);
    if (ret) {
      log(LOG_INFO, "Shutdown_task completed with errors");
    } else {
      log(LOG_INFO, "Shutdown_task completed okay.");
    }
  }
}

const char *
Session::dark_name(double exposure_time_secs, int num_exposures, bool defer_exposures) {
  char dark_command[256];

  if (defer_exposures) {
    std::string d = std::string(session_dir) + "dark" +
      std::to_string(int(exposure_time_secs+0.5)) + ".fits";
    return strdup(d.c_str());
  }

  sprintf(dark_command, COMMAND_DIR "/dark_manager -n %d -t %lf -d %s > /tmp/darkfilename",
	  num_exposures, exposure_time_secs, session_dir);
  if (system(dark_command) == -1) {
    fprintf(stderr, "session: cannot invoke dark_manager\n");
  }

  FILE *fp = fopen("/tmp/darkfilename", "r");
  if (!fp) {
    perror("session: unable to read /tmp/darkfilename:");
    return "";
  }
  char dark_filename[256];
  if (fgets(dark_filename, sizeof(dark_filename), fp)) {
    return strdup(dark_filename);
  } else {
    fprintf(stderr, "session: unable to get filename from /tmp/darkfilename\n");
    return "";
  }
}

Image *
Session::dark(double exposure_time_secs, int num_exposures) {
  const char *dark_filename = dark_name(exposure_time_secs, num_exposures, false);
  Image *return_value = 0;
  
  if(dark_filename) {
    return_value = new Image(dark_filename);
    free ((char *) dark_filename);
  }

  return return_value;
}

const Image *
Session::flat(void) {
  return flatfile;
}

const Image *
Session::flat(Filter *filter) {
  int filter_index = -1;
  for (int k = 0; k < NUM_SESSION_FILTERS; k++) {
    if (filter_id[k] == *filter) {
      filter_index = k;
      break;
    }
  }
  if (filter_index < 0) {
    fprintf(stderr, "Session::flat(): flat not defined for filter %s\n",
	    filter->NameOf());
  } else {
    return flatfile_filter[filter_index];
  }
  return 0;
}

const char *
Session::flat_filename(Filter *filter) {
  int filter_index = -1;
  for (int k = 0; k < NUM_SESSION_FILTERS; k++) {
    if (filter_id[k] == *filter) {
      filter_index = k;
      break;
    }
  }
  if (filter_index < 0) {
    fprintf(stderr,
	    "Session::flat_filename(): flat not defined for filter %s\n",
	    filter->NameOf());
  } else {
    return flatfilename_filter[filter_index];
  }
  return 0;
}

void
Session::log(int level, const char *format, ...) {
  va_list ap;
  va_start(ap, format);

  tm     *time_data;
  time_t clock_data;
  time(&clock_data);
  time_data = localtime(&clock_data);
  
  // test for level?

  fprintf(logfile, "%d/%d/%d %02d:%02d:%02d ",
	  time_data->tm_mon + 1,
	  time_data->tm_mday,
	  time_data->tm_year + 1900,
	  time_data->tm_hour,
	  time_data->tm_min,
	  time_data->tm_sec);
  vfprintf(logfile, format, ap);
  int n_print = fprintf(logfile, "\n");
  if(n_print != 1) {
    fprintf(stderr, "Session: fprintf(log) returned %d, err=%d\n",
	    n_print, errno);
  }
}

void
Session::PutFileIntoLog(int level, const char *filename) {
  tm     *time_data;
  time_t clock_data;
  time(&clock_data);
  time_data = localtime(&clock_data);

  FILE *fp_in = fopen(filename, "r");
  if (!fp_in) {
    log(LOG_ERROR, "Unable to insert file %s into log.\n", filename);
  } else {
    char buffer[256];

    while(fgets(buffer, sizeof(buffer), fp_in)) {
      fprintf(logfile, "%d/%d/%d %d:%02d:%02d %s",
	      time_data->tm_mon + 1,
	      time_data->tm_mday,
	      time_data->tm_year + 1900,
	      time_data->tm_hour,
	      time_data->tm_min,
	      time_data->tm_sec,
	      buffer);
    }
    fclose(fp_in);
  }
}

static void split_time(JULIAN jdate, int &day, int &month, int &year) {
  time_t evening_time = jdate.to_unix();
  struct tm evening_time_info;
  localtime_r(&evening_time, &evening_time_info);

  day = evening_time_info.tm_mday;
  month = 1 + evening_time_info.tm_mon;
  year = 1900 + evening_time_info.tm_year;
}

void
Session::EveningDate(int &day, int &month, int &year) {
  JULIAN evening_date = JULIAN((double) (long) session_start_time.day());
  split_time(evening_date, day, month, year);
}

void
Session::MorningDate(int &day, int &month, int &year) {
  JULIAN morning_date = JULIAN((double) 1+ (long) session_start_time.day());
  split_time(morning_date, day, month, year);
}

struct pending_tasks {
  char *command_line;
  Session *owner;
  pending_tasks *next;
} *head_task = 0;

// "child_pid" is set to 0 whenever there is no background process
// running.
static int child_pid = 0;

// "running_task" holds the command line of the background process
struct pending_tasks *running_task = 0;

#define SYNCHRONOUS 1

void SigChildHandler(int x) {
  // wait for the child
  int status;
  if(waitpid(child_pid, &status, WNOHANG) >= 0) {
    child_pid = 0;		// nothing running anymore
    if (running_task) {
      free (running_task->command_line);
      delete running_task;
      running_task = 0;
    }
    if(head_task) head_task->owner->StartBackgroundTask(SYNCHRONOUS);
  }
}

void
Session::FlushBackgroundTasks(void) {
  while(head_task) {
    StartBackgroundTask(!SYNCHRONOUS);
  }
}

void
Session::StartBackgroundTask(int synchronous) {
  while(head_task) {
    fprintf(stderr, "...forking...\n");
    fflush(0);

    pending_tasks *just_started = head_task;
    head_task = head_task->next;
    running_task = just_started;

    child_pid = fork();
    if(child_pid == 0) {
      // this is the child
      struct sigaction act;
      act.sa_handler = SIG_IGN;
      sigemptyset(&act.sa_mask);	// nothing blocked
      act.sa_flags = SA_NOCLDSTOP;

      sigaction(SIGCHLD, &act, 0);
      
      // sleep(1);			// give parent time to save child_pid
      exit(system(just_started->command_line));
    } else {
      // this is the parent
      ;
    }

    // if asynchronous, keep kicking things off
    if(synchronous == SYNCHRONOUS) break;
  }
  fprintf(stderr, "child_pid = %d\n", child_pid);
}
      

void
Session::RunTaskInBackground(const char *shell_command) {
  // Setup a signal handler on first entrance. The signal handler will
  // call SigChildHandler() whenever a child process exits.
  static int sig_handler_setup = 0;
  if(!sig_handler_setup) {
    struct sigaction act;
    act.sa_handler = SigChildHandler;
    sigemptyset(&act.sa_mask);	// nothing blocked
    act.sa_flags = SA_NOCLDSTOP;

    sigaction(SIGCHLD, &act, 0);
    sig_handler_setup = 1;
  }

  pending_tasks *new_task = new pending_tasks;

  new_task->command_line = strdup(shell_command);
  new_task->owner = this;
  new_task->next = head_task;
  head_task = new_task;

  if(child_pid == 0) StartBackgroundTask(SYNCHRONOUS);
}

// Prints stuff into the session log file
void
Session::PrintSessionTimes(void) {
  log(LOG_INFO, "Session start = %s (%lf)",
      session_start_time.to_string(),
      session_start_time.day());
  log(LOG_INFO, "Session quit  = %s (%lf)",
      termination_time.to_string(),
      termination_time.day());
}
  
void
Session::SessionPrintStatus(void) {
  double ambient_temp, ccd_temp, cooler_setpoint, humidity;
  int cooler_power, mode;

  CCD_cooler_data(&ambient_temp,
		  &ccd_temp,
		  &cooler_setpoint,
		  &cooler_power,
		  &humidity,
		  &mode);

  log(LOG_INFO, "Ambient temperature = %.1f, cooler temp = %.1f",
      ambient_temp, ccd_temp);
}

void
Session::check_focus_using_temp(void) {
  static int first_call = 1;
  static int focus_steps_so_far = 0;
  static double reference_ambient;

  double ambient_temp, ccd_temp, cooler_setpoint, humidity;
  int cooler_power, mode;

  CCD_cooler_data(&ambient_temp,
		  &ccd_temp,
		  &cooler_setpoint,
		  &cooler_power,
		  &humidity,
		  &mode);

  if(first_call) {
    first_call = 0;
    reference_ambient = ambient_temp;
  } else {
    // changed from one step every 0.5-degree to one step every 2 deg
    int delta_steps = (int) (0.5 + (reference_ambient - ambient_temp)/2.0);
    while(delta_steps > focus_steps_so_far) {
      log(LOG_INFO, "Performing 100msec focus tweak due to temp drop.");
      scope_focus(100);		// 100 msec per 0.5 deg C drop in temp
      sleep(2);			// let focus motor grind to halt
      focus_steps_so_far++;
    }
  }
}
      
Session::SessionInfo
Session::StatusCheck(Session::TaskInfo t, int sleep_time_in_seconds) {
  if(termination_time < JULIAN(time(0))) {
    // we are past scheduled end time
    return QUIT_TASK;
  }
  
  // check_focus_using_temp();

  return SESSION_OKAY;
}
