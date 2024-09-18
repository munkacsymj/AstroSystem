/*  obs_record.cc -- maintain database of all observations (just time
 *  of observation) 
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
#include <string.h>		// pick up strcmp()
#include <stddef.h>		// offsetof()
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include "obs_record.h"
#include <gendefs.h>

/* The obs_record database is maintained for one primary purpose: to
 * support the scheduling of sessions.  We find the time of the last
 * observation of any particular star by looking in this
 * database. Then we can calculate how long it has been since the last
 * observation in order to determine how important today's observation
 * is.
 */

// Example lines:
// 2452548.595116 ty-lyr 621.,,12.341,13.100,\n
// 2452548.595116 ty-lyr
// 2452548.595116 ty-lyr 621.,,,,
// 2452548.595116 ty-lyr 621.,13.100,12.100,11.100,10.100\n

#define MAX_FIELDS 8

static char *strdup_lower(const char *s) {
  char *result = (char *) malloc(strlen(s) + 1);
  char *p = result;
  while(*s) {
    *p++ = (isalpha(*s) ? tolower(*s) : *s);
    s++;
  }
  *p = 0;
  return result;
}
      
void
ObsRecord::SyncWithDisk(void) {
  struct stat statbuf;
  if (stat(OBS_RECORD_FILENAME, &statbuf)) {
    perror("Unable to stat() 'observations' file:");
    return;
  }
  if (last_disk_sync.tv_sec == statbuf.st_mtim.tv_sec and
      last_disk_sync.tv_nsec == statbuf.st_mtim.tv_nsec) return; // nothing needed
  last_disk_sync = statbuf.st_mtim;

  all_obs.clear();
  ReadEntireFile();
}

ObsRecord::ObsRecord(void) {
  Obs_Filename = OBS_RECORD_FILENAME;
  last_disk_sync.tv_sec = 0;
  last_disk_sync.tv_nsec = 0;
  SyncWithDisk();
}

void
ObsRecord::ReadEntireFile(void) {
  FILE *fp = fopen(Obs_Filename, "r");

  if(!fp) {
    fprintf(stderr, "Warning: 'observations' file not found.\n");
  } else {
    char buffer[256];
    char *fields[MAX_FIELDS];

    while(fgets(buffer, sizeof(buffer), fp)) {
      int num_fields = 1;
      const char *comment_start = 0;
      char *s;

      for(s=buffer; *s; s++) {
	if (*s == '\n') {
	  *s = 0;
	  break;
	}
      }

      for(s=buffer; *s; s++) {
	if(*s == '#') {
	  *s = 0;
	  comment_start = (s+1);
	  break;
	}
      }

      fields[0] = buffer;
      s = buffer;
      while (*s) {
	if (*s == '\n') {
	  *s = 0;
	  break;
	}
	if (*s == ',') {
	  *s = 0;
	  fields[num_fields++] = s+1;
	}
	s++;
      }

      // Field 1: JULIAN Starname Exec_time

      char star_name[80];
      double obs_date, exec_time;
      int num_read = sscanf(buffer, "%lf %s %lf",
			    &obs_date,
			    star_name,
			    &exec_time);

      if(num_read == 1) {
	fprintf(stderr, "observations: bad input line: %s\n", buffer);
	continue;
      }

      Observation *obs = new Observation;

      if (num_read <= 0) {
	obs->empty_record = true;
      } else {
	obs->empty_record = false;
	obs->starname = strdup_lower(star_name);
	Strategy *strategy = Strategy::FindStrategy(obs->starname);

	obs->when = JULIAN(obs_date);
	obs->what = strategy;
	if(num_read == 3) {
	  obs->execution_time = exec_time;
	} else {
	  obs->execution_time = NAN;
	}

	// MAG_B
	if (num_fields >= 2) {
	  num_read = sscanf(fields[1], "%lf", &obs->B_mag);
	}
	// MAG_V
	if (num_fields >= 3) {
	  num_read = sscanf(fields[2], "%lf", &obs->V_mag);
	}
	// MAG_R
	if (num_fields >= 4) {
	  num_read = sscanf(fields[3], "%lf", &obs->R_mag);
	}
	// MAG_I
	if (num_fields >= 5) {
	  num_read = sscanf(fields[4], "%lf", &obs->I_mag);
	}
	
      }
      if (comment_start) {
	obs->comment_field = strdup(comment_start);
      }
      all_obs.push_back(obs);
    }
    fclose(fp);
  }
}

void
ObsRecord::RememberObservation(Observation &obs) {
  SyncWithDisk();
  Observation *new_obs = new Observation;
  *new_obs = obs;

  all_obs.push_back(new_obs);
}

ObsRecord::Observation *
ObsRecord::LastObservation(const char *name) {
  SyncWithDisk();
  char *lc_name = strdup_lower(name);
  Observation *latest_obs = 0;

  std::list<Observation *>::iterator it;
  for (it = all_obs.begin(); it != all_obs.end(); it++) {
    Observation *obs = (*it);
    
    if((!obs->empty_record) && (strcmp(obs->starname, lc_name) == 0)) {
      if(latest_obs == 0 ||
	 latest_obs->when < obs->when) {
	latest_obs = obs;
      }
    }
  }

  free(lc_name);
  return latest_obs;
}

ObsRecord::Observation *
ObsRecord::FindObservation(const char *name, JULIAN time_of_obs) {
  SyncWithDisk();
  char *lc_name = strdup_lower(name);
  ObsRecord::Observation *answer = nullptr;
  
  std::list<Observation *>::iterator it;
  for (it = all_obs.begin(); it != all_obs.end(); it++) {
    Observation *obs = (*it);

    if ((!obs->empty_record) && (strcmp(obs->starname, lc_name) == 0) &&
	fabs(obs->when.day() - time_of_obs.day()) < (2.0/24.0)) {
      answer = obs;
      break;
    }
  }
  return answer;
}

void
ObsRecord::Save(void) {		// important to call this if you've
				// changed or added any observations
  FILE *fp = fopen(Obs_Filename, "w");

  if(!fp) {
    fprintf(stderr, "Warning: 'observations' file not found.\n");
  } else {
    std::list<Observation *>::iterator it;
    for (it = all_obs.begin(); it != all_obs.end(); it++) {
      Observation *obs = (*it);

      if (!obs->empty_record) {
	fprintf(fp, "%.6lf %s ", obs->when.day(), obs->starname);

	if (isnormal(obs->execution_time)) {
	  fprintf(fp, "%.3lf", obs->execution_time);
	}
	fprintf(fp, ",");

	if (isnormal(obs->B_mag)) {
	  fprintf(fp, "%.3lf", obs->B_mag);
	}
	fprintf(fp, ",");
	  
	if (isnormal(obs->V_mag)) {
	  fprintf(fp, "%.3lf", obs->V_mag);
	}
	fprintf(fp, ",");
	  
	if (isnormal(obs->R_mag)) {
	  fprintf(fp, "%.3lf", obs->R_mag);
	}
	fprintf(fp, ",");
	  
	if (isnormal(obs->I_mag)) {
	  fprintf(fp, "%.3lf", obs->I_mag);
	}
      }

      if (obs->comment_field && *obs->comment_field) {
	fprintf(fp, "#%s", obs->comment_field);
      }
      fprintf(fp, "\n");
    } // end loop over all records
  } // end if fopen() was successful
  fclose(fp);

  struct stat statbuf;
  if (stat(OBS_RECORD_FILENAME, &statbuf)) {
    perror("Unable to stat() 'observations' file:");
    return;
  }
  last_disk_sync = statbuf.st_mtim;
}

//****************************************************************
// PredictBrightness() will return NAN if it can't make a valid
// prediction.
//****************************************************************
double
ObsRecord::PredictBrightness(const char *name,
			     char filter_letter,
			     double v_mag) {
  char *name_lc = strdup_lower(name);
  
  // find all observations for this star
  double sum_x = 0.0;
  double sum_y = 0.0;
  double sum_xx = 0.0;
  double sum_yy = 0.0;
  double sum_xy = 0.0;
  int count = 0;
  
  // If asking to use V to predict V, there's no thinking involved...
  if (filter_letter == 'V') return v_mag;

  size_t delta_addr;
  switch (filter_letter) {
  case 'B':
    delta_addr = offsetof(Observation, B_mag);
    break;
    
  case 'R':
    delta_addr = offsetof(Observation, R_mag);
    break;
    
  case 'I':
    delta_addr = offsetof(Observation, I_mag);
    break;

  default:
    fprintf(stderr, "PredictBrightness: invalid filter_letter: '%c'\n",
	    filter_letter);
    return NAN;
  }

  std::list<Observation *>::iterator it;
  for (it = all_obs.begin(); it != all_obs.end(); it++) {
    Observation *obs = (*it);
    
    if((!obs->empty_record) && (strcmp(obs->starname, name_lc) == 0)) {
      double target_mag = *((double *)(((char *)obs) + delta_addr));
      if (isnormal(obs->V_mag) && isnormal(target_mag)) {
	// fprintf(stderr, "X = %.3lf, Y = %.3lf\n",
	// obs->V_mag, target_mag);
	sum_x += obs->V_mag;
	sum_xx += (obs->V_mag * obs->V_mag);
	sum_y += target_mag;
	sum_yy += (target_mag * target_mag);
	sum_xy += (target_mag * obs->V_mag);
	count++;
      }
    }
  }
  if (count < 2) return NAN;
  double m = (count*sum_xy - sum_x*sum_y)/(count*sum_xx - sum_x*sum_x);
  double b = sum_y/count - m*sum_x/count;

  return m*v_mag + b;
}

ObsRecord::Observation::Observation(void) {
  when = JULIAN(0.0);
  what = 0;
  execution_time = NAN;
  V_mag = NAN;
  B_mag = NAN;
  R_mag = NAN;
  I_mag = NAN;
  comment_field = 0;
  empty_record = true;
}

