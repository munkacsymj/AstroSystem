/*  ccd_server.cc -- Server handles messages sent for camera control
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
#include <string.h>		// memset()
#include <sys/time.h>		// struct timeval
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <signal.h>		// signal()
#include "ports.h"
#include "ccd_server.h"
#include "ccd_message_handler.h"

/****************************************************************/
/*        DEFINITIONS						*/
/****************************************************************/

#define MAX_CONNECTIONS 5

struct connect_data {
  int fd;			// file descriptor
  int in_use;			// 0 means idle, 1 means active
  int auto_notify;		// 0 means ignore, 1 means send status
				// msg whenever an exposure ends
} connections[MAX_CONNECTIONS];

static int largest_fd = 2;

static int timeout_expired;	// nonzero means need to dispatch
				// TimeoutProcedure() from the
				// select() in the mail loop.
static func_ptr UserTimeoutProcedure;
static long     UserTimeoutData;

/****************************************************************/
/*        FORWARD DECLARATIONS					*/
/****************************************************************/

void ProcessMessages(void);

/****************************************************************/
/*        PROCEDURES						*/
/****************************************************************/

/*
 * invocation:
 * ccd_server
 */

void sig_alarm_handler(int sig_number) {
  // very simple
  timeout_expired = 1;
}
	       
void usage(const char *string) {
  fprintf(stderr, "%s: usage: ccd_server \n",
	  string);
  exit(2);
}

int main(int argc, char **argv) {
  if(argc != 1) {
    usage("wrong # arguments");
  }

  initialize_ccd();
  ProcessMessages();

}

void SetTimeout(struct   timeval *interval,
		long     UserData,
		func_ptr TimeoutProcedure) {
  struct itimerval interval_val;

  // this is the time we will pass to setitimer(). The "next" timer
  // field is set to the duration the user gave us. The "periodic"
  // timer is cleared (set to zero) to make this a one-shot timer.
  interval_val.it_value = *interval;
  timerclear(&interval_val.it_interval);

  UserTimeoutProcedure = TimeoutProcedure;
  UserTimeoutData      = UserData;
  timeout_expired      = 0;

  if(setitimer(ITIMER_REAL, &interval_val, 0) < 0) {
    perror("Error starting itimer");
  } else {
    (void) signal(SIGALRM, sig_alarm_handler);
  }
}

void set_auto_notify(int fd,
		     int auto_notify) {
  for(int j=0; j<MAX_CONNECTIONS; j++) {
    if(connections[j].in_use &&
       connections[j].fd == fd) {
      connections[j].auto_notify = auto_notify;
      return;
    }
  }
  fprintf(stderr, "set_auto_notify: fd %d not found.\n", fd);
}

void ProcessMessages(void) {
  struct timeval tv;
  int retval;
  int s1, s2;
  struct sockaddr_in my_address;

  for(int k = 0; k<MAX_CONNECTIONS; k++) {
    connections[k].in_use = 0;
  }

  memset(&my_address, 0, sizeof(my_address));
  my_address.sin_port = htons(CAMERA_PORT);
  my_address.sin_family = AF_INET;
  my_address.sin_addr.s_addr = INADDR_ANY;

  s1 = socket(PF_INET, SOCK_STREAM, 0);
  if(s1 < 0) {
    perror("Error creating socket");
    exit(2);
  }
  if(s1 > largest_fd) largest_fd = s1;
  {
    int True = 1;
    int Length = sizeof(int);
    if(setsockopt(s1,
		  SOL_SOCKET,
		  SO_REUSEADDR,
		  &True,
		  (socklen_t) Length) < 0) {
      perror("Error setting SO_REUSEADDR");
    }
  }

  if(bind(s1, (struct sockaddr *) &my_address, sizeof(my_address)) < 0) {
    perror("Error binding socket");
    fprintf(stderr, "Errno = %d\n", errno);
    exit(2);
  }

  // We allow 5 people to connect.  The number is rather arbitrary.
  if(listen(s1, MAX_CONNECTIONS) < 0) {
    perror("Error setting up socket queue size");
    exit(2);
  }

  fprintf(stderr, "Waiting for connection . . .\n");

  while(1) {
    fd_set  server_fds_r;

    {
      FD_ZERO(&server_fds_r);
      FD_SET(s1, &server_fds_r);
      for(int j=0; j<MAX_CONNECTIONS; j++) {
	if(connections[j].in_use) {
	  FD_SET(connections[j].fd, &server_fds_r);
	}
      }
    }
    
    fprintf(stderr, "Calling select() with n=%d after setting fd=%d\n",
	    largest_fd+1, s1);
    if (timeout_expired) {
      retval = -1;
    } else {
      retval = select(largest_fd+1, &server_fds_r, 0, 0, 0);
      fprintf(stderr, "select() returned %d.\n", retval);
    }

    if(timeout_expired) {
      fprintf(stderr, "Timeout expired.\n");
      timeout_expired = 0;	// prevent repeats
      (UserTimeoutProcedure)(UserTimeoutData);
    }


    // quietly ignore if retval <= 0, since was probably caused by
    // an interval timer timing out

    if(retval > 0) {
      int j;

      for(j=0; j<MAX_CONNECTIONS; j++) {
	const int this_fd = connections[j].fd;

	if(connections[j].in_use &&
	   FD_ISSET(this_fd, &server_fds_r)) {
	  fprintf(stderr, "Received message on socket %d\n", this_fd);
	  if(handle_message(this_fd) < 0) {
	    // Handle_message() will return -1 if the connection has
	    // been lost
	    fprintf(stderr, "Closing connection on socket %d\n",
		    this_fd);
	    close(this_fd);
	    FD_CLR(this_fd, &server_fds_r);
	    connections[j].in_use = 0;
	  }
	}
      }

      if(FD_ISSET(s1, &server_fds_r)) {
	// New connection requested

	struct sockaddr_in his_address;
	int socketaddresslength = sizeof(his_address);
	int s2;

	fprintf(stderr, "Getting new socket connection.\n");
	s2 = accept(s1,
		    (struct sockaddr *) &his_address,
		    (socklen_t *) &socketaddresslength);
	FD_SET(s2, &server_fds_r);
	if(s2 > largest_fd) largest_fd = s2;
	fprintf(stderr, "Connection established on socket %d.\n", s2);
	{
	  int j;
	  for(j=0; j<MAX_CONNECTIONS; j++) {
	    if(connections[j].in_use == 0) {
	      connections[j].in_use = 1;
	      connections[j].fd = s2;
	      connections[j].auto_notify = 0;	// default
	      break;
	    }
	  }
	}
      }
    }
    /*NOTREACHED*/
  }
}
