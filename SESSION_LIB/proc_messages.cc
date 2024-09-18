// This may look like C code, but it is really -*- C++ -*-
/*  proc_messages.cc -- handles cross-process messages
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

#include "proc_messages.h"
#include <sys/mman.h>		// mmap(); note: must link with -lrt
#include <pthread.h>
#include <sys/stat.h>		// mode constants
#include <fcntl.h>		// O_ constants
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#define MAX_PROC_NAME 64	// max # characters in a process name
#define MAX_MESSAGES 100
#define MAX_NUM_PROCS 32

struct SM_Message {
  bool inuse;
  int target_proc_index;
  int  SM_message_id;
  long SM_parameter_value;
};

struct SM_Area {
  pthread_mutex_t protect_lock;
  SM_Message all_messages[MAX_MESSAGES];
  char all_proc_name[MAX_NUM_PROCS][MAX_PROC_NAME];
};

SM_Area *area = 0;

void SetupArea(void) {
  // if area is not <nil>, then SetupArea() was previously called
  if (area) return;

  int shm_fd = shm_open("/astro_control_messages", O_RDWR|O_CREAT, 0666);
  if (shm_fd < 0) {
    perror("proc_messages: error creating shared memory:");
    return;
  }

  if (ftruncate(shm_fd, sizeof(struct SM_Area))) {
    perror("proc_messages: error setting shared memory size: ");
    return;
  }
  
  area = (SM_Area *) mmap(0, sizeof(struct SM_Area),
			  PROT_READ | PROT_WRITE,
			  MAP_SHARED, shm_fd, 0);
  if (area == MAP_FAILED) {
    perror("proc_messages: error mapping shared memory: ");
    return;
  }
  
  // try and obtain the lock. If it fails with EINVAL, this means that
  // the mutex has not been initialized, and we should go set things
  // up.
  if (pthread_mutex_lock(&area->protect_lock)) {
    // error. Verify that we got EINVAL
    if (errno == EINVAL) {
      // need to initialize
      pthread_mutex_init(&area->protect_lock, NULL);
      pthread_mutex_lock(&area->protect_lock);
      for (int i=0; i<MAX_MESSAGES; i++) {
	area->all_messages[i].inuse = false;
      }
    } else {
      // lock failed due to something other than lack of
      // initialization??
      perror("proc_messages: error testing lock: ");
    }
  }
  // we obtained the lock. Release it immediately
  pthread_mutex_unlock(&area->protect_lock);
}

#define NO_CREATE 0
#define CREATE_IF_NEEDED 1

int proc_name_to_index(const char *proc_name, int creation) {
  for (int i=0; i<MAX_NUM_PROCS; i++) {
    if (area->all_proc_name[i][0] == 0) {
      if (creation == CREATE_IF_NEEDED) {
	strcpy(area->all_proc_name[i], proc_name);
	return i;
      } else {
	return -1; // not found
      }
    } else if (strcmp(area->all_proc_name[i], proc_name) == 0) {
      return i;
    }
  }
  // only way to reach here is to have filled the proc_name array!
  fprintf(stderr, "proc_messages: proc_name_array is FULL!\n");
  exit(-2);
  /*NOTREACHED*/
  return 0;
}
	
//****************************************************************
//        SendMessage
//****************************************************************
int SendMessage(const char *destination,
		int message_id,
		long message_param) {
  int ret_value = 0;
  SetupArea();
  // grab the lock
  pthread_mutex_lock(&area->protect_lock);

  int proc_index = proc_name_to_index(destination, NO_CREATE);
  if (proc_index < 0) {
    fprintf(stderr, "proc_messages: no process called %s known.\n",
	    destination);
    ret_value = -1; // error return
  } else {
    bool found = false;
    // otherwise, we successfully found the proc's name
    for (int i=0; i<MAX_MESSAGES; i++) {
      if (area->all_messages[i].inuse == false) {
	area->all_messages[i].target_proc_index = proc_index;
	area->all_messages[i].SM_message_id = message_id;
	area->all_messages[i].SM_parameter_value = message_param;
	area->all_messages[i].inuse = true;
	found = true;
	break;
      }
    }

    if (!found) {
      fprintf(stderr, "proc_messages: message queue already full.\n");
      ret_value = -1; // error_return
    }
  }

  pthread_mutex_unlock(&area->protect_lock);
  return ret_value;
}

//****************************************************************
//        ReceiveMessage()
//****************************************************************
int ReceiveMessage(const char *my_name,
		   int *message_id,
		   long *message_param) {
  int ret_value = 0;
  int first_message = -1;
  SetupArea();
  // grab the lock
  pthread_mutex_lock(&area->protect_lock);

  int proc_index = proc_name_to_index(my_name, CREATE_IF_NEEDED);

  for (int i=0; i<MAX_MESSAGES; i++) {
    if (area->all_messages[i].inuse &&
	area->all_messages[i].target_proc_index == proc_index) {
      if (first_message < 0) {
	first_message = i;
	*message_id = area->all_messages[i].SM_message_id;
	if (message_param) {
	  (*message_param) = area->all_messages[i].SM_parameter_value;
	}
	area->all_messages[i].inuse = false;
      }
      ret_value++;
    }
  }

  pthread_mutex_unlock(&area->protect_lock);

  return ret_value;
}

//****************************************************************
//        GetProcessList()
//****************************************************************
ProcessList *GetProcessList(void) {
  ProcessList *result = new ProcessList;

  SetupArea();
  // grab the lock
  pthread_mutex_lock(&area->protect_lock);
      
  for (int i=0; i<MAX_NUM_PROCS; i++) {
    if (area->all_proc_name[i][0] != 0) {
      char *new_proc = strdup(area->all_proc_name[i]);
      result->push_back(new_proc);
    }
  }

  pthread_mutex_unlock(&area->protect_lock);

  return result;
}
  

  
