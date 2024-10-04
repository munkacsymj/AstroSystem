// This may look like C code, but it is really -*- C++ -*-
/*  strategy.h -- manages the execution of an observation for a
 *  single object according to the object's "strategy"
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
#ifndef _STRATEGY_H
#define _STRATEGY_H
#include <stdio.h>
#include <julian.h>
#include <dec_ra.h>
#include <HGSC.h>
#include <Image.h>
#include <list>
#include "plan_exposure.h"
#include <system_config.h>

enum Execution_Result {
  NOT_VISIBLE,			// not in sky; reschedule possible
  OKAY,				// images okay, all done
  LOST_IN_SPACE,		// telescope mount failure
  NO_STARS,			// bad weather? bad camera?
  POOR_IMAGE,			// windy
  PERFORM_SESSION_SHUTDOWN,	// need to shut down the telescope
};

class Session;			// forward declaration
class StrategyList;		// forward declaration
class ObservingAction;		// forward declaration

struct ObsInterval {
  double start;			// sometimes holds phase; sometimes JD
  double end;
  double fraction;		// always in range [0..1]
};

typedef ObsInterval ObservingHole; // uses phase [0..1] for start & end

class Strategy {
public:
  Strategy(const char *object_name, Session *session=nullptr);
  ~Strategy(void);

  // This method is used in the development of a schedule
  int include_in_schedule(Session *session); // 1=yes, 0=no

  double score(JULIAN observation_time, JULIAN last_observation_time, Session *session) const ;

  double execution_time_prediction(void) const ; // seconds

  Execution_Result execute(Session *session);
  static void FindAllStrategies(Session *session);
  static Strategy *FindStrategy(const char *name);
  static void RebuildStrategyDatabase(void);

  const char *object(void) const { return object_name; }
  const char *Designation(void) const {return designation; }
  // AAVSOName is the name that is used to look up this star in the
  // AAVSO VSP tool.
  const char *AAVSOName(void) const { return (aavso_name[0] ?
					      aavso_name : object_name); }
  const char *ReportName(void) const { return (report_name[0] ?
					       report_name :
					       object_name); }
  const char *RawReportName(void) const { return report_name; }
  bool IsStandardField(void) const { return is_standard_field; }

  // remarks() is guaranteed to always provide a trailing '\n', unless
  // there is a blank string, in which case a NULL pointer will be
  // returned. 
  const char *remarks(void) const { return object_remarks; }
  // ReportNotes() is what is put into the "notes" field of the AAVSO report 
  const char *ReportNotes(void) const { return report_notes; }

  // return 1 if object is in visible part of the sky; return 0 if
  // below observing horizon
  int IsVisible(JULIAN when) const ;

  // return a list of all the strategies that are children of this
  // parent strategy.
  StrategyList *ChildStrategies(void) { return child_strategies; }

  // returns zero if this strategy is a standalone strategy. Returns a
  // one if this strategy is a child of another one. Children
  // strategies should not be given their own exposures.
  int IsAChildStrategy(void) { return is_a_child; }

  const char *ReferenceStar(void) { return reference_star; }

  void SetLastObservation(JULIAN when) { last_observation = when; }
  JULIAN GetLastObservationTime(void) const { return last_observation; }
  void SetLastExecutionDuration(double how_long) { last_execution_duration = how_long; }

  DEC_RA GetObjectLocation(void) { return object_location; }

  char *ObjectChart(void) { return chart; }
  bool AutoUpdatePhotometry(void) { return phot_auto_update; }

  // the image parameter is used to get the initial WCS transformation
  DEC_RA UpdateTargetForBadPixels(Image *image);

  const char *FetchScript(void) { return object_script; }
  std::list<ObservingHole *> &FetchHoles(void) { return hole_list; }
  bool ValidEphemeris(void) { return (ephemeris_ref.day() != 0.0 &&
				      ephemeris_period != 0.0); }
  JULIAN FetchJDRef(void) { return ephemeris_ref; }
  double FetchEphemerisPeriodicity(void) { return ephemeris_period; }
  double FetchSecondaryOffset(void) { return secondary_offset; } // measured in days
  double FetchEclipseDuration(void) { return event_length; }

  double GetFinderExposureTime(void) { return finder_exposure_time; }
  double GetOffsetEast(void) { return offset_e; } // arc-radians
  double GetOffsetNorth(void) { return offset_n; } // arc-radians
  double GetOffsetTolerance(void) { return offset_tolerance; } // arc-radians

  double GetQuickExposureTime(void) { return quick_exposure_time;} // seconds, for QUICK sequences
  int GetQuickNumExposures(void) { return quick_num_exposures;} // for QUICK
  const char *GetQuickFilterName(void) { return quick_filter_name;}

  static void BuildObservingActions(Session *session);

private:
  enum PredefinedPeriodicity { ALWAYS, WEEKLY, DAILY, NEVER };
  class PERIODICITY {
  public:
    PERIODICITY(PredefinedPeriodicity p) {
      if(p == ALWAYS) p_in_days = 0.0;
      else if(p == WEEKLY) p_in_days = 7.0;
      else if(p == DAILY) p_in_days = 1.0;
      else if(p == NEVER) p_in_days = -1.0;
      else fprintf(stderr, "PERIODICITY: illegal value\n");
    }
    PERIODICITY(double period) { p_in_days = period; }
    PERIODICITY(void) { p_in_days = -1.0; /*never*/ }
    double PeriodicityInDays(void) const { return p_in_days; }
    double PeriodicityMatches(const PredefinedPeriodicity p) const {
      if(p == ALWAYS) return p_in_days == 0.0;
      if(p == WEEKLY) return p_in_days == 7.0;
      if(p == DAILY) return p_in_days == 1.0;
      if(p == NEVER) return p_in_days == -1.0;
      return 0.0;
    }
  private:
    double p_in_days;
  } periodicity;
    
  const SystemConfig *configuration;
  int default_left_column;	// depends on camera
  int default_top_row;
  int default_right_column;
  int default_bottom_row;
  
  bool strategy_enabled;       // include in the schedule?
  char designation[16];		// "1242+04"
  char object_name[32];		// "ru-vir"
  char aavso_name[32];		// "RU Vir"
  char *object_remarks;		// NULL or has trailing '\n'
  char *object_script;		// NULL or string
  char *report_notes;		// NULL or string without trailing '\n'
  const char *finder_imagename;	// filename of last good finder image
  DEC_RA object_location;
  char chart[32];		// ???
  double offset_n;		// radians, +=N, -=S
  double offset_e;		// arc-radians, +=E, -=W
  double offset_tolerance;      // radians
  DEC_RA target_location;
  double priority;		// observation priority. +1.0 is nominal
  double planning_time;		// typical execution duration, in minutes
  bool auto_sequence;		// true->use historical mag data to
				// set num_exp and exposure_time for
				// each filter.
  bool is_standard_field;	// An AAVSO standard field (with
				// nonstandard name) 

  bool use_historical_planning_time;
  char *report_name;		// if AAVSO reporting name is very
				// different from our local name.
  bool phot_auto_update;

  double quick_exposure_time; // seconds, for QUICK sequences
  int    quick_num_exposures; // for QUICK
  const char *quick_filter_name;

  char reference_star[32];
  const static int MAX_NUM_FILTERS = 5;
  int number_filters;		// number of filters to use
  const char *filter_name[MAX_NUM_FILTERS];
  const char *filter_letter[MAX_NUM_FILTERS];
  PhotometryColor filter_color[MAX_NUM_FILTERS];
  double main_exposure_time[MAX_NUM_FILTERS]; // seconds, main exposure
  FilterExposurePlan exposure_plan[MAX_NUM_FILTERS];
  double finder_exposure_time;	// seconds, finding exposure
  int    number_exposures[MAX_NUM_FILTERS]; // # of main exposures
  JULIAN last_observation;
  double last_execution_duration; // seconds or NAN or 0.0

  int    stack_exposures;	// 1=yes, 0=no

  Execution_Result Actual_Result;
  int    executed;		// 1=yes, 0=not yet
  int DoFinder(Session *session, DEC_RA &target_location);

  StrategyList *child_strategies; // nil = no children exist
  int is_a_child;		// 1 = is-a-child-of-something-else

  // returns true if successful; okay to call reentrantly
  bool ReadStrategyFile(const char *filename, Session *session, const char *object_name);
  // a helper to ReadStrategyFile
  void InterpretExposurePlanString(std::list<std::string> &ref_stars,
				   std::string planning_string,
				   const char *object_name);

  std::list<ObservingAction *> action_list;
  JULIAN ephemeris_ref;
  double ephemeris_period;	// measured in days
  double event_length;		// measured in days
  double secondary_offset;	// measured in days
  std::list<string> observe_strings;
  std::list<ObservingHole *> hole_list;
  std::list<std::string> exposure_reference_stars;
};

class StrategyList {
public:
  StrategyList(void);
  ~StrategyList(void);

  int NumberStrategies(void) { return strategy_count; }

  // strategies in a StrategyList are referenced by index number. It
  // works the same way array indices work (0 to (strategy_count-1)). 
  Strategy *Get(int i);

  // Add a strategy to a StrategyList.
  void Add(Strategy *s);

private:
  Strategy **main_list;
  int strategy_count;		// number that actually exist
  int array_size;		// allocated size of main_list

};
#endif
