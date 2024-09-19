/*  ambient.cc -- Provides ambient camera case temperature
 *
 *  Copyright (C) 2021 Mark J. Munkacsy

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

#include <time.h>
#include <errno.h>
#include <stdio.h>
#include "ambient.h"

double last_measurement;
time_t last_meas_time;
bool last_meas_valid = false;

static const char *TEMP_LOG_FILENAME = "/home/mark/ASTRO/LOGS/temperature.log";

bool ReadLogfile(void) {
  FILE *fp = fopen(TEMP_LOG_FILENAME, "r");
  if (!fp) {
    last_meas_valid = false;
    return false;
  }
  
  int status = fseek(fp, -120, SEEK_END);
  if (status) {
    if (errno == EINVAL) {
      rewind(fp);
    } else {
      perror("Error seeking to end of temperature.log:");
      fclose(fp);
      return false;
    }
  }

  bool first_line = true;
  bool any_vals_valid = false;
  
  while (not feof(fp)) {
    unsigned long long date_int;
    double temp;
    bool vals_valid = false;

    if (not first_line) {
      int fields = fscanf(fp, "%Lu %lf", &date_int, &temp);
      vals_valid = (fields == 2 && date_int > 1000000000L && temp > -40.0 && temp < 60.0);
      if (vals_valid) any_vals_valid = true;
    } else {
      first_line = false;
    }

    int c;
    do {
      c = getc(fp);
    } while (c != EOF and c != '\n');

    if (vals_valid) {
      last_measurement = temp;
      last_meas_time = date_int;
    }
  }

  last_meas_valid = any_vals_valid;
  fclose(fp);
  return last_meas_valid;
}

bool UpdateMeasurement(void) {
  if (not last_meas_valid) {
    ReadLogfile();
  }

  if (last_meas_valid) {
    time_t now = time(nullptr);
    if (now - last_meas_time > 11) {
      ReadLogfile();
    }
  }
  
  return last_meas_valid;
}

void AmbientInitialize(void) {
  if (not UpdateMeasurement()) {
    fprintf(stderr, "ambient.cc: unable to fetch ambient temp from logfile.\n");
  }
}

bool AmbientTempAvail(void) {
  return UpdateMeasurement();
}
  
double AmbientCurrentDegC(void) {
  UpdateMeasurement();
  return last_measurement;
}


