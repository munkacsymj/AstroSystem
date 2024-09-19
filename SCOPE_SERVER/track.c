/*  track.c -- Server-side handling of small mount motion (guiding) commands
 *
 *  Copyright (C) 2007, 2018 Mark J. Munkacsy

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
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>		/* for abs() */
#include <string.h>		/* strlen */
#include "track.h"
#include <string.h>

static int tmin(int a, int b) {
  if(a < b) return a;
  return b;
}

static int tmax(int a, int b) {
  if(a > b) return a;
  return b;
}

#define GM2000 // or, alternatively, GEMINI
#ifdef GM2000
char commands_sent[64];
char *commands_sent_ptr;

void any_track(int msec, char direction_letter) {
  char msg_buf[64];

  sprintf(msg_buf, ":M%c%03d#", direction_letter, msec);
  if (write(lx200_fd, msg_buf, strlen(msg_buf)) != strlen(msg_buf)) {
    perror("track: unable to send guide command");
  }

  usleep(msec * 1000); /* convert msec to usec */

  strcpy(commands_sent_ptr, msg_buf);
  commands_sent_ptr += strlen(msg_buf);
  strcpy(commands_sent_ptr, "\n");
  commands_sent_ptr += 1;
}

void track(int NorthMSec, int EastMSec) {
  static int guide_speed_set = 0;
  commands_sent_ptr = 0;
  commands_sent[0] = 0;

  /* guide speed needs to be set once per session, but it doesn't hurt
     to set it extra times. Make a half-hearted attempt to only set it
     once. */
  
  if (!guide_speed_set) {
    guide_speed_set = 1;
    const char *set_speed = ":RG0#";
    if (write(lx200_fd, set_speed, strlen(set_speed)) != strlen(set_speed)) {
      perror("track: unable to set guide speed");
    }
  }

  /* any_track needs its first argument to be a positive, non-zero integer */
  if (NorthMSec > 0) {
    any_track(NorthMSec, 'n');
  }
  if (NorthMSec < 0) {
    any_track(-NorthMSec, 's');
  }
  if (EastMSec > 0) {
    any_track(EastMSec, 'e');
  }
  if (EastMSec < 0) {
    any_track(-EastMSec, 'w');
  }

  extern FILE *logfile;
  extern int write_log;

  if (write_log) {
    fprintf(logfile, "%s", commands_sent);
    fflush(logfile);
  }
}
  

#else // GEMINI

void track(int NorthMSec, int EastMSec) {
  static char start_north[] = ":Mn#";
  static char start_south[] = ":Ms#";
  static char start_east[]  = ":Me#";
  static char start_west[]  = ":Mw#";
  static char end_north[]   = ":Qn#";
  static char end_south[]   = ":Qs#";
  static char end_east[]    = ":Qe#";
  static char end_west[]    = ":Qw#";

  size_t message_size_start, message_size_end;
  char *message_start, *message_end;
  int status;

  char *NSMsg = 0;
  char *EWMsg = 0;
  char *NSQuit;
  char *EWQuit;
  char *end_msg;
  int run_time_msec;

  static char set_speed[] = ":RG#"; /* guide mode speed */

  if(NorthMSec == 0 && EastMSec == 0) return;

  if(write(lx200_fd, set_speed, sizeof(set_speed)) != sizeof(set_speed)) {
    perror("track: unable to send set guide speed command");
  }

  if(NorthMSec > 0) {
    NSMsg = start_north;
    NSQuit = end_msg = end_north;
    run_time_msec = NorthMSec;
  }
  if(NorthMSec < 0) {
    NSMsg = start_south;
    NSQuit = end_msg = end_south;
    run_time_msec = -NorthMSec;
  }
  if(EastMSec > 0) {
    EWMsg = start_east;
    EWQuit = end_msg = end_east;
    run_time_msec = EastMSec;
  }
  if(EastMSec < 0) {
    EWMsg = start_west;
    EWQuit = end_msg = end_west;
    run_time_msec = -EastMSec;
  }

  if(NSMsg) {
    status = write(lx200_fd, NSMsg, strlen(NSMsg));
    if(status != strlen(NSMsg)) {
      perror("track: unable to send guide message");
    }
  }

  if(EWMsg) {
    status = write(lx200_fd, EWMsg, strlen(EWMsg));
    if(status != strlen(EWMsg)) {
      perror("track: unable to send guide message");
    }
  }

  if(NorthMSec == 0 || EastMSec == 0) {
    fprintf(stderr, "Running guide motor for %d msec\n", run_time_msec);
    usleep(run_time_msec * 1000); /* convert from msec to usec */
    
    status = write(lx200_fd, end_msg, strlen(end_msg));
    fprintf(stderr, "Guide motor stopped.\n");
    if(status != strlen(end_msg)) {
      perror("track: unable to send stop-guide-motor message");
    }
  } else {
    /* Both directions at same time */
    char *QuitMsg;

    const int interval1 = tmin(abs(NorthMSec), abs(EastMSec));
    const int interval2 = tmax(abs(NorthMSec), abs(EastMSec)) - interval1;

    usleep(interval1  * 1000);	/* convert msec to usec */

    /* stop one */
    QuitMsg = ((abs(NorthMSec) < abs(EastMSec)) ? NSQuit : EWQuit);
    
    status = write(lx200_fd, QuitMsg, strlen(QuitMsg));
    if(status != strlen(QuitMsg)) {
      perror("track: unable to send stop-guide-motor message");
    }

    /* wait the rest of the time */
    usleep(interval2 * 1000);
      
    QuitMsg = ((abs(NorthMSec) >= abs(EastMSec)) ? NSQuit : EWQuit);

    status = write(lx200_fd, QuitMsg, strlen(QuitMsg));
    if(status != strlen(QuitMsg)) {
      perror("track: unable to send stop-guide-motor message");
    }
  }
}
#endif // GEMINI

