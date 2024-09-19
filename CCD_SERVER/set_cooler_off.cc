/*  set_cooler_off.cc -- Invoked by udev rule to keep QHY268M cooler off
 *
 *  Copyright (C) 2022 Mark J. Munkacsy

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
#include <qhyccd.h>
#include <stdio.h>
#include <time.h>


const char *logfilename = "/var/local/udev-qhy.log";
qhyccd_handle *camhandle = nullptr;

int main(int argc, char **argv) {
  FILE *log = fopen(logfilename, "a");

  time_t now;
  time(&now);
  const char *pretty_time = ctime(&now);
  fprintf(log, "%s", pretty_time);

  fprintf(log, "set_cooler_off: invoked with arg %s\n",
	  (argc > 1 ? argv[1] : "<missing>"));

  int ret = InitQHYCCDResource();
  if (ret != QHYCCD_SUCCESS) {
    fprintf(log, "InitQHYCCDResource() failed.\n");
    exit(3);
  }

  int num = ScanQHYCCD();
  if (num == 0) {
    fprintf(log, "No camera found. Give up.\n");
    exit(3);
  }
  if (num > 1) {
    fprintf(log, "Multiple cameras found. Give up.\n");
    exit(3);
  }

  char id[32];
  ret = GetQHYCCDId(0, id);

  camhandle = OpenQHYCCD(id);
  if (camhandle == nullptr) {
    fprintf(stderr, "OpenQHYCCD() failed.\n");
    exit(3);
  }

  double pwm = GetQHYCCDParam(camhandle, CONTROL_CURPWM);
  fprintf(log, "Initial CCD power = %.2lf\n", pwm);
  
  ret = SetQHYCCDParam(camhandle, CONTROL_MANULPWM, 0);
  if (ret != QHYCCD_SUCCESS) {
    fprintf(log, "SetQHYCCDParam(CONTROL_MANULPWM, x) failed.\n");
  } else {
    fprintf(log, "Cooler turned off.\n");
  }
  
  fclose(log);
  return 0;
}
