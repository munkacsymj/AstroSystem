/*  focus.cc -- Manages the focus motor on the mount
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
#include <unistd.h>
#include "focus.h"

static long NetFocusPosition = 0;

void focus(int direction, unsigned long duration_in_usec) {
  static char start_out[] = ":F+#";
  static char start_in[] = ":F-#";
  static char end_out[] = ":FQ#";
  static char end_in[] = ":FQ#";
  static int prior_direction;

  static int focus_speed_set = 0;

  size_t message_size_start, message_size_end;
  char *message_start, *message_end;
  int status;

  if(!focus_speed_set) {
    static char set_speed[] = ":FS#"; /* slow */
    focus_speed_set = 1;
    if(write(lx200_fd, set_speed, sizeof(set_speed)) != sizeof(set_speed)) {
      perror("focus: unable to send set focus speed command");
    }
    /* we set focus speed during initialization, and we reset our */
    /* direction history flag at the same time. */
    prior_direction = direction;
  }

#if 0
  if(direction != prior_direction) {
    long SavedFocusPosition = NetFocusPosition;
    prior_direction = direction;
    focus(direction, 300000);	/*  1200 msec hysteresis */
    NetFocusPosition = SavedFocusPosition;
  }
#endif

  if(direction == DIRECTION_IN) {
    message_size_start = sizeof(start_in);
    message_size_end   = sizeof(end_in);
    message_start = start_in;
    message_end = end_in;
    NetFocusPosition += duration_in_usec;
  } else {
    message_size_start = sizeof(start_out);
    message_size_end = sizeof(end_out);
    message_start = start_out;
    message_end = end_out;
    NetFocusPosition -= duration_in_usec;
  }
  
  status = write(lx200_fd, message_start, message_size_start);
  if(status != message_size_start) {
    perror("focus: unable to send focus message");
  } else {
    if(direction == DIRECTION_IN) {
      duration_in_usec = (int) (1.025 * duration_in_usec + 0.5);
    }
    usleep(duration_in_usec + 5000);

    status = write(lx200_fd, message_end, message_size_end);
    if(status != message_size_end) {
      perror("focus: unable to send stop-focus-motor message");
    }
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
  return NetFocusPosition/1000;
}
