/*  focuser_reset.c -- Manages serial link communications with a JMI SmartFocus box
 *
 *  Copyright (C) 2015 Mark J. Munkacsy

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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <stdlib.h>		/* exit() */
#include <stdio.h>

const char *error_filename = "./focuser_reset.stderr";

void record_error(const char *msg) {
  static FILE *err_fp = 0;
  static bool err_file_open_failed = false;

  if (err_fp == 0 && !err_file_open_failed) {
    err_fp = fopen(error_filename, "w");
    if (!err_fp) {
      fprintf(stderr, "Error: cannot open error log: %s\n",
	      error_filename);
      err_file_open_failed = true;
    } else {
      fprintf(stderr, "Errors encountered: see error log: %s\n",
	      error_filename);
    }
  }
  if (err_fp) {
    fprintf(err_fp, "%s", msg);
    fflush(err_fp);
  }
}
    
int initialized = 0;
int focus_fd;	/* file descriptor for the focuser */

void initialize_jmi(void) {
  struct termios term_struct;

  focus_fd = open("/dev/ttyS0", O_RDWR);
  if(focus_fd < 0) {
    perror("Unable to open serial connection to JMI SmartFocus");
    record_error("Unable to open serial connection to JMI SmartFocus\n");
    return;
  }

  term_struct.c_iflag = (IGNBRK | IGNPAR) ;
  term_struct.c_oflag = 0;
  term_struct.c_cflag = (CS8 | CREAD | CLOCAL);
  term_struct.c_lflag = 0;

  cfsetospeed(&term_struct, B9600);
  cfsetispeed(&term_struct, B9600);
  if(tcsetattr(focus_fd, TCSANOW, &term_struct) != 0) {
    perror("Unable to setup /dev/tty to JMI SmartFocus");
    record_error("Unable to setup /dev/tty to JMI SmartFocus\n");
  }
}

int main(int argc, char **argv) {
  char init_cmd = 'h';
  char read_buffer[8];
  char message[80];
  int loop_count = 10;
  int status;
  int bad_bytes = 0;
  
  fprintf(stderr, "focuser_initialize will reset focuser position in 4 seconds.\n");

  initialize_jmi();

  // flush any existing data (crud) in serial I/O line
  
  loop_count = 4;
  while (loop_count-- >= 0) {
    status = read(focus_fd, read_buffer, 1);
    if (status == 1) {
      bad_bytes++;
      loop_count++;
    }
    sleep(1);
  }
  if (bad_bytes) {
    fprintf(stderr, "%d bad bytes were flushed.\n", bad_bytes);
  }

  loop_count = 10;

  status = write(focus_fd, &init_cmd, 1);
  if (status != 1) {
    perror("Error writing command to focuser:");
    record_error("Error writing command to focuser:");
    exit(-2);
  }

  // Wait up to 10 seconds for read() to return something
  while (loop_count-- >= 0) {
    status = read(focus_fd, read_buffer, 1);
    if (status) break;
    sleep(1);
  };

  if (status != 1 || read_buffer[0] != 'h') {
    sprintf(message, "read() returned %d bytes.\n", status);
    fprintf(stderr, "%s", message);
    record_error(message);
    if (status == 1) {
      sprintf(message, "read() returned '%c'\n", read_buffer[0]);
      fprintf(stderr, "%s", message);
      record_error(message);
    }
    sprintf(message, "focuser_initialize: improper comms with SmartFocus unit.\n");
    fprintf(stderr, "%s", message);
    record_error(message);
    perror("Error reading from focuser:");
  } else {
    int n;
    fprintf(stderr, "Initialization started...\n");
    // wait up to 30sec for read() to return something other than 0 bytes read.
    for (n=0; n<30; n++) {
      status = read(focus_fd, read_buffer, 1);
      if (status) break;
      sleep(1);
    }
    if (status == 0) {
      fprintf(stderr, "Reset timed out.\n");
    } else if (status == 1 && read_buffer[0] == 'c') {
      fprintf(stderr, "completed.\n");
    } else {
      fprintf(stderr, "Read() returned %d after reset.\n", status);
      sprintf(message, "err. Response = '%c'\n", read_buffer[0]);
      fprintf(stderr, "%s", message);
      record_error(message);
    }
  }
  return 0;
}

