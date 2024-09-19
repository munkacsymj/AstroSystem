/*  focus_jmi.c -- Manages serial link communications with a JMI SmartFocus box
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
#include <errno.h>
#include <stdio.h>
#include "focus.h"

static int LARGEST_FOCUS_POSITION = 2500;
static int SMALLEST_FOCUS_POSITION = 0;

int initialized = 0;
int focus_fd;	/* file descriptor for the focuser */

void get_focus_encoder(void);

char read_one_byte(void) {
  int loop_count = 30; // max timeout in seconds
  while(loop_count--) {
    char response;
    int ret = read(focus_fd, &response, 1);
    if (ret == 1) return response;
    if (ret < 0 && errno != EAGAIN) break;
    sleep(1);
  }
  return 0;
}

void initialize_jmi(void) {
  struct termios term_struct;

  focus_fd = open("/dev/ttyS0", O_RDWR);
  if(focus_fd < 0) {
    perror("Unable to open serial connection to JMI SmartFocus");
    return;
  }

  term_struct.c_iflag = (IGNBRK | IGNPAR) ;
  term_struct.c_oflag = 0;
  term_struct.c_cflag = (CS8 | CREAD | CLOCAL);
  term_struct.c_lflag = 0;
  term_struct.c_cc[VMIN] = 0;
  term_struct.c_cc[VTIME] = 5; // 1/2 second

  cfsetospeed(&term_struct, B9600);
  cfsetispeed(&term_struct, B9600);
  if(tcsetattr(focus_fd, TCSANOW, &term_struct) != 0) {
    perror("Unable to setup /dev/tty to JMI SmartFocus");
  } else {
    int loop_count = 4;
    int bad_bytes = 0;
    char read_buffer[2];
    
    while (loop_count-- >= 0) {
      int status = read(focus_fd, read_buffer, 1);
      if (status == 1) {
	bad_bytes++;
	loop_count++;
      }
      sleep(1);
    }
    if (bad_bytes) {
      fprintf(stderr, "focus_jmi: %d bad bytes were flushed.\n", bad_bytes);
    }
    get_focus_encoder();
  }
}

static long NetFocusPosition = 0; /* encoder value */

void get_focus_encoder(void) {
  char query_cmd = 'p';		/* Read Position Register */
  int status;
  unsigned char response_buffer[4];

  if (!initialized) {
    initialized = 1;
    initialize_jmi();
  }

  status = write(focus_fd, &query_cmd, 1); /* 1-byte command */
  fprintf(stderr, "focus_jmi: 1-byte write returned %d\n",
	  status);

  status = read_one_byte();
  //status = read(focus_fd, response_buffer, 3); /* 3-byte response */
  //fprintf(stderr, "focus_jmi: 3-byte read returned %d\n",
  //	  status);
  if (status != 'p') {
    fprintf(stderr, "focus_jmi: incorrect response to p command: 0x%02x\n",
	    (unsigned int) status);
    return;
  }
  response_buffer[1] = read_one_byte();
  response_buffer[2] = read_one_byte();

  NetFocusPosition = ((unsigned int) response_buffer[1]) << 8 |
    ((unsigned int) response_buffer[2]);
  fprintf(stderr,
	  "focus_jmi: focuser encoder value = %ld (0x%02x,0x%02x)\n",
	  (long) NetFocusPosition, response_buffer[1], response_buffer[2]);
}

void focus(int direction, unsigned long duration) {
  if (!initialized) {
    initialized = 1;
    initialize_jmi();
  }

  long desired_position;
  if (direction == NO_DIRECTION_MOVE_ABSOLUTE) {
    desired_position = duration;
  } else {
    desired_position = NetFocusPosition +
      ((direction == DIRECTION_IN) ? -duration : duration);
  }

  // limit motion if a bad request was sent
  if (desired_position < SMALLEST_FOCUS_POSITION) {
    desired_position = SMALLEST_FOCUS_POSITION;
  }
  if (desired_position > LARGEST_FOCUS_POSITION) {
    desired_position = LARGEST_FOCUS_POSITION;
  }
  
  unsigned char set_focus_cmd[3];
  int status;

  set_focus_cmd[0] = 'g';
  set_focus_cmd[1] = desired_position >> 8;
  set_focus_cmd[2] = desired_position & 0x00ff;

  fprintf(stderr, "focus_jmi: sending goto(%ld) command\n",
	  desired_position);

  status = write(focus_fd, set_focus_cmd, 3);
  if(status != 3) {
    perror("focus: unable to send focus message");
  } else {
    char response[2];

    response[0] = read_one_byte();
    response[1] = read_one_byte();

    if (response[0] != 'g' || response[1] != 'c') {
      fprintf(stderr, "focus_jmi: bad response to goto command: %c%c\n",
	      response[0], response[1]);
    } else {
      fprintf(stderr, "focus_jmi: good response to goto command.\n");
    }

    get_focus_encoder();
  }
}

void focus_move(int direction,
		unsigned long total_duration,
		unsigned long step_size) {
  int number_of_steps = total_duration/step_size;

  while(number_of_steps-- > 0) {
    focus(direction, step_size);
    sleep(2);
  }
}
  
long cum_focus_position(void) {
  return NetFocusPosition;
}

