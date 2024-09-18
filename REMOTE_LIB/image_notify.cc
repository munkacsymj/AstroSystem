/*  image_notify.cc -- Interprocess communication to notify process
 *  when new image is available from the camera
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
#include <fcntl.h>		// needed for open(), flock()
#include <stdio.h>
#include <sys/types.h>		// needed for getpid(), stat()
#include <sys/file.h>		// flock()
#include <sys/stat.h>		// needed for stat()
#include <unistd.h>		// getpid(),
#include <stdlib.h>		// malloc(), free()
#include <signal.h>		// SIGUSR1
#include "image_notify.h"

/*
 * Two special files are created in /var/run: 
 *    - one holds the name of the most recent image file
 *    - the other holds the (single) PID of a process that wants to be
 *    notified when a new image becomes available.
 *
 * Whenever a process creates an image file from an exposure, that
 * process will call NotifyServiceProvider(image_filename). That call
 * will result in the filename getting put into the corresponding
 * /var/run file. The process that created the image will then read
 * the file holding the PID and send a SIGUSR1 signal to that
 * process. 
 *
 * Meanwhile, the process that wants to receive the signal will have
 * called RegisterAsProvider(), with a pointer to the (X11-style)
 * callback to be invoked when an image appears. That call will put
 * its PID into the special /var/run PID file. When the SIGUSR1 signal
 * arrives, the callback will be invoked with the image filename
 * (extracted from the /var/run file) provided as an argument.
 *
 * LOCKING: We try to protect against non-atomic reads and writes to
 * the two /var/run files. That way we can be confident that any PID
 * we read will actually be a PID and not garbage. Similarly, the
 * filename that we read for the image will not be "sliced in half" by
 * the other process performing a write while we are doing a read.
 */

static const char *PIDfilename = "/home/ASTRO/var/ASTRO_image_monitor.pid";
static const char *FilenameFilename = "/home/ASTRO/var/ASTRO_last_image.filename";
static const char *LockFilename = "/home/ASTRO/var/ASTRO_notification_lock";

const static int NOTIFY_SIG = SIGUSR1;

XtSignalId sigid;

static void (*userCallback)(char *);

static bool use_unix_signal = false;

// Normal Unix signal handler for SIGUSR1
void UnixSigHandler(int n) {
  if (use_unix_signal) {
    userCallback(ProvideCurrentFilename());
  } else {
    XtNoticeSignal(sigid);
  }
}

typedef enum { LOCK_FOR_READ, LOCK_FOR_WRITE } lock_style_type;
static int lock_fd = -1;	// local to this process
static void GetLock(lock_style_type lock_type) {
  lock_fd = open(LockFilename,
		 O_WRONLY | O_CREAT | O_TRUNC,
		 0666);
  if(lock_fd < 0) {
    perror("NotifyServiceProvider: Cannot open /home/ASTRO/var lock file");
    return;
  }
  flock(lock_fd, (lock_type == LOCK_FOR_READ ? LOCK_SH : LOCK_EX));
}

static void ReleaseLock(void) {
  if(lock_fd >= 0) {
    flock(lock_fd, LOCK_UN);
    close(lock_fd);
    lock_fd = -1;
  }
}

char *ProvideCurrentFilename(void) {
  int fd = open(FilenameFilename, O_RDONLY, 0);
  if(fd < 0) {
    fprintf(stderr, "image_notify callback: %s not found.\n",
	    FilenameFilename);
    return 0;
  }

  // Get an advisory lock on the filename file. (Readers get a shared
  // lock. Writers get an exclusive lock.)
  GetLock(LOCK_FOR_READ);
  // from this point on *must* ensure that lock is released before return.
  
  // "stat()" the file to find its length so we can malloc() a
  // long-enough string.
  struct stat file_info;
  fstat(fd, &file_info);

  char *s;
  int read_count;
  const int max_length = file_info.st_size + 4;
  char *read_filename = (char *) malloc(max_length);
  if(!read_filename) {
    perror("error allocating memory for read_filename");
    goto unlock;
  }

  read_filename[0] = 0;		// accident protection
  read_count = read(fd, read_filename, max_length);
  if(read_count <= 0) {
    free(read_filename);
    read_filename = 0;
    goto unlock;
  }
  read_filename[read_count]=0;

  // Clean up the filename a little
  s = read_filename;
  while(*s && *s != '\n') s++;
  *s = 0;

unlock:
  ReleaseLock();
  close(fd);
  return read_filename;
}

void XSignalCallback(XtPointer client,
		     XtSignalId *signalid) {
  char *filename = ProvideCurrentFilename();

  if(filename) {
    userCallback(filename);

    free(filename);
  }
}



void RegisterAsProviderCommon(bool raw_mode, void (*callback)(char *)) {
  static int already_registered = 0;
  use_unix_signal = raw_mode;
  if(already_registered) {
    fprintf(stderr, "RegisterAsProvider: double registration attempted.\n");
    exit(-2);
  } else {
    already_registered = 1;
  }

  userCallback = callback;
  (void) signal(NOTIFY_SIG, UnixSigHandler);

  const long processID = getpid();

  GetLock(LOCK_FOR_WRITE);
  FILE *fp = fopen(PIDfilename, "w");
  if(!fp) {
    perror("Cannot open PIDfilename");
  } else {
    fprintf(fp, "%ld\n", processID);
    fclose(fp);
  }
  ReleaseLock();
}

// This call registers the process executing the call as a service
// provider that needs to be notified whenever a library user sends a
// "NofityServiceProvider()" call.
void RegisterAsProvider(XtAppContext context,
			void (*callback)(char *)) {
  sigid = XtAppAddSignal(context, XSignalCallback, (XtPointer) NOTIFY_SIG);
  RegisterAsProviderCommon(false, callback);
}

void RegisterAsProviderRaw(void (*callback)(char *)) {
  RegisterAsProviderCommon(true, callback);
}

/****************************************************************/
/*        Library-user calls					*/
/****************************************************************/

void NotifyServiceProvider(const char *filename) {
  int fd = open(FilenameFilename,
		O_WRONLY | O_CREAT | O_TRUNC,
		0666);

  if(fd < 0) {
    perror("NotifyServiceProvider: can't open /var file");
    return;
  }

  GetLock(LOCK_FOR_WRITE);
  // At this point we have an exclusive lock on the file
  int filenameLength = strlen(filename);
  if(write(fd, filename, filenameLength) != filenameLength) {
    perror("Error writing my PID to /var/run file");
  }
  close(fd);

  // send a signal
  FILE *fp = fopen(PIDfilename, "r");
  if(fp) {
    int pid;

    if(fscanf(fp, "%d", &pid) == 1) {
      kill(pid, NOTIFY_SIG);
    }
    fclose(fp);
  }
  ReleaseLock();
}
