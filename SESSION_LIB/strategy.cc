/*  strategy.cc -- manages the execution of an observation for a
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
#include <unistd.h>		// unlink()
#include <stdlib.h>		// system(), mkstemp()
#include <string.h>		// strdup
#include <ctype.h>		// toupper, isdigit, isspace
#include <sys/types.h>		// (DIR *)
#include <dirent.h>		// opendir(), ...
#include <iostream>
#include <assert.h>
#include <scope_api.h>
#include <camera_api.h>
#include <Image.h>
#include <IStarList.h>
#include <HGSC.h>
#include <alt_az.h>
#include <named_stars.h>
#include <mount_model.h>
#include "strategy.h"
#include "observing_action.h"
#include <visibility.h>
#include <gendefs.h>
#include "finder.h"
#include "session.h"
#include "obs_record.h"
#include "obs_spreadsheet.h"
#include "proc_messages.h"
#include <StrategyDatabase.h>
#include "script_out.h"
#include "validation.h"
#include "focus_manager.h"
#include "plan_exposure.h"
#include <gendefs.h>
#include "mag_from_image.h"
#include <bad_pixels.h>

// Manage parent->child relationships in strategies. 

// A "child" strategy is one that has no exposures of its
// own. Instead, it represents a star that is found in exposures made
// of its "parent". Thus, a child strategy is not scheduled into a
// nightly schedule, but needs to be recognized during analysis, so
// that data on the child is analyzed. We discover parent->child
// relationships while the strategies are being read. Each child
// strategy contains a PARENT=objectname line which is read and stored
// in the "struct crosslink" below. Then, after all have been read in,
// you need to call FixAllCrosslinks() to actually build all the links
// that are needed in the strategies themselves.
struct crosslink {
  Strategy *child;
  char *parent_object_name;
  struct crosslink *next;
} *FirstCrosslink = 0;

// After strategies have been read in, the crosslink list will contain
// a set of parent->child references that haven't been populated in
// the Strategy structures themselves (because we can't guarantee the
// order that the Strategies are actually read in, so we can't handle
// forward references until we know that everything has been read).
void FixAllCrosslinks(void) {
  // we delete the crosslinks at the same time as we fix them up.
  while(FirstCrosslink) {
    Strategy *parent_link =
      Strategy::FindStrategy(FirstCrosslink->parent_object_name);
    if(parent_link == 0) {
      fprintf(stderr,
	      "FixAllCrosslinks: no parent strategy named %s for %s\n",
	      FirstCrosslink->parent_object_name,
	      FirstCrosslink->child->object());
    } else {
      parent_link->ChildStrategies()->Add(FirstCrosslink->child);
    }

    //struct crosslink *finished = FirstCrosslink;
    FirstCrosslink = FirstCrosslink->next;
    //free(finished->parent_object_name);
    //delete finished;
  }
}
 
static std::list<Strategy *> all_strategies;

static ObsRecord *obs_record = 0;
static const char *strategy_directory = STRATEGY_DIR;

// Strategy files are determined by the name of the object, and a
// strategy. For now, we ignore strategy; it's there for the future.
static const char *strategy_filename(const char *object_name) {
  static char buffer[132];
  char namebuffer[80];

  strcpy(namebuffer, object_name);
  if (namebuffer[0] == 'g' and
      namebuffer[1] == 's' and
      namebuffer[2] == 'c') {
    namebuffer[0] = 'G';
    namebuffer[1] = 'S';
    namebuffer[2] = 'C';
  }

  sprintf(buffer, "%s/%s.str", strategy_directory, namebuffer);
  return buffer;
}

// Convert multi-char filter name to one-letter name
static const char *FilterName2Letter(const char *name) {
  if (strcmp(name, "Vc") == 0 or
      strcmp(name, "Bc") == 0 or
      strcmp(name, "Rc") == 0 or
      strcmp(name, "Ic") == 0) {
    char *answer = strdup(name);
    answer[1] = 0;
    return answer;
  } else if (strlen(name) == 1) {
  } else {
    fprintf(stderr, "FilterName2Letter() can't convert %s\n", name);
  }
  return strdup(name);
}
    
// return 1 on success, 0 on failure
static int GetOffset(const char *s, // in: input string
		     double *value, // out: double value found(radians)
		     char *direction, // out: NSE or W
		     const char **next) { // out: next character to scan
  // skip leading whitespace (shouldn't be any to start with, since
  // line was scrunched before it got to us here).
  while(isspace(*s)) s++;

  double negative = 1.0;	// either +1 or -1
  // should be pointing at number at this point
  if(*s == '-') {
    negative = -1.0;
    s++;
  }
  
  if(!isdigit(*s) && *s != '.') return 0;

  int int_part = 0;
  while(isdigit(*s)) {
    int_part *= 10;
    int_part += (*s - '0');
    s++;
  }

  // now either points to decimal point or to direction letter.
  double fraction_part = 0.0;
  double fraction_places = 1.0;
  if(*s == '.') {
    s++;
    while(isdigit(*s)) {
      fraction_places *= 10.0;
      fraction_part += ((double) (*s - '0'))/fraction_places;
      s++;
    }
  }

  // now MUST point at a direction letter
  if(! *s) return 0;
  
  char dir_letter = toupper(*s);
  if(dir_letter == 'E' ||
     dir_letter == 'N' ||
     dir_letter == 'W' ||
     dir_letter == 'S') {
    *next = (s+1);
    *direction = dir_letter;
    *value = (negative * (int_part + fraction_part)) * ((1.0/60.0)*M_PI/180.0);
    return 1;
  } else
    return 0;
}

// sort of copy from first param to second param. Start the copy with
// the first non-blank character that follows the first '=' in the
// first parameter's string. Continue copying until the last non-blank
// character in the input string.
void AltValueWithSpaces(const char *s,
			char *d) {
  *d = 0;
  // Find the "=" character
  while(*s && *s != '=') s++;
  if(!*s) return;// error: no '=' found

  s++;
  while(*s && (*s == ' ' || *s == '\t')) s++;
  // now pointing to first non-blank after the first = char
  char *endit = d;
  while(*s) {
    *d++ = *s;
    if(*s != ' ' && *s != '\n' && *s != '\t') endit = d;
    s++;
  }
  *endit = 0;
}

// Establish a strategy. Read in the corresponding strategy file. If
// the object name doesn't correspond to an existing strategy file,
// print an error message onto stderr and continue with a rather empty
// strategy.
Strategy::Strategy(const char *Object_Name, Session *session) :
  configuration(session ? &(session->configuration) : nullptr) {
  // setup defaults
  if (session) {
    if (configuration->IsST9()) {
      default_left_column = 0;
      default_bottom_row = 0;
      default_top_row = 511;
      default_right_column = 511;
    } else if (configuration->IsQHY268M()) {
      default_left_column = 600;
      default_bottom_row = 0;
      default_right_column = 5679;
      default_top_row = 4209;
    } else {
      fprintf(stderr, "strategy.cc: ERROR: configuration.camera() isn't ST-9 or QHY268M\n");
      exit(-2);
      /*NOTREACHED*/
    }
  }
  designation[0] = 0;
  periodicity = WEEKLY;
  strcpy(object_name, Object_Name);
  object_remarks = 0;		// default NULL return
  report_notes = 0;
  object_script  = 0;		// default NULL
  finder_imagename = 0;
  chart[0] = 0;
  offset_n = offset_e = 0.0;
  offset_tolerance = (2.0/60.0)*(M_PI/180.0);	// radians
  reference_star[0] = '\0';
  number_filters = 1;
  main_exposure_time[0] = 1.0;
  exposure_plan[0].eQuantity = 0; // zero exposures means not used
  filter_name[0] = "Vc";
  filter_letter[0] = "V";
  finder_exposure_time = 20.0;
  number_exposures[0] = 1;
  stack_exposures = 0;
  is_standard_field = false;
  planning_time = 0.0;
  priority = 1.0;
  executed = 0;
  phot_auto_update = true;
  auto_sequence = true;
  use_historical_planning_time = true;
  child_strategies = new StrategyList;
  is_a_child = 0;
  aavso_name[0] = 0;
  report_name = strdup("");

  const char *filename = strategy_filename(object_name);
  if (!ReadStrategyFile(filename, session, Object_Name)) {
    fprintf(stderr, "Error reading strategy file.\n");
  }
  
  NamedStar ref_star(Object_Name);
  if(ref_star.IsKnown()) {
    object_location = ref_star.Location();
  } else if(!is_a_child) {
    fprintf(stderr, "Strategy: %s not in named star catalog.\n",
	    object_name);
  }
}
    
	    
Strategy::~Strategy(void) {
  ;
}

// This method is used in the development of a schedule
int
Strategy::include_in_schedule(Session *session) { // 1=yes, 0=no
  // should take into account length of time since last observed
  return 1;
}

double
Strategy::score(JULIAN observation_time,
		JULIAN last_observation_time,
		Session *session) const {
  // verify visible at both start and end
  JULIAN end_time =
    observation_time.add_days(execution_time_prediction()/(3600 * 24.0));
  
  // if not visible, score is zero
  if((!IsVisible(observation_time)) ||
     (!IsVisible(end_time))) return 0.0;

  ALT_AZ alt_start(object_location, observation_time);
  ALT_AZ alt_finish(object_location, end_time);

  // determine altitude (worst-case)
  double min_alt = alt_start.altitude_of();
  if(min_alt > alt_finish.altitude_of())
    min_alt = alt_finish.altitude_of();

  double days_since_last_obs = (observation_time - last_observation_time);
  double periodicity_factor = 1.0;
  if(periodicity.PeriodicityMatches(ALWAYS)) {
    periodicity_factor = 1.0;
  } else if(periodicity.PeriodicityMatches(NEVER)) {
    periodicity_factor = 0.0;
  } else {
    const double n = periodicity.PeriodicityInDays();
    if(days_since_last_obs < n*(5.0/7.0)) {
      periodicity_factor = days_since_last_obs/n;
    } else if(days_since_last_obs > n) {
      periodicity_factor = 1.0 + ((days_since_last_obs-n)/(3.0*n));
      if(periodicity_factor > 2.0) periodicity_factor = 2.0;
    } else {
      periodicity_factor = (days_since_last_obs - (n*(5.0/7.0)))/(2.0*n/7.0);
    }
  }

  double this_score= periodicity_factor * sin(min_alt);
  if(this_score > 2.0) {
    fprintf(stderr, "peridicity_factor = %f, this_score = %f\n",
	    periodicity_factor, this_score);
  }
  return this_score*priority;
}

double
Strategy::execution_time_prediction(void) const { // seconds
  if (isnormal(last_execution_duration)) {
    return last_execution_duration;
  }
  if (planning_time) {
    return 60.0 * planning_time;
  }
  // slew + 2 finder exposures + main exposure
  return 30 + 2*(15+finder_exposure_time) +
    number_exposures[0]*(15+main_exposure_time[0]);
}

/****************************************************************/
/*        GetSafetyLimit()					*/
/****************************************************************/
void GetSafetyLimit(double *eastern_limit,
		    double *western_limit) { // angles in degrees

#ifdef GM2000
  *eastern_limit = 0.0;
  *western_limit = 0.0;
#else
  char message[32];
  char response[32];
  ScopeResponseStatus status;
  int west_deg, west_min, east_deg, east_min;

  BuildMI250Command(message,
		    MI250_GET,
		    220);	// Get PEC value
  if(scope_message(message,
		   RunFast,
		   StringResponse,
		   response,
		   strlen(response),
		   &status)) {
    fprintf(stderr, "scope_interface.cc: error quering for worm position\n");
    return;
  }
  if(sscanf(response, "%dd%d;%dd%d",
	    &east_deg, &east_min, &west_deg, &west_min) != 4) {
    fprintf(stderr,
	    "scope_interface.cc: GetSafetyLimit cannot parse scope response: '%s'\n",
	    response);
    return;
  } else {
    // force conversion to double and decimal degrees. All angles positive?
    *eastern_limit = east_deg + (east_min/60.0);
    *western_limit = west_deg + (west_min/60.0);
  }
#endif
}

Execution_Result
Strategy::execute(Session *session) {
  {
    const std::string default_filter("V");
    focus_check(session, default_filter, true); // allow_slew = true
  }

  session->log(LOG_INFO, "Starting strategy for %s (%s)",
	       object_name, designation);

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
	    quit = true;
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

  // verify visibiliby
  if(!IsVisible(JULIAN(time(0)))) {
    session->log(LOG_ERROR, "%s not visible. Strategy aborted.", object_name);
    return NOT_VISIBLE;
  }

  // These two variables are used to compute the actual execution time
  // of the strategy (for future planning purposes)
  JULIAN strategy_start_time(time(0));
  double non_strategy_time_secs = 0.0;
  
  // Verify we have a dark for our "finder" exposure
  JULIAN dark_start(time(0));
  session->verify_dark_available(finder_exposure_time, 1);
  JULIAN dark_end(time(0));

  // This is the amount of time in seconds that we weren't actually
  // doing something useful for this strategy.
  non_strategy_time_secs += (dark_end.day() - dark_start.day())*24.0*3600.0;
  
  // That might have taken a while, if we needed to make a bunch of
  // dark exposures. Therefore, we now tweak the focus if tempterature
  // has dropped.

  // 9/11/2007: disabled to implement fuzzy focus algorithm after
  // "normal" exposures 

  // session->check_focus_using_temp();
  
  if(!DoFinder(session, target_location)) {
    session->log(LOG_ERROR,
		 "Lost trying to identify area around %s. Strategy aborted.",
		 object_name);
    return LOST_IN_SPACE;
  }

  // otherwise, Finder() succeeded and we're pointing at the right
  // spot
  {
    Image finder_image(finder_imagename);
    AddImageToExposurePlanner(finder_image, finder_imagename);
  }

  // before we start making main exposures, we run any script that
  // might have been provided
  if(object_script) {
    char script_filename[132];
    char script_results[132];

    sprintf(script_filename, "/tmp/script%d.txt", (int) getpid());
    sprintf(script_results, "/tmp/script%d.out", (int) getpid());

    FILE *fp_script = fopen(script_filename, "w");
    if(!fp_script) {
      fprintf(stderr, "strategy: cannot create script file in /tmp\n");
    } else {
      fputs(object_script, fp_script);
      fclose(fp_script);

      char command_buffer[512];
      sprintf(command_buffer,
	      "execute_script -n %s -i %s -d %s -e %s -o %s\n",
	      object_name,
	      finder_imagename,
	      session->dark_name(finder_exposure_time, 1, false),
	      script_filename,
	      script_results);

      fprintf(stderr, "Executing: %s", command_buffer);
      
      if(system(command_buffer) >= 0) {
	// parse the output file, looking for variable values
	ParameterSet::ResultStatus result;
	Script_Output output(script_results, 0);
	ParameterSet params(&output);

	params.DefineParameter("filters", ParameterSet::LIST_VALUE);
	params.DefineParameter("number_exposures", ParameterSet::VARIANT);
	params.DefineParameter("main_exposure_time", ParameterSet::VARIANT);

	number_filters = params.GetListSize("filters", result);
	if (result == ParameterSet::NO_VALUE || number_filters == 0) {
	  // no filters specified; set up for a single V filter
	  session->log(LOG_INFO, "Script: setup for default V filter");
	  number_filters = 1;
	  filter_name[0] = "Vc";
	  filter_letter[0] = "V";
	  filter_color[0] = PHOT_V;
	  double script_exposure_time =
	    params.GetValueDouble("main_exposure_time", result, "Vc");
	  if (result == ParameterSet::PARAM_OKAY) {
	    main_exposure_time[0] = script_exposure_time;
	  }
	  session->log(LOG_INFO, "V exposure time set to %lf",
		       main_exposure_time[0]);

	  int script_num_exposures = params.GetValueInt("number_exposures",
							result, "Vc");
	  if (result == ParameterSet::PARAM_OKAY) {
	    number_exposures[0] = script_num_exposures;
	  }
	  session->log(LOG_INFO, "V number exposures set to %d",
		       number_exposures[0]);
	} else {
	  for (int i=0; i<number_filters; i++) {
	    filter_name[i] = params.GetValueString("filters", result, 0, i);
	    filter_letter[i] = FilterName2Letter(filter_name[i]);

	    if (result == ParameterSet::PARAM_OKAY) {
	      Filter filter(filter_name[i]);
	      filter_color[i] = FilterToColor(filter);
	      double script_exposure_time =
		params.GetValueDouble("main_exposure_time",
				      result, filter_name[i]);
	      if (result == ParameterSet::PARAM_OKAY) {
		main_exposure_time[i] = script_exposure_time;
	      } else {
		main_exposure_time[i] = main_exposure_time[0];
	      }
	      int script_num_exposures =
		params.GetValueInt("number_exposures", result, filter_name[i]);
	      if (result == ParameterSet::PARAM_OKAY) {
		number_exposures[i] = script_num_exposures;
	      } else {
		number_exposures[i] = number_exposures[0];
	      }
	    }
	  }
	}
      }
      //unlink(script_filename);
      //unlink(script_results);
    }
  }

  //****************************************************************
  //        Exposure planning using plan_exposure
  //    Must be prepared for possibility that plan_exposure fails, so
  //    must then must fall back on other exposure planning
  //    techniques. 
  //****************************************************************
  bool filter_locked[MAX_NUM_FILTERS]{false};
  bool any_missing = true;
  ExposurePlanList epl;
  epl.exposure_plan_valid = true; // valid, but empty
  HGSCList catalog(object_name);
  for (int i=0; i<number_filters; i++) {
    exposure_plan[i].eQuantity = 0;
  }
  
  if (exposure_reference_stars.size() > 0) {
    fprintf(stderr, "Using new plan_exposure algorithm.\n");
    session->log(LOG_INFO, "Using new plan_exposure algorithm.\n");
    
    MagnitudeList B_mags, V_mags, R_mags, I_mags;
    for (auto star : exposure_reference_stars) {
      HGSC *cat_star = catalog.FindByLabel(star.c_str());
      if (!cat_star) {
	session->log(LOG_INFO, "%s: AUTOEXPOSURESTARS star named %s not recognized",
		     object_name, star.c_str());
      } else {
	if (cat_star->do_submit) {
	  // variable, have to get brightness from finder image
	  double finder_mag =
	    magnitude_from_image(finder_imagename,
				 session->dark_name(finder_exposure_time, 1, false),
				 star.c_str(),
				 object_name);
	  fprintf(stderr, "extracted V mag for %s is %.1lf\n", star.c_str(), finder_mag);
	  session->log(LOG_INFO, "Extracted V mag for %s is %.1lf", star.c_str(), finder_mag);
    
	  if (!isnormal(finder_mag)) {
	    fprintf(stderr, "...so using V mag of 15.1\n");
	    finder_mag = 15.1;
	  }
	  V_mags.push_back(finder_mag);
	  for (int filter_index = 0; filter_index < number_filters; filter_index++) {
	    const char filter_letter = this->filter_letter[filter_index][0];
	    // object_name is wrong. star.c_str() is closer, but
	    // probably won't work on secondary variables. 
	    const double predicted_mag = obs_record->PredictBrightness(star.c_str(),
								 filter_letter,
								 finder_mag);
	    if (isnormal(predicted_mag)) {
	      switch(filter_letter) {
	      case 'V': V_mags.push_back(predicted_mag); break;
	      case 'B': B_mags.push_back(predicted_mag); break;
	      case 'R': R_mags.push_back(predicted_mag); break;
	      case 'I': I_mags.push_back(predicted_mag); break;
	      }
	    }
	    fprintf(stderr, "    predicted mag for %c is %.1lf\n",
		    filter_letter, predicted_mag);
	    session->log(LOG_INFO, "    Predicted mag for %c is %.1lf",
			 filter_letter, predicted_mag);
	  }
	} else {
	  // isn't a variable. Use the mags from the catalog
	  fprintf(stderr, "Exposure ref star: %s\n", star.c_str());
	  session->log(LOG_INFO, "Exposure ref star: %s\n", star.c_str());

	  for (int filter_index = 0; filter_index < number_filters; filter_index++) {
	    const char filter_letter = this->filter_letter[filter_index][0];
	    if (not cat_star->multicolor_data.IsAvailable(filter_color[filter_index]))
	      continue;
	    const double predicted_mag = cat_star->multicolor_data.Get(filter_color[filter_index]);
	    fprintf(stderr, "    catalog mag for %c is %.1lf\n",
		    filter_letter, predicted_mag);
	    session->log(LOG_INFO, "    Catalog mag for %c is %.1lf",
			 filter_letter, predicted_mag);

	    switch(filter_letter) {
	    case 'V': V_mags.push_back(predicted_mag); break;
	    case 'B': B_mags.push_back(predicted_mag); break;
	    case 'R': R_mags.push_back(predicted_mag); break;
	    case 'I': I_mags.push_back(predicted_mag); break;
	    }
	  }
	}
      }
    }
    ColorMagnitudeList ml;
    ml.insert({ {PHOT_V, V_mags},
		{PHOT_B, B_mags},
		{PHOT_R, R_mags},
		{PHOT_I, I_mags}});
    epl = GetExposurePlan(ml);

    if (epl.exposure_plan_valid) {
      any_missing = false;
      for (int filter_index = 0; filter_index < number_filters; filter_index++) {
	bool matched = false;
	exposure_plan[filter_index].eQuantity = 0;
	
	for (auto c : epl.exposure_plan_list) {
	  const PhotometryColor &pc = c.first;
	  const FilterExposurePlan &fep = c.second;
	  if (filter_color[filter_index] == pc) {
	    matched = true;
	    filter_locked[filter_index] = true;
	    main_exposure_time[filter_index] = fep.eTime;
	    number_exposures[filter_index] = fep.eQuantity;
	    exposure_plan[filter_index] = fep;
	    break;
	  }
	}
	if (not matched) any_missing = true;
      }
    }
    fprintf(stderr, "epl.exposure_plan_valid = %s, any_missing = %s\n",
	    (epl.exposure_plan_valid ? "true" : "false"),
	    (any_missing ? "true" : "false"));
    session->log(LOG_INFO, "epl.exposure_plan_valid = %s, any_missing = %s\n",
		 (epl.exposure_plan_valid ? "true" : "false"),
		 (any_missing ? "true" : "false"));
  }

  if (auto_sequence or any_missing) {
    fprintf(stderr, "Using legacy exposure algorithm.\n");
    session->log(LOG_INFO, "Using legacy exposure algorithm.\n");
    
    double finder_mag =
      magnitude_from_image(finder_imagename,
			   session->dark_name(finder_exposure_time, 1, false),
			   object_name,
			   object_name);
    fprintf(stderr, "extracted V mag is %.1lf\n", finder_mag);
    session->log(LOG_INFO, "Extracted V mag is %.1lf", finder_mag);
    
    if (!isnormal(finder_mag)) {
      fprintf(stderr, "...so using V mag of 15.1\n");
      finder_mag = 15.1;
    }
    for (int filter_index = 0; filter_index < number_filters; filter_index++) {
      if (!isnormal(finder_mag)) continue;
      if (filter_locked[filter_index]) continue;
      char filter_letter = this->filter_letter[filter_index][0];
      double predicted_mag = obs_record->PredictBrightness(object_name,
							   filter_letter,
							   finder_mag);
      fprintf(stderr, "    predicted mag for %c is %.1lf\n",
	      filter_letter, predicted_mag);
      session->log(LOG_INFO, "    Predicted mag for %c is %.1lf",
		   filter_letter, predicted_mag);
		   
      if (!isnormal(finder_mag)) continue;

      switch(filter_letter) {
	// A good set of darks for this strategy is:
	//  12 x 6sec
	//  10 x 10sec
	//  4 x 30sec
	//  4 x 60sec
	//  4 x 120sec

	// VALUES FOR QHY268M
      case 'V':
      case 'B':
	if (predicted_mag < 9.0) {
	  // BRIGHTER THAN MAG 9 (V,B)
	  main_exposure_time[filter_index] = 9.0;
	  number_exposures[filter_index] = 10;
	  exposure_plan[filter_index] = { 9.0, // time
					  10,   // quantity
					  0,  // gain
					  3,   // mode
					  5 }; // offset
	} else if(predicted_mag < 12.0) {
	  // BETWEEN MAG 9 and 12 (V,B)
	  main_exposure_time[filter_index] = 10.0;
	  number_exposures[filter_index] = 6;
	  exposure_plan[filter_index] = { 10.0, // time
					  6,   // quantity
					  56,  // gain
					  1,   // mode
					  5 }; // offset
	  
	} else if (predicted_mag < 15.0) {
	  // BETWEEN MAG 12 and 15 (V,B)
	  main_exposure_time[filter_index] = 30.0;
	  number_exposures[filter_index] = 6;
	  exposure_plan[filter_index] = { 30.0, // time
					  6,   // quantity
					  56,  // gain
					  1,   // mode
					  5 }; // offset
	  
	} else {
	  // FAINTER THAN MAG 15 (V,B)
	  main_exposure_time[filter_index] = 60.0;
	  number_exposures[filter_index] = 5;
	  exposure_plan[filter_index] = { 60.0, // time
					  5,   // quantity
					  56,  // gain
					  1,   // mode
					  5 }; // offset
	  
	}
	break;

      case 'R':
	if (predicted_mag < 7.0) {
	  main_exposure_time[filter_index] = 9.0;
	  number_exposures[filter_index] = 10;
	  exposure_plan[filter_index] = { 9.0, // time
					  10,   // quantity
					  0,  // gain
					  3,   // mode
					  5 }; // offset
	} else if(predicted_mag < 8.5) {
	  main_exposure_time[filter_index] = 10.0;
	  number_exposures[filter_index] = 10;
	  exposure_plan[filter_index] = { 10.0, // time
					  10,   // quantity
					  56,  // gain
					  1,   // mode
					  5 }; // offset
	} else {
	  main_exposure_time[filter_index] = 30.0;
	  number_exposures[filter_index] = 4;
	  exposure_plan[filter_index] = { 30.0, // time
					  4,   // quantity
					  56,  // gain
					  1,   // mode
					  5 }; // offset
	  
	}
	break;

      case 'I':
	if (predicted_mag < 5.9) {
	  main_exposure_time[filter_index] = 10.0;
	  number_exposures[filter_index] = 12;
	  exposure_plan[filter_index] = { 10.0, // time
					  12,   // quantity
					  56,  // gain
					  1,   // mode
					  5 }; // offset
	} else {
	  main_exposure_time[filter_index] = 30.0;
	  number_exposures[filter_index] = 4;
	  exposure_plan[filter_index] = { 30.0, // time
					  4,   // quantity
					  56,  // gain
					  1,   // mode
					  5 }; // offset
	}
	break;

      default:
	fprintf(stderr, "strategy.cc: invalid color letter: '%c'\n",
		filter_letter);
      }

#if 0 // ST-9
      case 'V':
	if (predicted_mag < 9.0) {
	  main_exposure_time[filter_index] = 10.0;
	  number_exposures[filter_index] = 5;
	} else if(predicted_mag < 14.0) {
	  main_exposure_time[filter_index] = 30.0;
	  number_exposures[filter_index] = 4;
	} else if (predicted_mag < 15.0) {
	  main_exposure_time[filter_index] = 60.0;
	  number_exposures[filter_index] = 4;
	} else {
	  main_exposure_time[filter_index] = 120.0;
	  number_exposures[filter_index] = 4;
	}
	break;

      case 'B':
	if (predicted_mag < 13.0) {
	  main_exposure_time[filter_index] = 60.0;
	  number_exposures[filter_index] = 4;
	} else if(predicted_mag < 15.5) {
	  main_exposure_time[filter_index] = 120.0;
	  number_exposures[filter_index] = 3;
	} else {
	  main_exposure_time[filter_index] = 120.0;
	  number_exposures[filter_index] = 4;
	}
	break;

      case 'R':
	if (predicted_mag < 7.0) {
	  main_exposure_time[filter_index] = 6.0;
	  number_exposures[filter_index] = 10;
	} else if(predicted_mag < 8.5) {
	  main_exposure_time[filter_index] = 10.0;
	  number_exposures[filter_index] = 10;
	} else {
	  main_exposure_time[filter_index] = 30.0;
	  number_exposures[filter_index] = 3;
	}
	break;

      case 'I':
	if (predicted_mag < 5.9) {
	  main_exposure_time[filter_index] = 6.0;
	  number_exposures[filter_index] = 12;
	} else if(predicted_mag < 7.5) {
	  main_exposure_time[filter_index] = 10.0;
	  number_exposures[filter_index] = 10;
	} else {
	  main_exposure_time[filter_index] = 30.0;
	  number_exposures[filter_index] = 4;
	}
	break;

      default:
	fprintf(stderr, "strategy.cc: invalid color letter: '%c'\n",
		filter_letter);
      }
#endif
    } // end loop over all filters
  }

  for (int filter_index = 0; filter_index < number_filters; filter_index++) {
    char filter_letter = this->filter_letter[filter_index][0];
    const int num_exposures = number_exposures[filter_index];
    const double exposure_time = main_exposure_time[filter_index];

    session->log(LOG_INFO, "%s (%c) exposure plan set to %d x %.0lf",
		 object_name, filter_letter, num_exposures, exposure_time);
  }

  JULIAN start_time(time(0));
  // filelist holds all the exposures for this star (all filters)
  SpreadSheet_Filelist fileList;

  DB_Measurement measurement(*session->astro_db, object_name);

  for (int filter_index = 0; filter_index < number_filters; filter_index++) {
    const int num_exposures = number_exposures[filter_index];
    const double exposure_time = main_exposure_time[filter_index];
    const std::string filter_string(filter_name[filter_index]);
    
    session->log(LOG_INFO, "Starting strategy for filter %s",
		 this->filter_letter[filter_index]);
    focus_check(session, filter_string , false); // allow_slew = false

    // Verify we have a dark for the main exposure. It'd be nice to have
    // 5 times as many darks as we have exposure time (to minimize noise
    // from the darks themselves), but we don't want to spend all night
    // making dark exposures.  We pick a compromise.
    if (session->GetOptions()->use_work_queue == false) {
      int dark_quantity = 5;	// default minimum
      if(dark_quantity < num_exposures) dark_quantity = num_exposures;
      JULIAN dark_start(time(0));
      session->verify_dark_available(exposure_time, dark_quantity);
      JULIAN dark_end(time(0));

      // This is the amount of time in seconds that we weren't actually
      // doing something useful for this strategy.
      non_strategy_time_secs += (dark_end.day() - dark_start.day())*24.0*3600.0;

      // Again, tweak the focus in case it took a while to make a bunch
      // of dark exposures

      // 9/11/2007: disabled to replace with fuzzy focus management
      // after "normal" exposures

      // session->check_focus_using_temp();
    }

    int exposure_number;
    char **exposure_names = new char * [num_exposures];
    int total_filename_chars = 0;
    exposure_flags main_flags("photometry");

    // select the desired filter
    Filter current_filter(filter_name[filter_index]);
    main_flags.SetFilter(current_filter);

    for(exposure_number = 0;
	exposure_number < num_exposures;
	exposure_number++) {
      // double x_blur = -1.0;
      // double y_blur = -1.0;

      // Before the exposure, check and see if any tweaks are needed to
      // the pointing of the scope
      // CheckScopePointing(target_location, main_exposure_time);

      if (ReceiveMessage("simple_session", &message_id)) {
	session->log(LOG_INFO, "Received notification message. Quitting strategy.");
	return PERFORM_SESSION_SHUTDOWN;
      }

      // make many "main" exposures (args are bottom, top, left, right)
      char *image_filename = expose_image_next(exposure_time, main_flags, "PHOTOMETRY");

      fileList.Add_Filename(image_filename);
      session->log(LOG_INFO, "Exposure %d for %s (%s): %.1f secs: %s",
		   exposure_number+1,
		   object_name, this->filter_letter[filter_index],
		   exposure_time, image_filename);
      fprintf(stderr, "Exposure %d: %s\n", exposure_number+1, image_filename);
      total_filename_chars += (4 + strlen(image_filename));
      exposure_names[exposure_number] = strdup(image_filename);

      // add OBJECT to FITS file
      {
	ImageInfo info(image_filename);
	info.SetObject(object_name);
	info.WriteFITS();

	Image main_image(image_filename);
	AddImageToExposurePlanner(main_image, image_filename);
	
	double airmass {0.0};
	double midpoint_jd {0.0};
	if (info.AirmassValid()) {
	  airmass = info.GetAirmass();
	}
	if (info.ExposureMidpointValid()) {
	  midpoint_jd = info.GetExposureMidpoint().day();
	}
	measurement.AddExposure(image_filename, filter_name[filter_index],
				midpoint_jd,
				exposure_time,
				airmass,
				this->ObjectChart(),
				true, // needs_dark
				true); // needs_flat
      }
    }
    // as of 11/2023, the keyword STACK is no longer used in
    // strategies, so stack_exposures is always false
    if(stack_exposures) {
      char output_name[68];
      sprintf(output_name, "%s%s%sXXXXXX", session->Session_Directory(),
	      object_name, this->filter_letter[filter_index]);
      {
	int fd_unused = mkstemp(output_name);	// pick an output filename
	if(fd_unused == -1) {
	  perror("Error trying to create temp file for stack command.");
	} else {
	  const unsigned int command_len = total_filename_chars +
	    strlen(COMMAND_DIR) +
	    strlen(output_name) +
	    strlen(session->dark_name(exposure_time, num_exposures,
				      session->GetOptions()->use_work_queue)) +
	    strlen(session->flat_filename(&current_filter)) +
	    64;
	  char *command_buffer = (char *) malloc(command_len);
	  sprintf(command_buffer, "%s/stack -o %s -d %s -s %s ",
		  COMMAND_DIR,
		  output_name,
		  session->dark_name(exposure_time, num_exposures, false),
		  session->flat_filename(&current_filter));
	  if (strlen(command_buffer) > command_len) {
	    fprintf(stderr,
		    "strategy.cc: ERROR: command_buf overflow: %ld vs %d\n",
		    strlen(command_buffer), command_len);
	    assert(command_len > strlen(command_buffer));
	  }

	  for(int j=0; j<num_exposures; j++) {
	    strcat(command_buffer, exposure_names[j]);
	    strcat(command_buffer, " ");
	  }

	  session->log(LOG_INFO,
		       "Submitting stack command in background to create %s",
		       output_name);
	  if (session->GetOptions()->use_work_queue) {
	    session->SubmitWorkTask(std::string(command_buffer));
	  } else {
	    session->RunTaskInBackground(command_buffer);
	  }
	  free(command_buffer);
	}
      }
    }

    const unsigned int command_buffer_len = 256 + num_exposures *
      (strlen(session->Session_Directory()) + 25);
    char *command_buffer = (char *) malloc(command_buffer_len);
  
    if(!command_buffer) {
      perror("strategy cannot allocate memory for analysis command");
    } else {
      sprintf(command_buffer, COMMAND_DIR "/full_script -f %c -o %s%s%s.phot -n %s -s %s -d %s ",
	      this->filter_name[filter_index][0],
	      session->Session_Directory(),
	      object_name,
	      this->filter_letter[filter_index],
	      object_name,
	      session->flat_filename(&current_filter),
	      session->dark_name(exposure_time, num_exposures,
				 session->GetOptions()->use_work_queue));

      for(int j=0; j<num_exposures; j++) {
	strcat(command_buffer, exposure_names[j]);
	strcat(command_buffer, " ");
      }
      char redirect_buffer[80];
      sprintf(redirect_buffer, " > %s%s%s.out 2>&1",
	      session->Session_Directory(), object_name, this->filter_letter[filter_index]);
      strcat (command_buffer, redirect_buffer);

      // now verify that we didn't overrun the command_buffer
      if (strlen(command_buffer) > command_buffer_len) {
	fprintf(stderr, "Fatal malloc error in strategy.cc: command_buffer[] overrun.\n");
	abort();
      }

      if (session->GetOptions()->use_work_queue) {
	session->log(LOG_INFO,
		     "Submitting analysis command to work_queue: %s",
		     object_name);
	session->SubmitWorkTask(std::string(command_buffer));
      } else {
	session->log(LOG_INFO,
		     "Submitting analysis command in background for %s",
		     object_name);
	session->RunTaskInBackground(command_buffer);
      }
      free(command_buffer);
    }

    // release memory we grabbed earlier
    for(int n=0; n<num_exposures; n++) {
      free(exposure_names[n]);
    }
    delete [] exposure_names;
  } // end loop over all filters

  juid_t measurement_juid = measurement.Close(true /*stack_exposures*/);

  juid_t target_juid = session->astro_db->CreateNewTarget(object_name);
  session->astro_db->AddJUIDToTarget(target_juid, measurement_juid);
  juid_t lpv_target = session->astro_db->CreateNewTarget("lpv");
  session->astro_db->AddJUIDToTarget(lpv_target, target_juid);

  session->log(LOG_INFO, "Done with %s\n", object_name);
  JULIAN end_time(time(0));
  JULIAN obs_time(start_time.day() + (end_time - start_time)/2.0);

  Add_Spreadsheet_Entry(object_name, designation, &fileList, obs_time);

  // Add spreadsheet entries for all the children of this strategy
  {
    StrategyList *children = ChildStrategies();
    if(children) {
      int num_children = children->NumberStrategies();
      int j;

      for(j=0; j<num_children; j++) {
	Strategy *g = children->Get(j);
	Add_Spreadsheet_Entry(g->object_name,
			      g->designation,
			      &fileList,
			      obs_time);
      }
    }
  }

  session->log(LOG_INFO, "%s observation time = %.4f",
	       object_name, obs_time.day());
  session->log(LOG_INFO, "Reference (comp) star for %s = %s\n",
	       object_name, ReferenceStar());
  session->SessionPrintStatus();

  // Save this observation for next session
  HGSCIterator it(catalog);
  const double execution_time = (end_time.day() - strategy_start_time.day())
	*24.0*3600.0 - non_strategy_time_secs;
  for (auto one_star = it.First(); one_star; one_star = it.Next()) {
    if (one_star->do_submit) {
      ObsRecord::Observation *obs = new ObsRecord::Observation;
      obs->empty_record = false;
      obs->when = JULIAN(time(0));
      obs->starname = strdup(one_star->label);
      obs->what = this;
      obs->execution_time = execution_time;

      obs_record->RememberObservation(*obs);
    }
  }
  obs_record->Save(); // write everything

  return OKAY;
}

// return 1 if object is in visible part of the sky; return 0 if
// below observing horizon
//visibility table:
#if 0
// This first table is the horizon map built for the original
// observatory position next to the fence with the LX200 in it on a
// tripod. 
static double vis_table[][2] = { { -180.0, 45.0 },
			  { -131.0, 5.0 }, // ???
			  { -97.0, 5.0 },
			  { -89.0, 5.5 },
			  { -76.0, 9.0 },
			  { -60.0, 18.0 },
			  { -39.0, 23.0 },
			  { -17.0, 24.0 },
			  { -3.0, 24.0 },
			  { 2.0, 11.0 },
			  { 6.0, 11.0 },
			  { 8.0, 18.0 },
			  { 14.0, 24.0 },
			  { 19.0, 24.0 },
			  { 25.0, 20.0 },
			  { 33.0, 22.0 },
			  { 39.0, 18.0 },
			  { 44.0, 23.0 },
			  { 56.0, 29.0 },
			  { 66.0, 28.0 },
			  { 71.0, 19.0 },
			  { 77.0, 19.0 },
			  { 86.0, 19.0 },
			  { 91.0, 23.0 },
			  { 102.0, 23.0 },
			  { 106.0, 19.0 },
			  { 110.0, 20.0 },
			  { 117.0, 25.0 },
			  { 129.0, 22.0 },
			  { 136.0, 21.0 },
			  { 145.0, 33.0 },
			  { 160.0, 34.0 },
			  { 170.0, 34.0 },
			  { 171.0, 45.0 },
			  { 180.0, 45.0 }};

// This second table is for the MI-250 in the larger observatory a
// full 3 feet from the property line prior to any renovation of the
// house.
// Note that +-180 is North and 0 is South, with angles increasing
// clockwise.
static double vis_table[][2] = { { -180.0, 35.0 }, // North
			  { -174.2, 27.7 },
			  { -168.9, 12.0 },
			  { -165.8, 3.0 },
			  {  -97.0, 3.0 },
			  {  -92.5, 4.8 },
			  {  -83.7, 8.4 },
			  {  -74.8, 12.3 },
			  {  -66.2, 17.9 },
			  {  -57.1, 20.4 },
			  {  -37.9, 22.7 },
			  {  -20.4, 25.9 },
			  {   -9.5, 18.6 },
			  {   -2.9, 13.4 },
			  {    4.1, 13.3 },
			  {    7.7, 9.0 },
			  {    9.5, 18.7 },
			  {   15.9, 23.1 },
			  {   25.1, 19.6 },
			  {   34.5, 17.8 }, // roof (garage)
			  {   53.5, 18.5 },
			  {   68.3, 19.7 },
			  {   98.4, 18.8 },
			  {  114.3, 20.2 },
			  {  122.5, 25.2 },
			  {  132.1, 23.2 },
			  {  139.6, 19.5 },
			  {  146.2, 22.4 },
			  {  156.7, 34.5 },
			  {  172.1, 34.9 },
			  {  180.0, 35.0 }};
			  
#endif

int
Strategy::IsVisible(JULIAN when) const {
  ALT_AZ alt_az(object_location, when);

  return ::IsVisible(alt_az, when);
}

//****************************************************************
//        Deal with bad pixels
//****************************************************************
int
Strategy::DoFinder(Session *session, DEC_RA &target_location) {  
  Finder finder(this, session);
  //finder.SetBadPixelAvoidance(true);
  FinderResult result = finder.Execute();
  finder_imagename = strdup(finder.final_imagename());
  return result;
}
      
void
Strategy::FindAllStrategies(Session *session) {
  int num_found = 0;
  DIR *strat_dir = opendir(strategy_directory);
  if(!strat_dir) {
    fprintf(stderr, "strategy: FindAllStrategies: cannot opendir() in %s\n",
	    strategy_directory);
    return;
  }

  struct dirent *dp;
  while((dp = readdir(strat_dir)) != NULL) {
    int len = strlen(dp->d_name);
    if(len >= 4 &&
       (strcmp(dp->d_name + (len - 4), ".str") == 0)) {
      // got one!
      char strategy_name[64];
      strcpy(strategy_name, dp->d_name);
      strategy_name[len-4] = 0;

      if(session)
	session->log(LOG_INFO, "Found strategy for %s", strategy_name);

      Strategy *new_strategy = new Strategy(strategy_name, session);
      if(!new_strategy) {
	fprintf(stderr,
		"Unknown error reading strategy %s in FindAllStrategies\n",
		strategy_name);
      } else {
	num_found++;
	all_strategies.push_back(new_strategy);
      }
    }
  }

  fprintf(stderr, "FindAllStrategies() found %d strategies.\n", num_found);
  closedir(strat_dir);
  FixAllCrosslinks();

  if(session) {
    // now get recent observation dates
    obs_record = new ObsRecord;

    for (Strategy *strategy : all_strategies) {
      ObsRecord::Observation *obs =
	obs_record->LastObservation(strategy->object_name);

      if (obs && !obs->empty_record) {
	strategy->last_observation = obs->when;
	strategy->last_execution_duration = obs->execution_time;
      } else {
	strategy->last_observation = JULIAN(0.0);
	strategy->last_execution_duration = NAN;
      }
    }
  }
}

Strategy *
Strategy::FindStrategy(const char *name) {

  for (Strategy *strategy : all_strategies) {
    if(strcmp(name, strategy->object_name) == 0) {
      return strategy;
    }
  }

  return 0;
}
  
StrategyList::StrategyList(void) {
  array_size = strategy_count = 0;
  main_list = 0;
}

StrategyList::~StrategyList(void) {
  if(main_list) delete [] main_list;
}

// strategies in a StrategyList are referenced by index number. It
// works the same way array indices work (0 to (strategy_count-1)). 
Strategy *
StrategyList::Get(int i) {
  if(i < 0 || i >= strategy_count) return 0;

  return main_list[i];
}

// Add a strategy to a StrategyList.
void
StrategyList::Add(Strategy *s) {
  if(strategy_count == array_size) {
    // not enough room to squeeze another entry, so need to expand the
    // array.
    static const int ARRAY_SIZE_INCREMENT = 12;
    // make a new array
    Strategy **new_list = new Strategy * [array_size + ARRAY_SIZE_INCREMENT];

    // copy the old array into the new one
    int j;
    for(j=0; j<array_size; j++) {
      new_list[j] = main_list[j];
    }

    // array grows, delete the old array
    array_size += ARRAY_SIZE_INCREMENT;
    if(main_list) delete [] main_list;
    main_list = new_list;
  } // end if we had to grow the array

  // add this strategy into the array
  main_list[strategy_count++] = s;
}

void
Strategy::RebuildStrategyDatabase(void) {
  ClearStrategyDatabase();
  initialize_validation_file(AAVSO_VALIDATION_DIR);

  for (Strategy *strategy : all_strategies) {
    if(strcmp("9999+99", strategy->Designation()) != 0)
      validate_star(strategy->Designation(),
		    strategy->ReportName(),
		    0); // don't suppress messages

    AddStrategyToDatabase(strategy, "");
  }

  // Now read the catalog files
  for (Strategy *strategy : all_strategies) {
    char cat_filename[120];
    sprintf(cat_filename, "%s/%s", CATALOG_DIR, strategy->object());
    FILE *fp = fopen(cat_filename, "r");

    // silently ignore strategies that don't have catalog files
    if(fp) {
      HGSCList cat_list(fp);
      HGSCIterator iter(cat_list);
      HGSC *star;

      for(star = iter.First(); star; star = iter.Next()) {
	if(star->A_unique_ID && *star->A_unique_ID) {
	  // found a star with an AUID
	  // Is this star already in the database?
	  StrategyDatabaseEntry *entry = LookupByLocalName(star->label);
	  if(!entry) {
	    // Nope, craft a new entry from scratch
	    entry = CreateBlankEntryInDatabase();
	    entry->local_name = strdup(star->label);
	    entry->strategy_filename = "";
	    entry->designation = "";
	    entry->chartname = "";
	    entry->reporting_name = "";
	  }
	  strcpy(entry->AAVSO_UID, star->A_unique_ID);
	  if(star->report_ID) entry->reporting_name = strdup(star->report_ID);
	}
      }
      fclose(fp);
    }
  }
  SaveStrategyDatabase();
}
			  
bool
Strategy::ReadStrategyFile(const char *filename, Session *session, const char *object_name) {
  bool strategy_error = false;
  
  FILE *fp = fopen(filename, "r");
  if(!fp) {
    fprintf(stderr, "Strategy: strategy file '%s' not found.\n",
	    filename);
  } else {
    char orig_line[132];
    char buffer[132];

    while(fgets(orig_line, sizeof(orig_line), fp)) {
      strcpy(buffer, orig_line);

      // delete comments
      char *s;

      for(s=buffer; *s; s++)
	if(*s == '#') {
	  *s = 0;
	  break;
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

      // ignore blank lines
      if(buffer[0] == 0) continue;

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

      // DESIGNATION
      if(strcmp(keyword, "DESIGNATION") == 0) {
	if(!value) {
	  fprintf(stderr,
		  "%s strategy file: no designation provided\n",
		  object_name);
	  strategy_error = true;
	} else {
	  strcpy(designation, value);
	}

	// CHART
      } else if(strcmp(keyword, "CHART") == 0) {
	if(!value) {
	  fprintf(stderr,
		  "%s strategy file: no chart name\n", object_name);
	  strategy_error = true;
	} else {
	  strcpy(chart, value);
	}

      } else if(strcmp(keyword, "AUTOPHOTUPDATE") == 0) {
	if(!value) {
	  fprintf(stderr,
		  "%s strategy file: no bool value for AUTOPHOTUPDATE\n",
		  object_name);
	  strategy_error = true;
	} else {
	  if (strcmp(value, "TRUE") == 0) {
	    phot_auto_update = true;
	  } else if (strcmp(value, "FALSE") == 0) {
	    phot_auto_update = false;
	  } else {
	    fprintf(stderr, "%s strategy file: AUTOPHOTUPDATE value invalid: %s\n",
		    object_name, value);
	    strategy_error = true;
	  }
	}

      } else if(strcmp(keyword, "STANDARD_FIELD") == 0) {
	if(!value) {
	  fprintf(stderr,
		  "%s strategy file: no bool value for STANDARD_FIELD\n",
		  object_name);
	  strategy_error = true;
	} else {
	  if (strcmp(value, "TRUE") == 0) {
	    is_standard_field = true;
	  } else if (strcmp(value, "FALSE") == 0) {
	    is_standard_field = false;
	  } else {
	    fprintf(stderr, "%s strategy file: STANDARD_FIELD value invalid: %s\n",
		    object_name, value);
	    strategy_error = true;
	  }
	}

      } else if(strcmp(keyword, "USE_HISTORICAL_PLANNING_TIME") == 0) {
	if(!value) {
	  fprintf(stderr,
		  "%s strategy file: no bool value for USE_HISTORICAL_PLANNING_TIME\n",
		  object_name);
	  strategy_error = true;
	} else {
	  if (strcmp(value, "TRUE") == 0) {
	    use_historical_planning_time = true;
	  } else if (strcmp(value, "FALSE") == 0) {
	    use_historical_planning_time = false;
	  } else {
	    fprintf(stderr, "%s strategy file: USE_HISTORICAL_PLANNING_TIME value invalid: %s\n",
		    object_name, value);
	    strategy_error = true;
	  }
	}

      } else if(strcmp(keyword, "AUTOSEQUENCE") == 0) {
	if(!value) {
	  fprintf(stderr,
		  "%s strategy file: no bool value for AUTOSEQUENCE\n",
		  object_name);
	  strategy_error = true;
	} else {
	  if (strcmp(value, "TRUE") == 0) {
	    auto_sequence = true;
	  } else if (strcmp(value, "FALSE") == 0) {
	    auto_sequence = false;
	  } else {
	    fprintf(stderr,
		    "%s strategy file: AUTOSEQUENCE value invalid: %s\n",
		    object_name, value);
	    strategy_error = true;
	  }
	}

	// PRIORITY
      } else if(strcmp(keyword, "PRIORITY") == 0) {
	if (!value) {
	  fprintf(stderr,
		  "%s strategy file: no priority value\n", object_name);
	  strategy_error = true;
	} else {
	  char *end_char;
	  priority = strtod(value, &end_char);
	  if(*end_char != 0) {
	    fprintf(stderr,
		    "%s strategy file: garbage after PRIORITY number\n",
		    object_name);
	    strategy_error = true;
	  }
	}

	// PLANNING_TIME
      } else if(strcmp(keyword, "PLANNING_TIME") == 0) {
	if (!value) {
	  fprintf(stderr,
		  "%s strategy file: no planning_time value\n", object_name);
	  strategy_error = true;
	} else {
	  char *end_char;
	  planning_time = strtod(value, &end_char);
	  if(*end_char != 0) {
	    fprintf(stderr,
		    "%s strategy file: garbage after PLANNING_TIME number\n",
		    object_name);
	    strategy_error = true;
	  }
	}

	// SECONDARY_ECLIPSE_OFFSET (measured in days from primary eclipse)
      } else if (strcmp(keyword, "SECONDARY_ECLIPSE_OFFSET") == 0) {
	if (!value) {
	  fprintf(stderr,
		  "%s strategy file: no secondary offset value\n", object_name);
	  strategy_error = true;
	} else {
	  char *end_char;
	  secondary_offset = strtod(value, &end_char);
	  if (*end_char != 0) {
	    fprintf(stderr,
		    "%s strategy file: garbage after SECONDARY_ECLIPSE_OFFSET number\n",
		    object_name);
	    strategy_error = true;
	  }
	}

	// ECLIPSE_LENGTH (measured in days)
      } else if (strcmp(keyword, "ECLIPSE_LENGTH") == 0) {
	if (!value) {
	  fprintf(stderr,
		  "%s strategy file: no ECLIPSE_LENGTH value\n", object_name);
	  strategy_error = true;
	} else {
	  char *end_char;
	  event_length = strtod(value, &end_char);
	  if (*end_char != 0) {
	    fprintf(stderr,
		    "%s strategy file: garbage after ECLIPSE_LENGTH number\n",
		    object_name);
	    strategy_error = true;
	  }
	}

	// EPHEMERIS
      } else if (strcmp(keyword, "EPHEMERIS") == 0) {
	// typical: "EPHEMERIS=2458906.23456+0.423567"
	if (!value) {
	  fprintf(stderr, "%s strategy file: no ephemeris value\n", object_name);
	  strategy_error = true;
	} else {
	  char *plus_sign = strchr(value, '+');
	  if (plus_sign == nullptr) {
	    fprintf(stderr, "%s strategy file: no + in ephemeris\n", object_name);
	  } else {
	    *plus_sign = '\0';
	    char *end_number;
	    double ref_jd = strtod(value, &end_number);
	    if (end_number != plus_sign) {
	      fprintf(stderr, "%s strategy file: garbage after JD in ephemeris\n",
		      object_name);
	    }
	    ephemeris_ref = JULIAN(ref_jd);
	    ephemeris_period = strtod(plus_sign+1, &end_number);
	    if (*end_number != '\0') {
	      fprintf(stderr,
		      "%s strategy file: garbage after period in ephemeris\n",
		      object_name);
	    }
	  }
	}
	// HOLES
      } else if (strcmp(keyword, "HOLES") == 0) {
	// typical: "HOLES=0.1-0.18,0.7-0.9"
	// note: an empty set of holes is perfectly valid
	char empty_string = 0;
	char *next_hole = value;
	  
	while (*next_hole) {
	  char *comma = strchr(next_hole, ',');
	  if (comma != nullptr) {
	    *comma = '\0';
	  }
	  char *minus_sign = strchr(next_hole, '-');
	  if (minus_sign == nullptr) {
	    fprintf(stderr, "%s strategy: missing '-' in a HOLES list\n",
		    object_name);
	    strategy_error = true;
	  } else {
	    *minus_sign = '\0';
	    char *end_number;
	    double hole_start = strtod(next_hole, &end_number);
	    if (end_number != minus_sign) {
	      fprintf(stderr, "%s strategy: garbage after hole start\n",
		      object_name);
	      strategy_error = true;
	    }
	    double hole_end = strtod(minus_sign+1, &end_number);
	    if (*end_number != '\0') {
	      fprintf(stderr, "%s strategy: garbage after hole end\n",
		      object_name);
	      strategy_error = true;
	    }
	    if (hole_end <= hole_start) {
	      fprintf(stderr, "%s strategy: hole end must be after hole start\n",
		      object_name);
	      strategy_error = true;
	    } else {
	      ObservingHole *oh = new ObservingHole;
	      oh->start = hole_start;
	      oh->end = hole_end;
	      hole_list.push_back(oh);
	    }
	  }
	  if (comma == nullptr) {
	    next_hole = &empty_string;
	  } else {
	    next_hole = comma+1;
	  }
	}
	
	// QUICK_EXPOSURE (exposure time for "quick"s)
      } else if(strcmp(keyword, "QUICK_EXPOSURE") == 0) {
	char *end_char;
	quick_exposure_time = strtod(value, &end_char);
	if(*end_char != 0) {
	  fprintf(stderr,
		  "%s strategy file: garbage after QUICK_EXPOSURE time\n",
		  object_name);
	  strategy_error = true;
	}

	// QUICK_SEQUENCE (num images for "quick"s)
      } else if(strcmp(keyword, "QUICK_SEQUENCE") == 0) {
	char *end_char;
	quick_num_exposures = strtol(value, &end_char, 10);
	if(*end_char != 0) {
	  fprintf(stderr,
		  "%s strategy file: garbage after QUICK_SEQUENCE number\n",
		  object_name);
	  strategy_error = true;
	}

	// QUICK_FILTER (filter to use for "quick"s)
      } else if(strcmp(keyword, "QUICK_FILTER") == 0) {
	quick_filter_name = strdup(value);
	
	// AUTOEXPOSURESTARS (list stars to use for exposure computation)
      } else if(strcmp(keyword, "AUTOEXPOSURESTARS") == 0) {
	InterpretExposurePlanString(exposure_reference_stars, std::string(value), object_name);

	// STACK
      } else if(strcmp(keyword, "STACK") == 0) {
	// ignore value
	stack_exposures = 1;
	
	// REPORTNAME
      } else if(strcmp(keyword, "REPORTNAME") == 0) {
	if(!value) {
	  fprintf(stderr, "%s strategy file: no AAVSO report name\n",
		  object_name);
	  strategy_error = true;
	} else {
	  char NameWithSpaces[64];
	  AltValueWithSpaces(orig_line, NameWithSpaces);
	  report_name = strdup(NameWithSpaces);
	}

	// AAVSONAME
      } else if(strcmp(keyword, "AAVSONAME") == 0) {
	if(!value) {
	  fprintf(stderr, "%s strategy file: no AAVSO starname\n",
		  object_name);
	  strategy_error = true;
	} else {
	  AltValueWithSpaces(orig_line, aavso_name);
	}


	// PARENT
      } else if(strcmp(keyword, "PARENT") == 0) {
	if(!value) {
	  fprintf(stderr, "%s strategy file: no parent specified\n",
		  object_name);
	  strategy_error = true;
	} else {
	  struct crosslink *c = new struct crosslink;
	  c->next = FirstCrosslink;
	  FirstCrosslink = c;
	  c->child = this;
	  c->parent_object_name = strdup(value);
	  is_a_child = 1;
	}

	// OBSERVE
      } else if(strcmp(keyword, "OBSERVE") == 0) {
	observe_strings.push_back(string(value));
	
	// SCRIPT
      } else if(strcmp(keyword, "SCRIPT") == 0) {
	// special handling: buffer[] has had all the spaces squeezed
	// out. We return to the original line to pick up stuff
	// without dropping those spaces.
	int new_length;
	if(object_script == 0) new_length = 0;
	else new_length = 6+strlen(object_script);

	new_length += strlen(orig_line);
	char *new_string = (char *) malloc(new_length);
	if(!new_string) {
	  fprintf(stderr, "strategy: malloc failed for SCRIPT\n");
	} else {
	  if(object_script) {
	    strcpy(new_string, object_script);
	    free(object_script);
	  } else {
	    new_string[0] = 0;
	  }
	  object_script = new_string;

	  char *q;
	  for(q = orig_line; *q && *q != '='; q++);
	  if(*q == '=') {
	    q++;
	    while(*q && isspace(*q)) q++;
	    strcat(object_script, q); // this will copy the \n left
				       // in place by fgets()
	  } else {
	    strcat(object_script, "\n");
	  }
	}

	// REMARKS
      } else if(strcmp(keyword, "REMARKS") == 0 ||
		strcmp(keyword, "REPORT_NOTES") == 0) {
	// special handling: buffer[] has had all the spaces squeezed
	// out. We return to the original line to pick up stuff
	// without dropping those spaces.
	char **target;
	int is_remarks = !strcmp(keyword, "REMARKS");
	if(is_remarks) target = &object_remarks;
	else           target = &report_notes;

	int new_length;
	if(*target == 0) new_length = 0;
	else new_length = 6+strlen(*target);

	new_length += strlen(orig_line);
	char *new_string = (char *) malloc(new_length);
	if(!new_string) {
	  fprintf(stderr, "strategy: malloc failed for REMARKS\n");
	} else {
	  if(*target) {
	    strcpy(new_string, *target);
	    free(*target);
	  } else {
	    new_string[0] = 0;
	  }
	  *target = new_string;

	  char *q;
	  for(q = orig_line; *q && *q != '='; q++);
	  if(*q == '=') {
	    q++;
	    while(*q && isspace(*q)) q++;
	    if(is_remarks)
	      strcat(*target, "### ");
	    strcat(*target, q); // this will copy the \n left
	                        // in place by fgets()
	    if(!is_remarks) {
	      (*target)[strlen(*target) - 1] = 0; // kill the '\n'
	    }
	  } else {
	    if(is_remarks)
	      strcat(*target, "### \n");
	  }
	}

	// REFERENCE
      } else if(strcmp(keyword, "REFERENCE") == 0) {
	if(!value) {
	  fprintf(stderr, "%s strategy file: no reference string\n",
		  object_name);
	  strategy_error = true;
	} else {
	  strcpy(reference_star, value);
	}

	// EXPOSURE (main exposure time in seconds)
      } else if(strcmp(keyword, "EXPOSURE") == 0) {
	char *end_char;
	main_exposure_time[0] = strtod(value, &end_char);
	if(*end_char != 0) {
	  fprintf(stderr,
		  "%s strategy file: garbage after EXPOSURE time\n",
		  object_name);
	  strategy_error = true;
	}

	// SEQUENCE (number of main exposures)
      } else if(strcmp(keyword, "SEQUENCE") == 0) {
	char *end_char;
	number_exposures[0] = strtol(value, &end_char, 10);
	if(*end_char != 0) {
	  fprintf(stderr,
		  "%s strategy file: garbage after SEQUENCE number\n",
		  object_name);
	  strategy_error = true;
	}

	// ID_EXPOSURE (duration of finder exposure)
      } else if(strcmp(keyword, "ID_EXPOSURE") == 0) {
	char *end_char;
	finder_exposure_time = strtod(value, &end_char);
	if(*end_char != 0) {
	  fprintf(stderr,
		  "%s strategy file: garbage after ID_EXPOSURE time\n",
		  object_name);
	  strategy_error = true;
	}

	// OFFSET_TOLERANCE (error from OFFSET allowed)
      } else if(strcmp(keyword, "OFFSET_TOLERANCE") == 0) {
	char *end_char;
	double ot_arcmin = strtod(value, &end_char);
	if(*end_char != 0) {
	  fprintf(stderr,
		  "%s strategy file: garbage after OFFSET_TOLERANCE time\n",
		  object_name);
	  strategy_error = true;
	} else {
	  offset_tolerance = (ot_arcmin/60.0)*M_PI/180.0;
	}

	// OFFSET (distance (arc-min) from star to center point)
      } else if(strcmp(keyword, "OFFSET") == 0) {
	const char *second_half;
	double first_value;
	double second_value;
	char first_direction;
	char second_direction;
	int offset_error = 1;	// easiest to assume an error first

	if(!GetOffset(value, &first_value, &first_direction, &second_half)) {
	  fprintf(stderr, "%s: Can't make sense of offset '%s'\n",
		  object_name, value);
	} else {
	  // successfully got first offset.  Look for another. Might
	  // be good, might be missing (only one offset specified --
	  // perfectly legal), or might be bad.  Last two of these
	  // cases looks the same. We tell them apart by looking at
	  // *next to see if anything is left on the line.
	  if(*second_half == 0 || *second_half == '\n') {
	    // there was only one offset provided.
	    if(first_direction == 'N' || first_direction == 'S') {
	      second_direction = 'E';
	    } else {
	      second_direction = 'S';
	    }
	    second_value = 0.0;
	    offset_error = 0;	// not invalid
	  } else {
	    // There are two offsets (or garbage follows the first offset)
	    if(!GetOffset(second_half, &second_value, &second_direction,
			  &second_half)) {
	      fprintf(stderr, "%s: Can't make sense of offset '%s'\n",
		      object_name, second_half);
	    } else {
	      // Ok, we had two valid offset values; does any garbage
	      // follow the second one?
	    if(*second_half != 0 &&
	       *second_half != '\n') {
	      fprintf(stderr, "%s: Garbage follows offset '%s'\n",
		      object_name, second_half);
	    } else {
	      // Only thing that could still be wrong is if the two
	      // offsets are not orthogonal
	      if(((first_direction == 'N' || first_direction == 'S') &&
		  (second_direction == 'N' || second_direction == 'S')) ||
		 ((first_direction == 'E' || first_direction == 'W') &&
		  (second_direction == 'E' || second_direction == 'W'))) {
		fprintf(stderr, "%s: conflicting offsets\n", object_name);
	      } else {
		// everything valid
		offset_error = 0;
		if(first_direction == 'N')
		  offset_n = first_value;
		if(first_direction == 'S')
		  offset_n = -first_value;
		if(first_direction == 'E')
		  offset_e = first_value;
		if(first_direction == 'W')
		  offset_e = -first_value;
		if(second_direction == 'N')
		  offset_n = second_value;
		if(second_direction == 'S')
		  offset_n = -second_value;
		if(second_direction == 'E')
		  offset_e = second_value;
		if(second_direction == 'W')
		  offset_e = -second_value;
	      }
	    }
	    }
	  }
	  strategy_error = offset_error;
	}
      } else if(strcmp(keyword, "INCLUDE") == 0) {
	bool path_error = false;
	char *include_file = 0;
	// INCLUDE
	if (value[0] == '/') {
	  // absolute pathname
	  include_file = strdup(value);
	} else {
	  // relative pathname
	  include_file = (char *) malloc(strlen(STRATEGY_DIR) +
					 strlen(value) +
					 8);
	  if (!include_file) {
	    fprintf(stderr, "strategy: malloc() error: include_file\n");
	    path_error = true;
	  } else {
	    strcpy(include_file, STRATEGY_DIR);
	    strcat(include_file, "/");
	    strcat(include_file, value);
	  }
	}
	if (!path_error) {
	  strategy_error = !ReadStrategyFile(include_file, session, object_name);
	  free(include_file);
	}
	
	// PERIODICITY
      } else if(strcmp(keyword, "PERIODICITY") == 0) {
	if(strcmp(value, "ALWAYS") == 0) {
	  periodicity = PERIODICITY(ALWAYS);
	} else if(strcmp(value, "DAILY") == 0) {
	  periodicity = PERIODICITY(DAILY);
	} else if(strcmp(value, "NEVER") == 0) {
	  periodicity = PERIODICITY(NEVER);
	} else if(strcmp(value, "WEEKLY") == 0) {
	  periodicity = PERIODICITY(WEEKLY);
	} else if(isdigit(*value) || (*value == '.')) {
	  double user_periodicity;
	  char *end_char;
	  user_periodicity = strtod(value, &end_char);
	  if (*end_char != 0) {
	    fprintf(stderr,
		    "%s strategy file: garbage after PERIODICITY number\n",
		    object_name);
	    strategy_error = true;
	    user_periodicity = 7.0;
	  }

	  periodicity = PERIODICITY(user_periodicity);
	} else {
	  fprintf(stderr, "Strategy %s: unrecognized periodicity: %s\n",
		  object_name, value);
	}
      } else {
	fprintf(stderr, "Strategy: invalid keyword: %s for %s\n", 
		keyword, object_name);
	strategy_error = true;
      }
    }
    fclose(fp);
    if (!obs_record) {
      obs_record = new ObsRecord;
    }

    if (use_historical_planning_time) {
      ObsRecord::Observation *obs =
	obs_record->LastObservation(object_name);

      if (obs && !obs->empty_record) {
	last_observation = obs->when;
	last_execution_duration = obs->execution_time;
      } else {
	last_observation = JULIAN(0.0);
	last_execution_duration = NAN;
      }
    }// end if use historical_planning_time
  } // end if strategy file exists

  return !strategy_error;
}
  
void
Strategy::BuildObservingActions(Session *session) {
  for (Strategy *strategy : all_strategies) {
    if (not strategy->IsAChildStrategy()) {
      ObservingAction::Factory(strategy->observe_strings,
			       strategy->action_list,
			       strategy, // strategy
			       session); // session
    } 
  }
  // And create the two pre-defined ObservingActions
  ObservingAction *dark_oa = new ObservingAction(nullptr, // strategy
						 session,
						 AT_Dark);
  dark_oa->SetPriority(1.0);
  dark_oa->SetGroups(std::list<string> { "DARK" });
  dark_oa->SetExecutionTime(1800.0);
  ObservingAction *flat_oa = new ObservingAction(nullptr, // strategy
						 session,
						 AT_Flat);
  flat_oa->SetPriority(1.0);
  flat_oa->SetGroups(std::list<string> { "FLAT" });
  flat_oa->SetExecutionTime(2400.0);
}

static std::string
StripSpaces(std::string s) {
  unsigned int start_pos = 0;
  unsigned int end_pos = 0;

  if (s.size() == 0) return s;
  
  for (start_pos = 0; start_pos < s.size(); start_pos++) {
    if (not isspace(s[start_pos])) break;
  }
  for (end_pos = s.size()-1; end_pos; end_pos--) {
    if (not isspace(s[end_pos])) break;
  }
  return s.substr(start_pos, end_pos - start_pos + 1);
}

void
Strategy::InterpretExposurePlanString(std::list<std::string> &ref_stars,
				      std::string planning_string,
				      const char *object_name) {
  HGSCList catalog(object_name);
  if (not catalog.NameOK()) return;
  
  std::list<std::string> wordlist;
  unsigned int start_pos = 0;
  for (unsigned int i=0; i<planning_string.size(); i++) {
    if (planning_string[i] == ',') {
      wordlist.push_back(planning_string.substr(start_pos, i-start_pos));
      start_pos = i+1;
    }
  }
  const int remainder = planning_string.size() - start_pos;
  if(remainder) {
    wordlist.push_back(planning_string.substr(start_pos, remainder));
  }

  for (auto s : wordlist) {
    std::string star{StripSpaces(s)};

    if (star == "VARIABLE" || star == "COMP" || star == "CHECK") {
      HGSCIterator it(catalog);
      for (auto c=it.First(); c; c=it.Next()) {
	if ((c->do_submit and star=="VARIABLE") ||
	    (c->is_comp and star=="COMP") ||
	    (c->is_check and star=="CHECK")) {
	  ref_stars.push_back(std::string(c->label));
	}
      }
    } else {
      ref_stars.push_back(star);
    }
  }
}
