// This may look like C code, but it is really -*- C++ -*-
/*  proc_messages.h -- handles cross-process messages
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
#ifndef _PROC_MESSAGES_H
#define _PROC_MESSAGES_H

#include <list>

// Messages are sent between processes.
// Each process has a process name (a string) which is taken from
// argv[0] (the name of the executable file)
// Each message has a message ID (an integer)
// Each message has an optional message parameter

// SendMessage will return SM_Okay on success or will return
// SM_Not_Found if the destination string does not match a known
// executable.

#define SM_Okay 0
#define SM_Not_Found -1

// Message types
#define SM_ID_Abort 1
#define SM_ID_Pause 2
#define SM_ID_Resume 3

// SendMessage() may block briefly if there is contention for the
// shared memory area used to pass messages back and forth
int SendMessage(const char *destination,
		int message_id,
		long message_param = 0);


// ReceiveMessage() will return the number of messages in the process'
// queue at the time of invocation. (The number of messages after the
// call will be one less than the return value, because one message
// will be pulled off the queue.) The message ID will be stored into
// message_id and if message_param is not nil, a param will also be
// stored. If there are no available messages, ReceiveMessage() will
// return 0.
int ReceiveMessage(const char *my_name,
		   int *message_id,
		   long *message_param = 0);

// GetProcessList() will return a list of processes that are known to
// "notify". 
typedef std::list<char *> ProcessList;
ProcessList *GetProcessList(void);

#endif

