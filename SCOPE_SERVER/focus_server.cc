/*  focus_server.cc -- Main program (server) handling commands for the mount
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
#include <stdio.h>
#include <stdlib.h>		// exit()
#include <string.h>		// memset()
#include <errno.h>
#include <sys/time.h>		// struct timeval
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include "lx200.h"
#include "focus.h"
#include "flatlight.h"
#include "ports.h"
#include "scope_message_handler.h"

void ProcessMessages(void);

/*
 * invocation:
 * focus_server
 */

void usage(const char *string) {
  fprintf(stderr, "%s: usage: focus_server \n",
	  string);
  exit(2);
}

int main(int argc, char **argv) {
  if(argc != 1) {
    usage("wrong # arguments");
  }

  write_log = 1;
  InitFlatLight();
  initialize_lx200();
  ProcessMessages();

}

#define MAX_CONNECTIONS 5

static int connection[MAX_CONNECTIONS];
static int largest_fd = 2;

void ProcessMessages(void) {
  struct timeval tv;
  int retval;
  int s1, s2;
  struct sockaddr_in my_address;

  for(int k = 0; k<MAX_CONNECTIONS; k++) {
    connection[k] = -1;
  }

  memset(&my_address, 0, sizeof(my_address));
  my_address.sin_port = htons(SCOPE_PORT);
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
    if(setsockopt(s1, SOL_SOCKET, SO_REUSEADDR, &True, Length) < 0) {
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

#if 0
  {
    struct sockaddr_in his_address;
    int socketaddresslength = sizeof(his_address);
    int s2;

    s2 = accept(s1,
		(struct sockaddr *) &his_address,
		&socketaddresslength);
    fprintf(stderr, "Connection accepted.\n");
    exit(1);
  }
#endif

  while(1) {
    fd_set  server_fds_r;

    {
      FD_ZERO(&server_fds_r);
      FD_SET(s1, &server_fds_r);
      for(int j=0; j<MAX_CONNECTIONS; j++) {
	if(connection[j] >= 0) {
	  FD_SET(connection[j], &server_fds_r);
	}
      }
    }
    
    fprintf(stderr, "Calling select() with n=%d after setting fd=%d\n",
	    largest_fd+1, s1);
    retval = select(largest_fd+1, &server_fds_r, 0, 0, 0);
    fprintf(stderr, "select() returned %d.\n", retval);

    if(retval < 0) {
      perror("focus_server: select failure");
      exit(2);
    } else {
      if(retval > 0) {
	int j;

	for(j=0; j<MAX_CONNECTIONS; j++) {
	  if(connection[j] >= 0 &&
	     FD_ISSET(connection[j], &server_fds_r)) {
	    fprintf(stderr, "Received message on socket %d\n", connection[j]);
	    if(handle_message(connection[j]) < 0) {
	      // Handle_message() will return -1 if the connection has
	      // been lost
	      fprintf(stderr, "Closing connection on socket %d\n",
		      connection[j]);
	      close(connection[j]);
	      FD_CLR(connection[j], &server_fds_r);
	      connection[j] = -1;
	    }
	  }
	}

	if(FD_ISSET(s1, &server_fds_r)) {
	  // New connection requested

	  struct sockaddr_in his_address;
	  socklen_t socketaddresslength = sizeof(his_address);
	  int s2;

	  fprintf(stderr, "Getting new socket connection.\n");
	  s2 = accept(s1,
		      (struct sockaddr *) &his_address,
		      &socketaddresslength);
	  FD_SET(s2, &server_fds_r);
	  if(s2 > largest_fd) largest_fd = s2;
	  fprintf(stderr, "Connection established on socket %d.\n", s2);
	  {
	    int j;
	    for(j=0; j<MAX_CONNECTIONS; j++) {
	      if(connection[j] == -1) {
		connection[j] = s2;
		break;
	      }
	    }
	  }
	}
      }
    }
    /*NOTREACHED*/
  }
}
