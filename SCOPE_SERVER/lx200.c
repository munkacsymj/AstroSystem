/*  lx200.c -- Manages serial link communications with a Gemini/LX200
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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <string.h>		/* strlen() */
#include <unistd.h>
#include <stdio.h>
#include "lx200.h"

int lx200_fd;

#define GM2000 // or, alternatively, GEMINI
#ifdef GM2000

#include <netdb.h>		// gethostbyname()
#include <netinet/in.h>
#include <string.h>		// memset()
#include <stdlib.h>		// exit()
#include <sys/socket.h>		// socket()
#include <arpa/inet.h>		// inet_ntoa()

#define SCOPE_HOST "gm2000"
#define SCOPE_PORT 3490

void initialize_lx200(void) {
  struct hostent *mount_ip;
  struct sockaddr_in my_address;

  memset(&my_address, 0, sizeof(my_address));
  mount_ip = gethostbyname(SCOPE_HOST);
  if(mount_ip == 0) {
    herror("Cannot lookup mount_ip (GM2000) host name:");
    exit(2);
  } else {
    my_address.sin_addr = *((struct in_addr *)(mount_ip->h_addr_list[0]));
    my_address.sin_port = htons(SCOPE_PORT); // port number
    my_address.sin_family = AF_INET;
    fprintf(stderr, "Connecting to mount @ %s\n",
	    inet_ntoa(my_address.sin_addr));
  }

  lx200_fd = socket(PF_INET, SOCK_STREAM, 0);
  if(lx200_fd < 0) {
    perror("Error creating mount socket");
    exit(2);
  }

  if(connect(lx200_fd,
	     (struct sockaddr *) &my_address,
	     sizeof(my_address)) < 0) {
    perror("Error connecting to mount socket");
    exit(2);
  }
  
}

#else // GEMINI

void initialize_lx200(void) {
  struct termios term_struct;

  lx200_fd = open("/dev/ttyS0", O_RDWR);
  if(lx200_fd < 0) {
    perror("Unable to open serial connection to lx200");
    return;
  }

  term_struct.c_iflag = (IGNBRK | IGNPAR) ;
  term_struct.c_oflag = 0;
  term_struct.c_cflag = (CS8 | CREAD | CLOCAL);
  term_struct.c_lflag = 0;

  cfsetospeed(&term_struct, B9600);
  cfsetispeed(&term_struct, B9600);
  if(tcsetattr(lx200_fd, TCSANOW, &term_struct) != 0) {
    perror("Unable to setup /dev/tty to LX200");
  }
}

#endif

int write_log = 0;
FILE *logfile = 0;
int logfile_initialized = 0;

void init_logfile(void) {
  logfile_initialized = 1;
  logfile = fopen("/tmp/mount.log", "w");
  if (!logfile) {
    perror("Unable to create mount.log in /tmp: ");
  }
}

int write_mount(int fd, const void *buffer, long buf_len) {
  if (write_log) {
    if (!logfile_initialized) init_logfile();

    if (logfile) {
      fprintf(logfile, "\n%s\n  ", (const char *) buffer);
      fflush(logfile);
    }
  }
  return write(fd, buffer, buf_len);
}
  
int read_mount(int fd, void *buffer, long count) {
  long result = read(fd, buffer, count);
  if (result > 0 && write_log) {
    if (!logfile_initialized) init_logfile();

    ((char *)buffer)[result] = 0;
    if (logfile) {
      fprintf(logfile, "%s", (char *) buffer);
      fflush(logfile);
    }
  }
  return result;
}

