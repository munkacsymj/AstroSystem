// This may look like C code, but it is really -*- C++ -*-
/*  observing_action.h -- a schedulable event
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
#ifndef _OBSERVING_ACTION_H
#define _OBSERVING_ACTION_H

#include "strategy.h"
#include "schedule.h"
#include <string>
#include <list>
#include <ostream>
using namespace std;

class Session;			// forward declaration

enum ActionType {
		 AT_Invalid,
		 AT_Time_Seq,	// EB and exoplanets
		 AT_Quick,	// EB eclipse search
		 AT_Script,	// LPV
		 AT_Dark,	// Darks
		 AT_Flat };	// Flats

class ParamValue {
public:
  ParamValue(void) { pv_val = PARAM_INVALID; }
  ParamValue(int x) { pv_val = x; }
  ~ParamValue(void) {;}

  const static int PARAM_HOLES = -1;
  const static int PARAM_PRIMARY_ECLIPSE = -2;
  const static int PARAM_SECONDARY_ECLIPSE = -3;
  const static int PARAM_INVALID = -99;

  int PV2Int(void) { return pv_val; }
  
private:
  int pv_val;
};

class ParsedActionString;

class ObservingAction {
public:
  // normal constructor
  ObservingAction(Strategy *strategy, Session *session, ActionType type);
  // creating an ObservingAction from a text line in a schedule file;
  // this is the inverse of ToScheduleString().
  ObservingAction(string &one_line);
  ~ObservingAction(void) {;}

  ActionType TypeOf(void) { return oa_type; }
  string TypeString(void);
  double CadenceSeconds(void) { return cadence; }
  double CadenceDays(void) { return cadence/(24.0*3600.0); }
  void SetCadenceSeconds(double c) { cadence = c; }
  void SetPriority(double p) { priority = p; }
  void SetSessionPriority(double p) { session_priority = p; }
  double GetPriority(void) { return priority; }
  int GetUniqueID(void) { return unique_id; }
  void ResetUniqueID(int i) { unique_id = i; }
  void SetSTP(Schedule::strategy_time_pair *s) { stp = s; }
  Schedule::strategy_time_pair *GetSTP(void) { return stp; }
  const char *GetObjectName(void) { return object_name.c_str(); }
  
  static void Factory(std::list<string> &action_strings,
		      std::list<ObservingAction *> &action_list,
		      Strategy *strategy,
		      Session *session);
  static list<ObservingAction *> GroupList(const string &group_name);
  
  static Schedule::strategy_time_pair *
  CreateExecutableSTP(const char *one_line); // uses strings from scheduler output

  double score(JULIAN observation_time, JULIAN last_observation_time, Session *session) const ;

  double execution_time_prediction(void) const { return planning_duration; } // seconds
  void SetExecutionTime(double duration) { planning_duration = duration; } // seconds

  void SetGroups(list<string> g);

  // Execute an observing action.
  Execution_Result execute(Session *session);

  double IntervalObservable(const ObsInterval &interval, ObsInterval &result);
  double EphemerisObservable(JULIAN jd_ref,
			     double phase_start,
			     double phase_end,
			     double period,
			     long orbit_number,
			     ObsInterval &result);
  void SetInterval(const ObsInterval &oi) { start_time = JULIAN(oi.start);
    end_time = JULIAN(oi.end); }

  const ObsInterval GetInterval(void) { return ObsInterval{start_time.day(),
							     end_time.day(),
							     1.0};}

  friend std::ostream& operator<< (std::ostream &out, const ObservingAction &oa);

  Strategy *strategy(void) { return parent_strategy; }

  // Provide score to the scheduler. In some cases, just invokes Strategy::score()
  double score(JULIAN last_observation_time,
	       JULIAN oa_start_time,
	       JULIAN oa_end_time);

  string ToScheduleString(void);
private:
  std::string script;
  std::string object_name;
  int unique_id;
  double priority;
  double session_priority;
  JULIAN start_time;
  JULIAN end_time;
  double cadence;		// in seconds
  double planning_duration;	// in seconds
  ActionType oa_type;
  Strategy *parent_strategy;
  Session *parent_session;
  list<string> groups;
  Schedule::strategy_time_pair *stp;
  int next_set_number;		// used for Quicks

  static void TimeSeqFactory(ParamValue pv,
			     list<string> &group_list,
			     double priority,
			     std::list<ObservingAction *> &action_list,
			     Strategy *strategy,
			     Session *session);
  static void QuickFactory(ParamValue pv,
			   list<string> &group_list,
			   double priority,
			   std::list<ObservingAction *> &action_list,
			   Strategy *strategy,
			   Session *session);
  static void ScriptFactory(ParamValue pv,
			    list<string> &group_list,
			    double priority,
			    std::list<ObservingAction *> &action_list,
			    Strategy *strategy,
			    Session *session);
  static void HoleFactory(ParamValue pv,
			  list<string> &group_list,
			  double priority,
			  std::list<ObservingAction *> &action_list,
			  Strategy *strategy,
			  Session *session);

};

void PrintSummaryByGroups(void);

#endif
