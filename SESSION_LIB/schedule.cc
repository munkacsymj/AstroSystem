/*  schedule.cc -- manages the scheduling of observations during a session
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
#include <unistd.h>		// sleep()
#include <string.h>
#include <stdlib.h>		// mkstemp()
#include <ctype.h>
#include "strategy.h"
#include "session.h"
#include "schedule.h"
#include "observing_action.h"
#include <scope_api.h>		// ControlTrackingMotor()
#include <gendefs.h>

const static int MAX_FAILURES_TO_FLUSH = 2;

static int times_are_close(JULIAN t1, JULIAN t2) {
  double delta = t1 - t2;
  if(delta < 0.0) delta = -delta;

  return (delta < 15.0 / (24.0 * 60.0));
}

Schedule::Schedule(Session *session) {
  fprintf(stderr, "Schedule::Schedule() -- new schedule created.\n");
  
  if(!session) {
    fprintf(stderr, "Schedule: invalid session pointer\n");
    exit(-1);
  }
  Executing_Session       = session;
  planned_exec_start_time = JULIAN();
  planned_exec_end_time   = JULIAN();
}

Schedule::~Schedule(void) {
  for (auto s : all_strategies) {
    delete s;
  }
}

double
Schedule::create_schedule(void) {
  Executing_Session->log(LOG_INFO, "starting create_schedule");

  if(Executing_Session->StatusCheck(Session::TASK_RESCHEDULING)
     == Session::QUIT_TASK) {
    // session wants us to stop
    return 0.0;
  }

  char temp_name_in[80];
  char temp_name_out[80];
  char temp_logfile[80];
  char temp_dir[24];
  double score;

  strcpy(temp_dir, "/tmp/schedule.XXXXXX");
  if (!mkdtemp(temp_dir)) {
    perror("Error creating schedule /tmp directory: ");
    return 0.0;
  }
  sprintf(temp_name_in, "%s/schedule.in", temp_dir);
  sprintf(temp_name_out, "%s/schedule.out", temp_dir);
  sprintf(temp_logfile, "%s/schedule.log", temp_dir);

  fprintf(stderr, "schedule: in file = %s, out file = %s\n",
	  temp_name_in, temp_name_out);

  // now we put a list of strategy names into temp_name_in
  FILE *fp_in = fopen(temp_name_in, "w");
  if(!fp_in) {
    perror("schedule: could not create temp_in for scheduler");
  } else {
    JULIAN now(time(0));
    // add 5 minutes to allow for time to run the scheduler
    now = now.add_days(5.0/(24.0*60.0));
    if(now < planned_exec_start_time) now = planned_exec_start_time;

    fprintf(fp_in, "%f %f %s\n",
	    now.day(),
	    planned_exec_end_time.day(),
	    temp_logfile);

    for (auto item : all_strategies) {
      if (item->needs_execution) {
	fprintf(fp_in, "%s\n", item->oa->ToScheduleString().c_str());
	item->scheduled = 0;
      }
    }
    
    fclose(fp_in);
    char sys_command[256];
    sprintf(sys_command, COMMAND_DIR "/scheduler %s %s",
	    temp_name_in, temp_name_out);
    Executing_Session->log(LOG_INFO, "Executing command %s", sys_command);
    // fflush(0);
    if(system(sys_command) == -1) {
      Executing_Session->log(LOG_INFO, "Command execution failed.");
    } else {
      Executing_Session->log(LOG_INFO, "Command finished.");
    }
    // fflush(0);

    // read temp_name_out
    FILE *fp_out = fopen(temp_name_out, "r");
    if(!fp_out) {
      fprintf(stderr, "couldn't open output filename from scheduler\n");
    } else {
      char buffer[132];

      if (fscanf(fp_out, "%lf", &score) != 1) {
	fprintf(stderr, "schedule.cc: error reading score from scheduler output.\n");
	score = 0.0;
      }

      // Clear out anything in existing schedule
      for (auto t : current_schedule) {
	delete t;
      }
      current_schedule.clear();
      
      while(fgets(buffer, sizeof(buffer), fp_out)) {
	strategy_time_pair *item = ObservingAction::CreateExecutableSTP(buffer);
	if (item == nullptr) {
	  fprintf(stderr, "Error matching line in scheduler output: %s\n",
		  buffer);
	} else {
	  item->scheduled      = 1;
	  current_schedule.push_back(item);
	}
      }
      fclose(fp_out);
      currently_executing_action = -1;
    }

    // unlink(temp_name_in);
    // unlink(temp_name_out);
    // rmdir();
  }
  log();
  return score;
}

void
Schedule::initialize_schedule(void) {
  // Look to the session's observing "groups" to determine what to schedule
  for (auto g : Executing_Session->GetGroups()) {
    for (ObservingAction *item : ObservingAction::GroupList(g.groupname)) {
      include_in_schedule(item, g.priority);
    }
  }
  
  string message("schedule: completing schedule initialization with " +
		 to_string(all_strategies.size()) + " ObservingActions");
  Executing_Session->log(LOG_INFO, message.c_str());
}

void
Schedule::include_in_schedule(ObservingAction *item, double session_priority) {
  strategy_time_pair *stp = new strategy_time_pair;
  item->SetSTP(stp);
  stp->strategy = item->strategy();
  stp->oa = item;
  item->SetSessionPriority(session_priority);
  stp->needs_execution = 1;
  stp->failures_so_far = 0;
  stp->prior_observation = JULIAN(0.0);
  if (item->TypeOf() == AT_Time_Seq) {
    const ObsInterval oi = item->GetInterval();
    stp->scheduled_time = JULIAN(oi.start);
    stp->scheduled_end_time = JULIAN(oi.end);
  }
  all_strategies.push_back(stp);
}


int
Schedule::Execute_Schedule(void) {
  // after a few in a row with no stars visible, we conclude that the
  // weather has gotten bad and we should quit.
  int no_stars_count = 0;
  strategy_time_pair *strategy = 0;
  int need_reschedule = 0;	// 1=> need to reschedule everything

  while((strategy = SelectNextStrategyAndWait(&need_reschedule))) {
    bool force_shutdown = false;
    
    Executing_Session->log(LOG_INFO,
			   "Starting strategy for %s",
			   strategy->oa->GetObjectName());

    Execution_Result result = strategy->oa->execute(Executing_Session);
    if(result == NO_STARS) {
      strategy->failures_so_far++;
      no_stars_count++;
      if(no_stars_count >= 3) {
	Executing_Session->log(LOG_ERROR,
			       "Consistently find no stars. Quitting.");
	return SCHED_ABORT;
      }
    } else {
      no_stars_count = 0;	// reset the count
    }

    switch(result) {
    case OKAY:
      Executing_Session->log(LOG_INFO,
			     "Strategy for %s completed okay.",
			     strategy->oa->GetObjectName());
      strategy->needs_execution = 0;
      strategy->status_code = COMPLETED;
      break;

    case PERFORM_SESSION_SHUTDOWN:
      Executing_Session->log(LOG_INFO,
			     "Commencing shutdown per strategy's return value.");
      strategy->status_code = FAILED;
      force_shutdown = true;
      break;

    case NOT_VISIBLE:
      Executing_Session->log(LOG_ERROR,
			     "%s not visible, will retry strategy later.",
			     strategy->oa->GetObjectName());
      strategy->failures_so_far++;
      strategy->needs_execution = 1;
      strategy->status_code = RECOVERABLE_SKIP;
      need_reschedule++;
      break;

    case LOST_IN_SPACE:
      Executing_Session->log(LOG_ERROR,
			     "Can't identify field. Will retry %s later.",
			     strategy->oa->GetObjectName());
      strategy->failures_so_far++;
      strategy->needs_execution = 1;
      strategy->status_code = RECOVERABLE_SKIP;
      need_reschedule++;
      break;

    case NO_STARS:
      Executing_Session->log(LOG_ERROR,
			     "No stars seen in images for %s. Will retry later.",
			     strategy->oa->GetObjectName());
      strategy->failures_so_far++;
      strategy->needs_execution = 1;
      strategy->status_code = RECOVERABLE_SKIP;
      need_reschedule++;
      break;

    case POOR_IMAGE:
      Executing_Session->log(LOG_ERROR,
			     "Image quality too poor for %s.",
			     strategy->oa->GetObjectName());
      strategy->failures_so_far++;
      strategy->needs_execution = 1;
      strategy->status_code = RECOVERABLE_SKIP;
      need_reschedule++;
      break;

    }
    if (force_shutdown) {
      return SCHED_ABORT;
    }
  } // nothing else to be worked
  return SCHED_NORMAL;
}

Schedule::strategy_time_pair *
Schedule::SelectNextStrategyAndWait(int *need_reschedule) {

  if(Executing_Session->StatusCheck(Session::TASK_OVER)
     == Session::QUIT_TASK) {
    // session wants us to stop
    return 0;
  }

  currently_executing_action++;
  if(*need_reschedule >= 3 or currently_executing_action >= (int) current_schedule.size()) {
    Executing_Session->log(LOG_INFO,
			   "need_reschedule= %d. performing reschedule.",
			   *need_reschedule);

    create_schedule();		// reschedule
    *need_reschedule = 0;
    currently_executing_action = 0;
    if (current_schedule.size() == 0) return 0;
  }
  
  strategy_time_pair *candidate = current_schedule[currently_executing_action];

  // can we blindly execute the next item in the list?
  JULIAN now(time(0));

  if(candidate &&
     times_are_close(now, candidate->scheduled_time) &&
     candidate->needs_execution)
    return Designate_next_strategy(candidate);

  // okay, things will be a little more complex.

  // if schedule's even a little bit busted, reschedule
  if(*need_reschedule) {
    Executing_Session->log(LOG_INFO,
			   "Schedule seems busted. Rescheduling.");
    create_schedule();
    *need_reschedule = 0;
    currently_executing_action = 0;
    if (current_schedule.size() == 0) return 0;
    now = JULIAN(time(0));
    candidate = current_schedule[currently_executing_action];
  }

  // ready to execute or need to sleep for a while
  if(!candidate->needs_execution) {
    fprintf(stderr, "schedule: assertion: candidate needs execution FAILED.\n");
    (*need_reschedule)++;
    return SelectNextStrategyAndWait(need_reschedule);
  }

  if(times_are_close(now, candidate->scheduled_time))
    return Designate_next_strategy(candidate);

  // next item has time in the past?? Our last strategy took too long,
  // maybe?
  if(candidate->scheduled_time < now) {
    Executing_Session->log(LOG_INFO,
			   "Missed next strategy. Rescheduling.");
    create_schedule();
    *need_reschedule = 0;
    currently_executing_action = -1;
    if (current_schedule.size() == 0) return 0;
    return SelectNextStrategyAndWait(need_reschedule);
  }

  // we will sleep for a while; turn off tracking motor while asleep
  // to prevent the mount from running into the stops
  do {
    now = JULIAN(time(0));
    int delta_time_secs = (int) ((candidate->scheduled_time - now)*24*60*60);
    if(delta_time_secs <= 0 || delta_time_secs > (6*3600)) break;

    ControlTrackingMotor(1 /*TURN_OFF*/);
    Executing_Session->log(LOG_INFO,
			   "Sleeping for %d secs",
			   delta_time_secs);
    sleep(delta_time_secs);
    Executing_Session->log(LOG_INFO, "Woke Up.");
  } while(1);
  ControlTrackingMotor(0 /*TURN_ON*/);
  return Designate_next_strategy(candidate);
}

Schedule::strategy_time_pair *
Schedule::Designate_next_strategy(strategy_time_pair *s) {
  return s;
}

void
Schedule::log(void) {
  Executing_Session->log(LOG_INFO, "Current schedule:");
  for (strategy_time_pair *item : current_schedule) {
    Executing_Session->log(LOG_INFO,
			   "    %16s %s",
			   item->oa->GetObjectName(),
			   item->scheduled_time.to_string());
  }
}
