/*  focus_alg.cc -- use measured bluring of star images to control focus
 *  Copyright (C) 2007, 2015 Mark J. Munkacsy

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

#include "focus_manager.h"
#include <session.h>
#include <scope_api.h>
#include <time.h>
#include <julian.h>
#include <gendefs.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <list>
#include <unordered_map>
#include <string>

// Logfiles: $D/session_focuslog00.txt
//           $D/focus_000.txt (each call to "focus")

JULIAN last_focus_check(0.0);
FILE *session_focus_log = 0;
long session_start_focus = -1;

static const char *clean_gmt(void) {
  time_t clock_data;
  time(&clock_data);
  char *buffer = ctime(&clock_data);
  buffer[24] = 0; //clobber trailing '\n'
  return buffer;
}

void setup_session_focus_log(Session *session) {
  struct stat stat_info;
  char filename[64];
  for (int i = 0; i < 100; i++) {
    sprintf(filename, "%s/session_focuslog%02d.txt",
	    session->Session_Directory(), i);
    if (stat(filename, &stat_info)) {
      // non-zero means failure. This is a good filename
      session_focus_log = fopen(filename, "w");
      if (session_focus_log == 0) {
	perror("focus_manager: error opening session log");
	abort();
      } else {
	return;
      }
    }
  }
  fprintf(stderr, "setup_session_focus_log: too many logfiles???\n");
  return;
}
	       
//****************************************************************
//        FocusOffset class
//****************************************************************
std::unordered_map<std::string, long> offset_lookup;
bool Initialize_Focus_Offset(void) {
  const char *offset_filename = "/home/ASTRO/CURRENT_DATA/focus_offset.txt";
  FILE *fp = fopen(offset_filename, "r");
  if (fp) {
    char buffer[80];
    while (fgets(buffer, sizeof(buffer), fp)) {
      if (buffer[0] == '\n') continue;
      // two words should be present: filter name and offset value
      char *filtername = buffer;
      const char *offset = buffer; // will change later
      while (isspace(*filtername )) filtername++;
      char *s = filtername;
      while(*s and not isspace(*s)) s++;
      if (*s == 0) {
	// no offset, so assume zero
	offset = "0";
      } else {
	*s = 0; // terminate the filtername
	offset = s+1;
	while (isspace(*offset)) offset++;
      }
      long offset_val = strtol(offset, nullptr, 10);
      Filter filter(filtername);
      offset_lookup[std::string(filter.NameOf())] = offset_val;
    }
    fclose(fp);
  }
  return true;
}

long GetFocusOffset(const std::string &filtername) {
  auto lookup = offset_lookup.find(filtername);
  if (lookup == offset_lookup.end()) {
    fprintf(stderr, "GetFocusOffset(): Filter name unrecognized: %s\n",
	    filtername.c_str());
    return 0;
  }
  return lookup->second;
}
      
static bool FocusOffsetInitialized = false; 

struct Measurement {
  double focuser_value;
  JULIAN time_of_measurement; // absolute
  double delta_minutes;
  double weight;
};

JULIAN reference_blur_measurement_time;
std::list<Measurement *> *all_measurements = 0;

struct FocusModel {
  bool   model_empty;
  double ref_focus_measurement;
  JULIAN ref_focus_time;
  double focuser_drift_rate; // ticks per minute
} master_model = { true, 0.0, JULIAN(0.0), 0.0 };

// This is called when the measurements have changed and the model
// hasn't yet been updated. 
void UpdateModel(void) {
  if (all_measurements ==0) {
    fprintf(session_focus_log,
	    "focus_manager: UpdateModel(): called w/no measurement.\n");
  } else if (all_measurements->size() == 1) {
    master_model.model_empty = false;
    master_model.ref_focus_measurement = all_measurements->front()->focuser_value;
    master_model.ref_focus_time = all_measurements->front()->time_of_measurement;
    master_model.focuser_drift_rate = 0.0;
    fprintf(session_focus_log,
	    "Single focus measurement (%.0lf), zero drift.\n",
	    master_model.ref_focus_measurement);
  } else {
    double w_sum = 0.0;
    double sum_x = 0.0;
    double sum_y = 0.0;
    double sum_xy = 0.0;
    double sum_xx = 0.0;

    fprintf(session_focus_log, "--------------\nMeasurements:\n");

    std::list<Measurement *>::iterator it;
    for (it = all_measurements->begin(); it != all_measurements->end(); it++) {
      const Measurement *m = (*it);
      const double w = (m->weight);
      w_sum += w;
      const double x = 	m->delta_minutes;
      const double y = m->focuser_value;

      sum_x += x*w;
      sum_xx += (x*x)*w;
      sum_y += y*w;
      sum_xy += (x*y)*w;
      fprintf(session_focus_log, "    %s (%.1lf mins): %.1lf @w=%.1lf\n",
	      m->time_of_measurement.to_string(),
	      m->delta_minutes,
	      m->focuser_value,
	      m->weight);
    }
    fprintf(session_focus_log, "------------\n");

    const double alpha = (sum_x*sum_y - sum_xy*w_sum)/
      (sum_x*sum_x - sum_xx*w_sum);
    // this intercept is the focuser setting when x == 0; that is, the
    // focuser setting when t == reference_blur_measurement_time
    const double intercept = (sum_xy - alpha*sum_xx)/sum_x;
    master_model.focuser_drift_rate = alpha;
    // we update the intercept point to the value of x when t == time
    // of last measurement
    master_model.ref_focus_time = all_measurements->back()->time_of_measurement;
    master_model.ref_focus_measurement = intercept + alpha * (master_model.ref_focus_time - reference_blur_measurement_time) * 24.0 * 60.0;
  }
  fprintf(session_focus_log, "New model: %.0lf + (%.4lf)*t\n",
	  master_model.ref_focus_measurement, master_model.focuser_drift_rate);
  fflush(session_focus_log);
}

void add_blur_measurement(double measurement) {
  // if the model is uninitialized, then this is the first measurement
  JULIAN now(time(0));
  Measurement *m = new Measurement;

  if (master_model.model_empty) {
    all_measurements = new std::list<Measurement *>;
    reference_blur_measurement_time = now;
    fprintf(session_focus_log, "Setting reference time to %ld\n",
	    (long) now.to_unix());
    m->weight = 1.0;
  } else {
    m->weight = 2.0 * all_measurements->back()->weight;
    // m->weight = 1.0;
  }

  m->focuser_value = measurement;
  m->time_of_measurement = now;
  m->delta_minutes = (now - reference_blur_measurement_time)*24.0*60.0;
  all_measurements->push_back(m);

  UpdateModel();
}

void focus_check(Session *session, const std::string &filtername, bool allow_slew) {
  if (not FocusOffsetInitialized) Initialize_Focus_Offset();
  
  if (session_focus_log == 0) {
    setup_session_focus_log(session);
    session_start_focus = scope_focus(0);
  }

  // do nothing if session checking of focus is turned off
  if (session->FocusCheckMinutes() <= 0.0) return;

  // is it time to do a focus update?
  JULIAN right_now(time(0));
  const double delta_t_mins = (right_now - last_focus_check)*24.0*60.0;
  if ( allow_slew &&
       ((all_measurements == 0) ||
	delta_t_mins > session->FocusCheckMinutes() ||
	(delta_t_mins > 10.0 && all_measurements->size() < 2) ||
	(delta_t_mins > 15.0 && all_measurements->size() < 4))) {
    // yes, time for an update
    session->log(LOG_INFO, "Starting focus check cycle.\n");
    fprintf(session_focus_log, "%s: Starting focus check cycle.\n",
	    clean_gmt());

    // Find a logfile
    char logfilename[128];
    char shellfilename[128];
    {
      struct stat stat_info;
      for (int i=0; i < 1000; i++) {
	sprintf(logfilename, "%s/focus_%03d.log",
		session->Session_Directory(), i);
	if (stat(logfilename, &stat_info)) {
	  // non-zero means failure. This is a good filename
	  sprintf(shellfilename, "%s/focus_%03d.shell",
		  session->Session_Directory(), i);
	  break;
	}
      }
    }
    
    bool use_dash_n_option = 
      session->GetOptions()->trust_focus_star_position;
    char buffer[512];
    double this_blur = -1.0;
    bool focus_valid = false;
    int original_focus = scope_focus(0);
    sprintf(buffer, COMMAND_DIR "/focus -s %ld -a %s -t 0.2 -D %s -p -l %s > %s 2>&1",
	    session_start_focus,
	    (use_dash_n_option ? "-n" : ""),
	    session->Session_Directory(), logfilename, shellfilename);
    session->log(LOG_INFO, buffer);
    int return_value = system(buffer);
    FILE *fp = fopen("/tmp/focus_param.txt", "r");
    if (!fp) {
      fprintf(session_focus_log, "%s: Cannot open /tmp/focus_param.txt\n",
	      clean_gmt());
    } else {
      const int n = fscanf(fp, "Focus = %lf", &this_blur);
      if (n != 1) {
	fprintf(session_focus_log,
		"%s: Invalid blur value from /tmp/focus_param.txt\n",
		clean_gmt());
      } else {
	focus_valid = true;
      }
      fclose(fp);
    }
    fprintf(session_focus_log,
	    "%s: focus command returned %.0lf (with status %d), focus_valid = %s\n",
	    clean_gmt(), this_blur, return_value,
	    (focus_valid ? "true" : "false"));
    if (focus_valid) {
      add_blur_measurement(this_blur);
      last_focus_check = JULIAN(time(0));
    } else {
      scope_focus(original_focus);
    }
  }

  // Extrapolate previous focus model to the current time
  if (master_model.model_empty) {
    fprintf(session_focus_log, "%s: focus: do nothing due to no model yet.\n",
	    clean_gmt());
  } else {
    JULIAN now(time(0));
    const double delta_mins = (now - master_model.ref_focus_time)*24.0*60.0;
    const long extrap = (long) (master_model.ref_focus_measurement + 0.5 +
				master_model.focuser_drift_rate * delta_mins);
    const long offset = GetFocusOffset(filtername);
    unsigned long current_encoder = scope_focus(0);
    unsigned long actual_encoder = scope_focus(extrap + offset - current_encoder);
    fprintf(session_focus_log,
	    "%s: focus: moving focuser to %ld (%ld actual)\n",
	    clean_gmt(), extrap, actual_encoder);
  }
  fflush(session_focus_log);
}
    
	    
