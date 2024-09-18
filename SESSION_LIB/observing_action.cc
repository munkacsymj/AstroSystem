/*  observing_action.cc -- manages the execution of an observation for a
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

#include "observing_action.h"
#include "strategy.h"
#include "session.h"
#include "proc_messages.h"	// notify command
#include "focus_manager.h"
#include "obs_record.h"
#include "finder.h"
#include <camera_api.h>
#include <iostream>
#include <stdlib.h>
#include <unistd.h>		// sleep()
#include <list>
#include <string>
#include <cassert>
using namespace std;

static unsigned long next_unique_id = 0x1000;
static unordered_map<std::string, std::list<ObservingAction *>> group_xref;
static unordered_map<unsigned long, ObservingAction *> uid_xref;
static ObsRecord *obs_record = nullptr;
static bool initialization_complete = false;

static void ObsActionInitialize(void) {
  initialization_complete = true;
  obs_record = new ObsRecord;
}

class ParsedActionString {
public:
  ParsedActionString(std::string str);
  ~ParsedActionString(void) {;}
  list<string> &group_list(void) { return groups_list; }
  double priority(void) { return action_priority; }
  int number_of_actions(void) { return type_list.size(); }
  ActionType type_of_action(int index) { return type_list[index]; }
  ParamValue param_of_action(int index) { return param_list[index]; }

private:
  double action_priority;
  std::vector<ActionType> type_list;
  std::vector<ParamValue> param_list;
  std::list<string> groups_list;
};

ParsedActionString::ParsedActionString(std::string str) {
  vector<string> words;
  std::size_t current, previous = 0;
  string groupstring;
  string remainder;

  action_priority = 1.0; // default
  if (str[0] == '(') { // if group field is a list...
    current = str.find(')');
    if (current == std::string::npos) {
      cerr << "ParsedActionString: group list missing close paren: "
	   << str << std::endl;
      return;
    }
    groupstring = str.substr(1, current);
    remainder = str.substr(current+1);
  } else {
    // single-word group
    current = str.find(',');
    if (current == std::string::npos) {
      cerr << "ParsedActionString: syntax err: no comma after group name: "
	   << str << std::endl;
      return;
    }
    groupstring = str.substr(0, current);
    remainder = str.substr(current+1);
  }
    
  // Parse the group list
  current = groupstring.find(',');
  while(current != std::string::npos) {
    groups_list.push_back(groupstring.substr(previous, current-previous));
    previous = current+1;
    current = groupstring.find(',', previous);
  }
  groups_list.push_back(groupstring.substr(previous, current-previous));
			  
  // now parse the remainder
  current = remainder.find(',');
  while (current != std::string::npos) {
    words.push_back(remainder.substr(previous, current - previous));
    previous = current+1;
    current = remainder.find(',', previous);
  }
  words.push_back(remainder.substr(previous, current - previous));

  // check for priority
  string &pri_word = words.back(); // last word, if present
  char *end = nullptr;
  double explicit_priority = strtod(pri_word.c_str(), &end);
  if (end == pri_word.c_str() ||
      *end != '\0' ||
      action_priority == HUGE_VAL) {
    ; // do absolutely nothing if the final word isn't a priority
  } else {
    action_priority = explicit_priority;
    words.pop_back(); // remove the priority
  }

  for (unsigned int i=0; i<words.size(); i++) {
    // type(param) (parenthesis are required)
    std::size_t p_start, p_end;
    p_start = words[i].find('(');
    p_end = words[i].find(')');
    if (p_start == std::string::npos ||
	p_end == std::string::npos ||
	p_start > p_end) {
      cerr << "ParsedActionString: invalid observing action (b): "
	   << words[i] << std::endl;
    } else {
      string action_word(words[i].substr(0, p_start));
      string param_word(words[i].substr(p_start+1, p_end-p_start-1));
      cerr << "ParsedActionString: "
	   << action_word << '(' << param_word << ')' << std::endl;
      ActionType at = AT_Invalid;
      ParamValue pv(ParamValue::PARAM_INVALID);
	
      if (action_word == "TimeSeq") at = AT_Time_Seq;
      else if (action_word == "Script") at = AT_Script;
      else if (action_word == "Quick_observe" ||
	       action_word == "Quick") at = AT_Quick;
      else if (action_word == "Dark") at = AT_Dark;
      else if (action_word == "Flat") at = AT_Flat;
      else {
	cerr << "ParsedActionString: action unrecognized: "
	     << action_word << std::endl;
      }

      if (param_word == "Pri") pv = ParamValue(ParamValue::PARAM_PRIMARY_ECLIPSE);
      else if (param_word == "Sec") pv = ParamValue(ParamValue::PARAM_SECONDARY_ECLIPSE);
      else if (param_word == "Hole" ||
	       param_word == "Holes") pv = ParamValue(ParamValue::PARAM_HOLES);
      else if (param_word == "") pv = ParamValue(ParamValue::PARAM_INVALID);
      else if (isdigit(param_word[0])) {
	pv = ParamValue(strtol(param_word.c_str(),0, 0));
      } else {
	cerr << "ParsedActionString: param unrecognized: "
	     << param_word << std::endl;
      }
      type_list.push_back(at);
      param_list.push_back(pv);
    }
  }
}

void
ObservingAction::Factory(std::list<string> &action_strings,
			 std::list<ObservingAction *> &action_list,
			 Strategy *strategy,
			 Session *session) {
  if (action_strings.size() == 0) {
    // If no actions are provided, the default is that this must be an
    // LPV target.
    std::list<string> lpv_group_list;
    lpv_group_list.push_back("LPV");
    ObservingAction::ScriptFactory(ParamValue(), /*NOTUSED*/
				   lpv_group_list,
				   1.0, // priority (default)
				   action_list, strategy, session);
  } else {
    for (std::string &one_string : action_strings) {
      ParsedActionString pas(one_string);
      
      cerr << "Found " << pas.number_of_actions() << " actions." << std::endl;

      for (int i=0; i<pas.number_of_actions(); i++) {
	switch (pas.type_of_action(i)) {
	case AT_Invalid:
	  break;
      
	case AT_Time_Seq:
	  if (pas.param_of_action(i).PV2Int() == ParamValue::PARAM_HOLES) {
	    ObservingAction::HoleFactory(pas.param_of_action(i),
					 pas.group_list(),
					 pas.priority(),
					 action_list, strategy, session);
	  } else {
	    ObservingAction::TimeSeqFactory(pas.param_of_action(i),
					    pas.group_list(),
					    pas.priority(),
					    action_list, strategy, session);
	  }
	  break;
      
	case AT_Quick:
	  ObservingAction::QuickFactory(pas.param_of_action(i),
					pas.group_list(),
					pas.priority(),
					action_list, strategy, session);
	  break;
      
	case AT_Script:
	  ObservingAction::ScriptFactory(pas.param_of_action(i),
					 pas.group_list(),
					 pas.priority(),
					 action_list, strategy, session);
	  break;

	default:
	  cerr << "ObservingAction::Factory: invalid action: "
	       << pas.type_of_action(i) << std::endl;
	}
      }
    }
  }
}
  
void
ObservingAction::TimeSeqFactory(ParamValue pv,
				list<string> &group_list,
				double priority,
				std::list<ObservingAction *> &action_list,
				Strategy *strategy,
				Session *session) {
  int candidates = 0;
  if ( not strategy->ValidEphemeris() ) return;
  JULIAN jd_ref = strategy->FetchJDRef();
  double periodicity = strategy->FetchEphemerisPeriodicity();
  JULIAN jd_start = session->SchedulingStartTime();
  JULIAN jd_end = session->SchedulingEndTime();

  // if a secondary eclipse, shift the JDref by half a period, and
  // keep the phase_start and phase_end centered around phase = 0.0.
  if (pv.PV2Int() == ParamValue::PARAM_SECONDARY_ECLIPSE) {
    const double offset = strategy->FetchSecondaryOffset();
    jd_ref.add_days(offset);
  }

  int orbit_at_start = (int) ((jd_start-jd_ref)/periodicity);
  int orbit_at_end =  1 + (int) ((jd_end-jd_ref)/periodicity);
  ObsInterval result;
  double phase_start = 0.0 - strategy->FetchEclipseDuration()/periodicity;
  double phase_end = 0.0 + strategy->FetchEclipseDuration()/periodicity;
  
  for (long orbit=orbit_at_start; orbit <= orbit_at_end; orbit++) {
    ObservingAction *oa = new ObservingAction(strategy, session, AT_Time_Seq);
    const double overlap = oa->EphemerisObservable(jd_ref,
						   phase_start,
						   phase_end,
						   periodicity,
						   orbit,
						   result);
    if (overlap > 0.8) {
      oa->SetInterval(result);
      oa->SetGroups(group_list);
      oa->SetPriority(priority);
      action_list.push_back(oa);
      candidates++;
    } else {
      delete oa;
    }
  }
  fprintf(stderr, "TimeSeqFactory: %d candidates for %s\n",
	  candidates, strategy->object());
}

void
ObservingAction::QuickFactory(ParamValue pv,
			      list<string> &group_list,
			      double priority,
			      std::list<ObservingAction *> &action_list,
			      Strategy *strategy,
			      Session *session) {
  ObservingAction *result = new ObservingAction(strategy, session, AT_Quick);
  if (pv.PV2Int() == ParamValue::PARAM_INVALID) {
    result->cadence = 3600.0; // one hour default
  } else {
    result->cadence = (double) pv.PV2Int();
  }
  result->SetExecutionTime(360.0); // 6 minute default time
  result->SetPriority(priority);
  result->SetGroups(group_list);
  result->next_set_number = 0;
  action_list.push_back(result);
}

void
ObservingAction::ScriptFactory(ParamValue pv,
			       list<string> &group_list,
			       double priority,
			       std::list<ObservingAction *> &action_list,
			       Strategy *strategy,
			       Session *session) {
  ObservingAction *result = new ObservingAction(strategy, session, AT_Script);
  const char *script = strategy->FetchScript();
  if (script != nullptr) {
    result->script = string(script);
  } else {
    result->script = string("");
  }
  result->SetPriority(priority);
  result->SetGroups(group_list);
  action_list.push_back(result);
}

ObservingAction::ObservingAction(Strategy *strategy, Session *session, ActionType type) :
  object_name(strategy ? strategy->object() : "N/A"),
  parent_strategy(strategy),
  parent_session(session) {

  if (not initialization_complete) ObsActionInitialize();

  oa_type = type;
  switch(oa_type) {
  case AT_Invalid:
    object_name = std::string("Invalid");
    break;
  case AT_Time_Seq:
    object_name = object_name + "(Time_Seq)";
    break;
  case AT_Quick:
    object_name = object_name + "(Quick)";
    next_set_number = 0;
    break;
  case AT_Script:
    break;
  case AT_Dark:
    object_name = "Dark";
    break;
  case AT_Flat:
    object_name = "Flat";
    break;
  }

  unique_id = next_unique_id++;
  uid_xref.emplace(unique_id, this);
}

ObservingAction::ObservingAction(string &one_line) :
  parent_strategy(nullptr), parent_session(nullptr) {
  if (not initialization_complete) ObsActionInitialize();
  // one_line format:
  // oa_ID oa_priority oa_type strategy_name [ { start_time end_time } | {cadence} ]
  constexpr int MAX_LEN = 132;
  if (one_line.length() >= MAX_LEN) {
    std::cerr << "ObservingAction::ObservingAction(string): string too long ("
	      << one_line.length() << " chars)." << std::endl;
  } else {
    char type_string[MAX_LEN];
    char strategy_name[MAX_LEN];
    double start_jd, end_jd;

    int num_fields = sscanf(one_line.c_str(), "%d %lf %s %s %lf %lf",
			    &unique_id, &priority, type_string, strategy_name,
			    &start_jd, &end_jd);
    if (strcmp(type_string, "Invalid") == 0) {
      std::cerr << "ObservingAction::ObservingAction(string): Invalid type is invalid."
		<< std::endl;
    } else if (strcmp(type_string, "Time_Seq") == 0) {
      oa_type = AT_Time_Seq;
      if (num_fields != 6) {
	std::cerr << "ObservingAction::ObservingAction(Time_Seq): wrong # args: "
		  << num_fields << std::endl;
      }
      start_time = JULIAN(start_jd);
      end_time = JULIAN(end_jd);
      object_name = string(strategy_name);
    } else if (strcmp(type_string, "Quick") == 0) {
      oa_type = AT_Quick;
      if (num_fields != 4) {
	std::cerr << "ObservingAction::ObservingAction(Quick): wrong # args: "
		  << num_fields << std::endl;
      }
      object_name = string(strategy_name);
      next_set_number = 0;
    } else if (strcmp(type_string, "Script") == 0) {
      oa_type = AT_Script;
      if (num_fields != 4) {
	std::cerr << "ObservingAction::ObservingAction(Script): wrong # args: "
		  << num_fields << std::endl;
      }
      object_name = string(strategy_name);
    } else if (strcmp(type_string, "Dark") == 0) {
      oa_type = AT_Dark;
      if (num_fields != 3) {
	std::cerr << "ObservingAction::ObservingAction(Script): wrong # args: "
		  << num_fields << std::endl;
      }
    } else if (strcmp(type_string, "Flat") == 0) {
      oa_type = AT_Flat;
      if (num_fields != 3) {
	std::cerr << "ObservingAction::ObservingAction(Script): wrong # args: "
		  << num_fields << std::endl;
      }
    } else {
      std::cerr << "ObservingAction::ObservingAction(): invalid type: "
		<< type_string << std::endl;
    }
  }
}

//****************************************************************
//        TypeString()
// Returns a std::string equal to "Time_Seq" or "Dark" or "Flat" or
//        "Quick" or "Script"
//****************************************************************
std::string
ObservingAction::TypeString(void) {
  switch(oa_type) {
  case AT_Time_Seq: return "Time_Seq";
  case AT_Quick: return "Quick";
  case AT_Script: return "Script";
  case AT_Dark: return "Dark";
  case AT_Flat: return "Flat";

  default:
  case AT_Invalid: return "Invalid";
  }
}
//****************************************************************
//        ToScheduleString()
//  Performs the exact inverse of ObservingAction(string)
// one_line format:
// oa_ID oa_priority oa_type strategy_name [ { start_time end_time } | {cadence} ]
//****************************************************************
std::string
ObservingAction::ToScheduleString(void) {
  const char *oa_type_string = nullptr;
  std::string extras;
  switch(oa_type) {
  case AT_Invalid:
    oa_type_string = " Invalid ";
    break;
  case AT_Time_Seq:
    oa_type_string = " Time_Seq ";
    extras = std::string(parent_strategy->object()) + ' ' +
      std::to_string(start_time.day()) + ' ' +
      std::to_string(end_time.day());
    break;
  case AT_Quick:
    oa_type_string = " Quick ";
    extras = std::string(parent_strategy->object()) + ' ' + std::to_string(cadence);
    break;
  case AT_Script:
    oa_type_string = " Script ";
    extras = object_name;
    break;
  case AT_Dark:
    oa_type_string = " Dark ";
    break;
  case AT_Flat:
    oa_type_string = " Flat ";
    break;
  default:
    oa_type_string = " Invalid ";
  }

  return std::to_string(unique_id) + oa_type_string
    + std::to_string(session_priority*priority)
    + ' ' + extras;
}

//****************************************************************
//        HoleFactory()
// Static (class) function that creates an ObservingAction for an
// observation to fill in a phase hole.
//****************************************************************
void
ObservingAction::HoleFactory(ParamValue pv,
			     list<string> &group_list,
			     double priority,
			     std::list<ObservingAction *> &action_list,
			     Strategy *strategy,
			     Session *session) {
  int num_candidates = 0;
  assert(strategy);
  assert(session);
  
  if ( not strategy->ValidEphemeris() ) return;
  JULIAN jd_ref = strategy->FetchJDRef();
  double periodicity = strategy->FetchEphemerisPeriodicity();
  JULIAN jd_start = session->SchedulingStartTime();
  JULIAN jd_end = session->SchedulingEndTime();

  int orbit_at_start = (int) ((jd_start-jd_ref)/periodicity);
  int orbit_at_end =  1 + (int) ((jd_end-jd_ref)/periodicity);
  
  for (auto hole : strategy->FetchHoles()) {
    for (long orbit=orbit_at_start; orbit <= orbit_at_end; orbit++) {
      ObsInterval result;
      ObservingAction *oa = new ObservingAction(strategy, session, AT_Time_Seq);
      const double overlap = oa->EphemerisObservable(jd_ref,
						     hole->start,
						     hole->end,
						     periodicity,
						     orbit,
						     result);
      if (overlap > 0.33) {
	oa->SetInterval(result);
	oa->SetGroups(group_list);
	oa->SetPriority(priority);
	action_list.push_back(oa);
	num_candidates++;
      } else {
	delete oa;
      }
    }
  }
  fprintf(stderr, "HoleFactory: %d candidates to fill %ld holes for %s\n",
	  num_candidates, strategy->FetchHoles().size(), strategy->object());
}

std::ostream&
operator<< (std::ostream &out, const ObservingAction &oa) {

  switch (oa.oa_type) {
  case AT_Invalid:
    out << oa.parent_strategy->object() << "::";
    out << "OA(AT_Invalid)";
    break;
    
  case AT_Time_Seq:
    out << oa.parent_strategy->object() << "::";
    out << "OA(AT_Time_Seq: ";
    out << oa.start_time.to_string() << " - "
	<< oa.end_time.to_string() << ")";
    break;
    
  case AT_Quick:
    out << oa.parent_strategy->object() << "::";
    out << "OA(AT_Quick, " << oa.cadence << ')';
    break;
    
  case AT_Script:
    out << oa.parent_strategy->object() << "::";
    out << "OA(AT_Script)";
    break;
    
  case AT_Dark:
    out << "OA(Dark)";
    break;
    
  case AT_Flat:
    out << "OA(AT_Flat)";
    break;
    
  default:
    out << "OA(<invalid_value>)";
  }
  return out;
}

double
  ObservingAction::IntervalObservable(const ObsInterval &interval, ObsInterval &result) {
  JULIAN jd_start = parent_session->SchedulingStartTime();
  JULIAN jd_end = parent_session->SchedulingEndTime();

  if (interval.end < jd_start or jd_end < interval.start) {
    result.fraction = 0.0;
    return 0.0;
  }

  result.start = interval.start;
  result.end = interval.end;

  if (interval.start < jd_start) {
    result.start = jd_start.day();
  }

  if (jd_end < interval.end) {
    result.end = jd_end.day();
  }
  
  const double interval_len = interval.end - interval.start;
  result.fraction = (result.end-result.start)/interval_len;
  return result.fraction;
}

double
ObservingAction::EphemerisObservable(JULIAN jd_ref,
				     double phase_start,
				     double phase_end,
				     double period,
				     long orbit_number,
				     ObsInterval &result) {
  ObsInterval oi;
  const double ref = jd_ref.day() + (period*orbit_number);
  oi.start = ref+phase_start*period;
  oi.end = ref+phase_end*period;
  return IntervalObservable(oi, result);
}

void
ObservingAction::SetGroups(list<string> g) {
  groups = g;
  for (const string &s : g) {
    if (group_xref.find(s) == group_xref.end()) {
      group_xref.insert({s, std::list<ObservingAction *>()} );
    }
    group_xref[s].push_back(this);
  }
}
 
void
PrintSummaryByGroups(void) {
  for (auto x : group_xref) {
    fprintf(stderr, "Group: %s\n", x.first.c_str());
    for (auto y : x.second) {
      cerr << *y << std::endl;
      //fprintf(stderr, "    %s\n", y.c_str());
    }
  }
  fprintf(stderr, "-----\n");
}

Schedule::strategy_time_pair *
ObservingAction::CreateExecutableSTP(const char *one_line) {
  unsigned long uid;
  char oa_type_string[32];/*NOTUSED*/
  char strategy_string[32];/*NOTUSED*/
  double start_time_raw, end_time_raw;
  Schedule::strategy_time_pair *lookup = nullptr;

  int num_fields = sscanf(one_line, "%ld %s %s %lf %lf",
			  &uid, oa_type_string, strategy_string,
			  &start_time_raw, &end_time_raw);
  if (num_fields == 4 or num_fields == 5) {
    auto x = uid_xref.find(uid);
    if (x == uid_xref.end()) {
      std::cerr << "Error in ObservingAction::Update(): invalid UID: "
		<< uid << std::endl;
    } else {
      const ObservingAction *oa = x->second;
      lookup = new Schedule::strategy_time_pair;
      *lookup = *(oa->stp);
      lookup->scheduled_time = JULIAN(start_time_raw);
      if (num_fields == 5) {
	lookup->scheduled_end_time = JULIAN(end_time_raw);
      }
    }
  } else {
    std::cerr << "ObservingAction::Update() wrong # fields: "
	      << one_line << std::endl;
  }
  return lookup;
}

list<ObservingAction *>
ObservingAction::GroupList(const string &group_name) {
  return group_xref[group_name];
}

// Provide score to the scheduler. In some cases, just invokes Strategy::score()
double
ObservingAction::score(JULIAN last_observation_time,
		       JULIAN oa_start_time,
		       JULIAN oa_end_time) {
  ALT_AZ alt_start, alt_finish;
  double min_alt = 0.0;
  double duration_days = oa_end_time - oa_start_time;
  double delta_t_days;
  double interval_factor = 1.0;

  if (oa_type == AT_Time_Seq || oa_type == AT_Quick) {
    DEC_RA where = parent_strategy->GetObjectLocation();
    alt_start = ALT_AZ(where, oa_start_time);
    alt_finish = ALT_AZ(where, oa_end_time);
    min_alt = alt_start.altitude_of();
    if(min_alt > alt_finish.altitude_of())
      min_alt = alt_finish.altitude_of();
  }

  switch(oa_type) {
  case AT_Invalid:
    return 0.0;

  case AT_Time_Seq:
    if (parent_strategy->IsVisible(oa_end_time) and
	parent_strategy->IsVisible(oa_start_time)) {
      return priority * session_priority *
	sin(min_alt) * duration_days * (24.0/0.3); // count of half-hours
    } else {
      return 0.0; // not visible
    }

  case AT_Quick:
    if (not parent_strategy->IsVisible(oa_start_time)) return 0.0;
    delta_t_days = oa_start_time - last_observation_time;
    if (delta_t_days > 1.1*CadenceDays()) {
      interval_factor = 1.1;
    } else {
      interval_factor = delta_t_days/CadenceDays();
    }
    return sin(min_alt) * interval_factor * priority * session_priority;

  case AT_Script:
    return priority * session_priority *
      parent_strategy->score(oa_start_time,
			     last_observation_time,
			     parent_session);

  case AT_Dark:
    return 1.0 * session_priority;
    break;

  case AT_Flat:
    return 1.0 * session_priority;
    break;

  default:
    fprintf(stderr, "ObservingAction::score(): invalid oa_type\n");
    return 0.0;
  }
  /*NOTREACHED*/
}

Execution_Result
ObservingAction::execute(Session *session) {
  int message_id;
  if (ReceiveMessage("simple_session", &message_id)) {
    bool force_shutdown = false;
    if (message_id == SM_ID_Abort) {
      force_shutdown = true;
    }
    if (message_id == SM_ID_Pause) {
      bool quit = false;
      session->log(LOG_INFO, "Received pause message. Starting pause.");
      while (!quit) {
	sleep(1);
	if (ReceiveMessage("simple_session", &message_id)) {
	  if (message_id == SM_ID_Resume) {
	    session->log(LOG_INFO, "Received resume message. Resuming.");
	    quit = true;
	    break;
	  } else if (message_id == SM_ID_Abort) {
	    force_shutdown = true;
	  } else if (message_id == SM_ID_Pause) {
	    session->log(LOG_INFO, "Received pause message. Continuing pause.");
	  }
	}
      }
    } // end pause

    if (force_shutdown) {
      session->log(LOG_INFO, "Received abort message. Quitting strategy.");
      return PERFORM_SESSION_SHUTDOWN;
    }
  }

  char buffer[432];
  int response;
  
  switch(oa_type) {
  case AT_Invalid:
    fprintf(stderr, "ObservingAction::execute(): cannot execute AT_Invalid.\n");
    return NO_STARS;

  case AT_Time_Seq:
    session->log(LOG_INFO, "Time_Seq requested, but don't have procedure.");
    return NO_STARS;

  case AT_Quick:
    {
      {
	const std::string filtername(strategy()->GetQuickFilterName());
	focus_check(session, filtername, true);
      }
      JULIAN start_time(time(0));
      session->log(LOG_INFO, "Starting Quick Obs for %s",
		   GetObjectName());
      Finder finder(strategy(), session);
      //finder.SetBadPixelAvoidance(true);
      FinderResult result = finder.Execute();
      if (result == FINDER_OKAY) {
	DB_Measurement measurement(*session->astro_db, strategy()->object());
	exposure_flags quick_flags("photometry");
	const int num_exposures = strategy()->GetQuickNumExposures();
	const double exp_time = strategy()->GetQuickExposureTime();
	quick_flags.SetFilter(strategy()->GetQuickFilterName());
	
	for (int i=0; i<num_exposures; i++) {
	  const char *filename = expose_image_next(exp_time,
						   quick_flags, "PHOTOMETRY");

	  // add OBJECT and SETNUM to FITS file
	  ImageInfo info(filename);
	  double airmass {0.0};
	  double midpoint_jd {0.0};
	  info.SetObject(GetObjectName());
	  info.SetSetNum(next_set_number);
	  info.WriteFITS();

	  if (info.AirmassValid()) {
	    airmass = info.GetAirmass();
	  }
	  if (info.ExposureMidpointValid()) {
	    midpoint_jd = info.GetExposureMidpoint().day();
	  }

	  session->log(LOG_INFO, "Quick exposure for %s: %f secs: %s",
		       GetObjectName(), exp_time, filename);
	  measurement.AddExposure(filename,
				  strategy()->GetQuickFilterName(),
				  midpoint_jd,
				  exp_time,
				  airmass,
				  strategy()->ObjectChart(),
				  true, // needs_dark
				  true); // needs_flat
	}
	measurement.Close(true); // create stack
      }
      session->log(LOG_INFO, "Done with Quick Obs for %s",
		   GetObjectName());
      next_set_number++;

      ObsRecord::Observation *obs = new ObsRecord::Observation;
      obs->empty_record = false;
      obs->when = JULIAN(time(0));
      obs->starname = strdup(strategy()->object());
      obs->what = strategy();
      obs->execution_time = (obs->when.day() - start_time.day()) * 24.0*3600.0;
      obs_record->RememberObservation(*obs);
      obs_record->Save();
      
      return OKAY;
    }

  case AT_Script:
    return strategy()->execute(session);

  case AT_Dark:
    session->log(LOG_INFO, "Generating darks.");
    response = system("/home/mark/ASTRO/CURRENT/TOOLS/DARK_MANAGER/make_standard_darks.sh");
    return OKAY;

  case AT_Flat:
    session->log(LOG_INFO, "Moving flatlight up.");
    response = system("flatlight -u -s -w");
    response = system("flatlight -u");
    sprintf(buffer, "auto_all_filter_flat -o %s",
	    session->Session_Directory());
    response = system(buffer);
    session->log(LOG_INFO, "Moving flatlight down.");
    response = system("flatlight -d -w");
    response = system("flatlight -d -w");
    return ((response == 0) ? OKAY : NOT_VISIBLE);

  default:
    fprintf(stderr, "ObservingAction::execute(): invalid oa_type\n");
  }
  return NO_STARS; // not quite accurate, but may be useful by
		   // triggering another attempt
}
