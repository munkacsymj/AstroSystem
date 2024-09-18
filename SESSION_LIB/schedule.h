// This may look like C code, but it is really -*- C++ -*-
/*  schedule.h -- manages the scheduling of observations during a session
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
#ifndef _SCHEDULE_H
#define _SCHEDULE_H
#include <julian.h>
#include <vector>
#include <list>
#include <string>

enum ItemStatus {
  COMPLETED,			// believed successful
  RECOVERABLE_SKIP,		// can't do now, try again later
  IMPOSSIBLE,			// can't do now, don't try again tonight
  FAILED,			// something went wrong
};

class Session;
class Strategy;
class ObservingAction;

// There is only one Schedule that is ever created.
class Schedule {
public:
  Schedule(Session *session);
  ~Schedule(void);

  // Add_Strategy() was the old way of building the schedule. The new
  // way uses Add_OA()
  void Add_Strategy(Strategy *strategy);
  void Add_OA(std::string &one_line);

  // create_schedule does the hard thinking to take those strategies
  // that were put into this Schedule (but haven't been executed
  // successfully) and finds the best order to execute these
  // strategies. 
  double create_schedule(void);
  void initialize_schedule(void);
  void include_in_schedule(ObservingAction *oa, double session_priority);

  void set_start_time(JULIAN start_time) {
    planned_exec_start_time = start_time;
  }
  
  void set_finish_time(JULIAN end_time) {
    planned_exec_end_time = end_time;
  }

  // Print the current schedule onto the session log.
  void log(void);

  double score(void) { return -1.0; }

  // return values for Execute_Schedule()
#define SCHED_NORMAL 0
#define SCHED_ABORT 1
  
  int Execute_Schedule(void);

  struct strategy_time_pair {
    Strategy           *strategy;
    ObservingAction    *oa;
    JULIAN              when;	// actually occured
    JULIAN              scheduled_time;	// scheduled
    JULIAN              scheduled_end_time;
    JULIAN              prior_observation;
    ItemStatus          status_code;
    int                 failures_so_far;
    // "needs_execution" is set to 1 for those strategies that have
    // been assigned scheduling slots, but that have not been
    // successfully executed. It is cleared if the strategy was
    // executed with success or was executed and failed in a way that
    // re-trying would be worthless.
    int                 needs_execution;
    // "scheduled" is set to 0 for strategies that have been inserted
    // into the schedule but that have not gone through the scheduling
    // process yet.
    int                 scheduled;
    int                 result;
    double              score;
  };

  // all_strategies contains an entry for every ObservingAction that
  // belongs to one of the groups that has been listed by the session
  // for inclusion in the schedule. The set of strategy_time_pairs in
  // all_strategies never changes during the session, no matter how
  // many times rescheduling is performed.
  std::list<strategy_time_pair *> all_strategies;
  
  std::vector<strategy_time_pair *> current_schedule;
  int currently_executing_action; // index into current_schedule[]

  Session *executing_session(void) const { return Executing_Session; }

private:
  Session *Executing_Session;

  JULIAN planned_exec_start_time;
  JULIAN planned_exec_end_time;

  void Mark_Current_Item(ItemStatus CurrentItemStatus);

  // This is passed a pointer to the current strategy_time_pair that
  // was just executed (nil if this is the first).  It will find a
  // strategy that can be executed, and return a pointer to it.  If
  // nothing is immediately ready for execution, this will block and
  // do nothing (or maybe do something with some incremental value)
  // until it is time to execute a strategy.
  strategy_time_pair *SelectNextStrategyAndWait(int *need_reschedule);
  strategy_time_pair *Designate_next_strategy(strategy_time_pair *s);

  void IncludeStrategy(Schedule::strategy_time_pair *item,
		       const char *orig_buffer,
		       char **words,
		       int word_num);
};

#endif
