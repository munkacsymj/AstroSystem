/*  notify.cc -- Main program to send quit commands to other programs
 *
 *  Copyright (C) 2015 Mark J. Munkacsy
 *
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
#include <stdlib.h>
#include <proc_messages.h>
#include <string.h>		// strcmp()

void usage(void) {
  fprintf(stderr, "usage: notify prog_name -l|quit|pause|resume\n");
  exit(-2);
}

int main(int argc, char **argv) {

  if (argc == 2 && strcmp(argv[1], "-l") == 0) {
    ProcessList *pl = GetProcessList();
    ProcessList::iterator it;

    for (it = pl->begin(); it != pl->end(); it++) {
      fprintf(stderr, "%s\n", *it);
    }
    return 0;
  } else if (argc != 3) {
    usage();
  }

  if (strcmp(argv[2], "quit") == 0) {
    if (SendMessage(argv[1], SM_ID_Abort) == 0) {
      printf("notify: message sent.\n");
    }
  } else if (strcmp(argv[2], "pause") == 0) {
    if (SendMessage(argv[1], SM_ID_Pause) == 0) {
      printf("notify: message sent.\n");
    }
  } else if (strcmp(argv[2], "resume") == 0) {
    if (SendMessage(argv[1], SM_ID_Resume) == 0) {
      printf("notify: message sent.\n");
    }
  } else {
    fprintf(stderr, "notify: illegal notification: %s\n", argv[2]);
    usage();
  }

  return 0;
}
