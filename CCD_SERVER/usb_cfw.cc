/*  usb_cfw.cc -- Handle the QHYCFW3 when connected via USB
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

#include "usb_cfw.h"
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <termios.h>		// tcflush()
#include <fcntl.h>

static int cfw_fd = -1;
pthread_t thread_id;
static int current_position = -1;
char readbuffer[80]; // way more than needed
int readbufferindex = 0;
pthread_mutex_t mutex;
time_t init_start;
bool in_move = false;
int filtercount = -1;

void ResetReadBuffer(void) {
  if(pthread_mutex_lock(&mutex)) {
    perror("pthread_mutex_lock(): ");
  } else {
    if (readbufferindex) {
      fprintf(stderr, "usb_cfw: flushing %d chars from CFW.\n",
	      readbufferindex);
      readbufferindex = 0;
    }
    if (pthread_mutex_unlock(&mutex)) {
      perror("pthread_mutex_unlock(): ");
    }
  }
}

// This is the "main program" of the usb_cfw dedicated thread. Its job
// is to keep reading characters coming back from the CFW.
void *USBCFW_read_thread(void *unused) {
  char buffer;
  
  while(1) {
    int ret = read(cfw_fd, &buffer, 1);

    if (ret == 1) {
      if(pthread_mutex_lock(&mutex)) {
	perror("pthread_mutex_lock(): ");
      } else {
	fprintf(stderr, "USBCFW_read_thread: captured '%c' = 0x%02x\n",
		buffer, buffer);
	readbuffer[readbufferindex++] = buffer;
	if (pthread_mutex_unlock(&mutex)) {
	  perror("pthread_mutex_unlock(): ");
	}
      }
    } else if (ret < 0) {
      fprintf(stderr, "USBCFW_read_thread: read failed.\n");
      sleep(2);
    }
  }
}
  

void USBCFWInitializeStart(void) {
  if (pthread_mutex_init(&mutex,
			 nullptr)) { // pthread_mutex_attr
    perror("pthread_mutex_init(): ");
  }
			 
  const char *cfw_filename = "/dev/serial/by-id/usb-Silicon_Labs_CP2102_USB_to_UART_Bridge_Controller_0001-if00-port0";
  cfw_fd = open(cfw_filename, O_RDWR);
  if (cfw_fd < 0) {
    perror("Cannot open link to CFW.\n");
    exit(-1);
  } else {
    fprintf(stderr, "USBCFWInitialize() init via USB.\n");
    init_start = time(0);
  }

  struct termios raw, orig_termios;
  if (tcgetattr(cfw_fd, &orig_termios) < 0) {
    fprintf(stderr, "Error getting original termios struct.\n");
    return;
  }

  fprintf(stderr, "...init c_iflag = 0x%lx\n",
	  (unsigned long)orig_termios.c_iflag);
  fprintf(stderr, "...init c_oflag = 0x%lx\n",
	  (unsigned long)orig_termios.c_oflag);
  fprintf(stderr, "...init c_cflag = 0x%lx\n",
	  (unsigned long)orig_termios.c_cflag);
  
  raw = orig_termios; // copy original
  if (cfsetispeed(&raw, B9600)) {
    fprintf(stderr, "Error setting input speed (usb_cfw)\n");
  }
  if (cfsetospeed(&raw, B9600)) {
    fprintf(stderr, "Error setting output speed (usb_cfw)\n");
  }
  
  raw.c_iflag = (IGNBRK | IGNPAR );
  //raw.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ICRNL | INPCK | ISTRIP | IXON |
  //		   IXOFF | IXANY | IGNCR | INLCR);
  raw.c_oflag = 0;
  //raw.c_oflag &= ~(OPOST | ONLCR);
  //raw.c_cflag = (CS8 | CREAD | CLOCAL);
  raw.c_cflag &= ~(PARENB | CSTOPB | CSIZE | CRTSCTS);
  raw.c_cflag |= (CS8 | CLOCAL);
  raw.c_lflag &= ~(ECHO | ECHOE | ICANON | IEXTEN | ISIG);

  if (tcsetattr(cfw_fd, TCSAFLUSH, &raw) < 0) {
    fprintf(stderr, "Error setting USB TTY to raw mode.\n");
    return;
  }
  
  int r = tcflush(cfw_fd, TCIOFLUSH);
  if (r) {
    fprintf(stderr, "Error flushing USB TTY buffers.\n");
  }

  int err = pthread_create(&thread_id,
			   nullptr,
			   USBCFW_read_thread,
			   nullptr);
  if (err) {
    fprintf(stderr, "Error creating thread in usb_cfw: %d\n", err);
  }
}

static int initialization_complete = false;

bool USBCFWInitializationComplete(void) { return initialization_complete; }

int USBCFWInitializeEnd(void) {
  if (initialization_complete) return filtercount;
  
  time_t now = time(0);
  int to_go = 22 - (now-init_start);
  if (to_go > 0) {
    fprintf(stderr, "USBCFWInitializeEnd(): sleeping for %d secs\n",
	    1+to_go);
    sleep(1+to_go);
  } else {
    fprintf(stderr, "USBCFWInitializeEnd(): to_go = %d\n", to_go);
  }

  // flush anything in the buffer
  ResetReadBuffer();
  // command filter move to position 0. Should complete quickly
  USBMoveFilterWheel(0);
  for (int i=0; i<10; i++) {
    if (readbufferindex) {
      // Aha! received something!
      if (readbufferindex > 1 || readbuffer[0] != '0') {
	fprintf(stderr, "USBCFWInitializeEnd(): invalid response_A: 0x%02x\n",
		readbuffer[0]);
      }
      break;
    }
    sleep(1);
  }
  current_position = 0;
  if (readbufferindex == 0) {
    fprintf(stderr, "USBCFWInitializeEnd(): init test_A failed.\n");
  }
  // flush from buffer
  ResetReadBuffer();

  // Fetch filter count
  int ret = write(cfw_fd, "MXP", 3);
  if (ret != 3) {
    perror("Error writing MXP to CFW:");
  }
  for (int i=0; i<10; i++) {
    if (readbufferindex) {
      break;
    } else {
      usleep(100000); // 0.1 seconds
    }
  }
  if (readbufferindex) {
    filtercount = readbuffer[0] - '0' + 1;
    fprintf(stderr, "filtercount = %d\n", filtercount);
    ResetReadBuffer();
  } else {
    fprintf(stderr, "Bad result from FetchFilterCount()\n");
  }

  // Fetch firmware version
  ret = write(cfw_fd, "VRS", 3);
  if (ret != 3) {
    perror("Error writing VRS to CFW:");
  } 

  for (int i=0; i<10; i++) {
    if (readbufferindex >= 8) {
      break;
    } else {
      usleep(100000); // 0.1 seconds
    }
  }
  if (readbufferindex == 8) {
    readbuffer[8] = 0;
    fprintf(stderr, "CFW FW Version = %s\n", readbuffer);
  } else {
    fprintf(stderr, "CFW FW Version bad fetch: %d\n",
	    readbufferindex);
  }
  initialization_complete = true;
  ResetReadBuffer();
  return filtercount;
}

// This should initiate the motion and be non-blocking
void USBMoveFilterWheel(int position) {
  ResetReadBuffer();
  char cmd = '0' + position;
  fprintf(stderr, "USBMoveFilterWheel to position %d ... ", position);
  int ret = write(cfw_fd, &cmd, 1);
  if (ret != 1) {
    fprintf(stderr, "\nusb_cfw: wrong response from write(): %d\n",
	    ret);
  } else {
    fprintf(stderr, "started.\n");
  }
  in_move = true;
}

int USBCFWCurrentPosition(void) {
  if (in_move && readbufferindex) {
    current_position = readbuffer[0] - '0';
    ResetReadBuffer();
  }
  return current_position;
}


