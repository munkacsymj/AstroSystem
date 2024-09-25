/*  temp_server.cc -- Server handles temperature measurement
 *
 *  Copyright (C) 2018 Mark J. Munkacsy

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

#include <unistd.h>		// sleep()
#include <stdlib.h>		// system()
#include <stdio.h>		// sprintf()
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>		// open()

const char *logfile_name = "/home/mark/ASTRO/LOGS/temperature.log";

#define MAX_LOGFILE_LENGTH (1024*1024)

void prune_logfile(void) {
  char orig_logfile[MAX_LOGFILE_LENGTH];

  int log_fd = open(logfile_name, O_RDWR);
  if (log_fd < 0) {
    perror("Cannot open logfile for pruning: ");
    return;
  }

  ssize_t n = read(log_fd, orig_logfile, MAX_LOGFILE_LENGTH);
  if (n >= MAX_LOGFILE_LENGTH) {
    fprintf(stderr, "temp_server: logfile is too long to prune.\n");
    return;
  } else if (n < 0) {
    perror("Cannot read logfile for pruning: ");
    return;
  } else {
    // search from the end of the file, counting newlines
    int numlines = 60*24; // prune to hold 24 hours of data
    unsigned int i;
    for (i = (n-1); i; i--) {
      if (orig_logfile[i] == '\n') numlines--;
      if (numlines == 0) break;
    }
    // okay, two possibilities: the file is already short enough (i ==
    // 0) or we found 24 hours worth of data (numlines == 0)
    if (i == 0) {
      // file short enough. Do nothing.
      close(log_fd);
    } else {
      i++; // skip over the newline that put us to 24 hours
      off_t r = lseek(log_fd, 0, SEEK_SET);
      if (r == (off_t) -1) {
	perror("temp_server: cannot seek to start of logfile: ");
      } else {
	ssize_t p = write(log_fd, orig_logfile+i, (n-i));
	if (p != (n-i)) {
	  perror("temp_server: short-write of pruned file: ");
	}
	if (ftruncate(log_fd, (n-i))) {
	  perror("temp_server: error truncating logfile: ");
	}
      }
      close(log_fd);
    }
  }
}

int main(int argc, char **argv) {
  while (1) {
    for (unsigned int min_count = 0; min_count < 60; min_count++) {
      // wait 60 seconds and then append a temperature onto the log
      sleep(60);

      char command[256];
      const char *cmd_start = "temper-poll -c | ( date '+%s ' | tr -d '\n';cat)";
      sprintf(command, "%s >> %s", cmd_start, logfile_name);
      int result = system(command);
    }
    // now perform a "prune" of the logfile
    prune_logfile();
  }
}
