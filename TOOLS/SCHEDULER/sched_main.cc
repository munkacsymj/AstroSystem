/*  sched_main.cc -- Program to generate a schedule for a session
 *
 *  Copyright (C) 2007 Mark J. Munkacsy
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
#include <schedule.h>
#include <session.h>
#include <strategy.h>
#include <obs_record.h>
#include <scheduler.h>
#include <observing_action.h>
#include <stdio.h>
#include <string>

// Invoked with two command-line arguments:
//    1. The name of the input file
//    2. The name of the output file

int main(int argc, char **argv) {
  double start_time, stop_time;
  char logfile_name[64];

  FILE *fp_in = fopen(argv[1], "r");
  if(!fp_in) {
    fprintf(stderr, "scheduler: cannot open input file %s\n", argv[1]);
    exit(-2);
  }

  if(fscanf(fp_in, "%lf %lf %s\n",
	    &start_time, &stop_time, logfile_name) != 3) {
    fprintf(stderr, "sched_main: error parsing input file.\n");
  }
  JULIAN StartJD(start_time);
  JULIAN StopJD(stop_time);

  SessionOptions opts;
  opts.no_session_file = 1;
  opts.keep_cooler_running = 1;	// no warmup time assumed
  Session *session = new Session(StartJD,
				 StopJD,
				 logfile_name,
				 opts);
  
  ObsRecord obs_record;
  char input_buffer[132];

  while(fgets(input_buffer, sizeof(input_buffer), fp_in)) {
    int uid;
    char oa_type[16];
    char strategy_name[64];
    double dstart_t, dend_t;
    double priority = 1.0;
    double cadence = 0.0;
    ActionType at = AT_Invalid;

    int fields = sscanf(input_buffer, "%d %s %lf %s %lf %lf",
			&uid, oa_type, &priority, strategy_name, &dstart_t, &dend_t);
    if (fields < 3) {
      fprintf(stderr, "sched_main: invalid input line: %s", input_buffer);
      continue;
    }

    string type_string(oa_type);
    if (type_string == "Script") {
      at = AT_Script;
      if (fields != 4) {
      fprintf(stderr, "sched_main: invalid field count (Script): %s", input_buffer);
      continue;
      }
    } else if (type_string == "Quick") {
      at = AT_Quick;
      if (fields != 5) {
	fprintf(stderr, "sched_main: invalid field count (Quick): %s", input_buffer);
	continue;
      }
      cadence = dstart_t;
    } else if (type_string == "Time_Seq") {
      at = AT_Time_Seq;
      if (fields != 6) {
	fprintf(stderr, "sched_main: invalid field count (Time_Seq): %s", input_buffer);
	continue;
      }
    } else if (type_string == "Dark") {
      at = AT_Dark;
      if (fields != 3) {
	fprintf(stderr, "sched_main: invalid field count (Dark): %s", input_buffer);
	continue;
      }
    } else if (type_string == "Flat") {
      at = AT_Flat;
      if (fields != 3) {
	fprintf(stderr, "sched_main: invalid field count (Flat): %s", input_buffer);
	continue;
      }
    } else {
      fprintf(stderr, "Sched_main: invalid type: %s", input_buffer);
      continue;
    }

    Strategy *strategy = nullptr;
    if (at != AT_Dark && at != AT_Flat) {
      strategy = new Strategy(strategy_name, session);
    }

    if (at == AT_Script || at == AT_Quick) {
      // Find the most recent observation for each star. Set the time
      // of the last observation (in the strategy) to that time, and
      // if the observation execution time (duration) is also
      // available from the ObsRecord file, pull that out and use it
      // for planning purposes.
      ObsRecord::Observation *obs = obs_record.LastObservation(strategy_name);
      if(obs && !obs->empty_record) {
	strategy->SetLastObservation(obs->when);
	if (isnormal(obs->execution_time)) {
	  strategy->SetLastExecutionDuration(obs->execution_time);
	}
      }
    }
    
    ObservingAction *oa = new ObservingAction(strategy, session, at);
    // Priority is a bit strange. The scheduler process doesn't have
    // access to the original session.txt file given to
    // simple_session. There's only a single priority field in the
    // input file, so the simple_session process multiplies the
    // session_priority times the ObservingAction priority. Here at
    // the receiving end, we can't undo that multiplication, so we
    // just set session_priority to 1.0 and put the composite priority
    // into the ObservingAction.
    oa->SetPriority(priority);
    oa->SetSessionPriority(1.0);
    oa->ResetUniqueID(uid);
    if (at == AT_Script) {
      oa->SetExecutionTime(strategy->execution_time_prediction());
    }
    if (at == AT_Time_Seq) {
      ObsInterval intval{dstart_t, dend_t, 1.0};
      oa->SetInterval(intval);
    }
    if (at == AT_Quick) {
      oa->SetCadenceSeconds(cadence);
      oa->SetExecutionTime(5.0*60.0); // 5 min
    }
    if (at == AT_Dark) {
      oa->SetExecutionTime(30.0*60.0); // 30 min
    }
    if (at == AT_Flat) {
      oa->SetExecutionTime(40.0*60.0); // 40 min
    }
    session->SessionSchedule()->include_in_schedule(oa, oa->GetPriority());
  }

  setup_stars(session->SessionSchedule());
  build_initial_population();
  fflush(0);
  main_loop(argv[2]);
}
  
				 
