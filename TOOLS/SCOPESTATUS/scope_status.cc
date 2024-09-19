/*  scope_status.cc -- Test program supporting scope_monitor program
 *
 *  Copyright (C) 2007 Mark J. Munkacsy
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
#include "scope_api.h"

void scope_error(char *response, ScopeResponseStatus Status) {
  const char *type = "<invalid>";

  if(Status == Okay) type = "Okay";
  if(Status == TimeOut) type = "TimeOut";
  if(Status == Aborted) type = "Aborted";

  fprintf(stderr, "ERROR: %s, string = '%s'\n", type, response);
}

int main(int argc, char **argv) {
  char response[64];
  ScopeResponseStatus Status;

  connect_to_scope();

  if(scope_message("\006",
		   RunFast,
		   FixedLength,
		   response,
		   1,
		   &Status)) {
    scope_error(response, Status);
  } else {
    fprintf(stdout, "Alignment mode = %s\n", response);
  }

  if(scope_message(":GR#",
		   RunFast,
		   StringResponse,
		   response,
		   0,
		   &Status)) {
    scope_error(response, Status);
  } else {
    fprintf(stdout, "RA = %s\n", response);
  }

  if(scope_message(":GD#",
		   RunFast,
		   StringResponse,
		   response,
		   0,
		   &Status)) {
    scope_error(response, Status);
  } else {
    fprintf(stdout, "Dec = %s\n", response);
  }

  if(scope_message(":GA#",
		   RunFast,
		   StringResponse,
		   response,
		   0,
		   &Status)) {
    scope_error(response, Status);
  } else {
    fprintf(stdout, "Alt = %s\n", response);
  }

  if(scope_message(":P#",
		   RunFast,
		   StringResponse,
		   response,
		   0,
		   &Status)) {
    scope_error(response, Status);
  } else {
    fprintf(stderr, "Pointing mode changed to %s\n", response);
  }

  if(scope_message(":GZ#",
		   RunFast,
		   StringResponse,
		   response,
		   0,
		   &Status)) {
    scope_error(response, Status);
  } else {
    fprintf(stdout, "Az = %s\n", response);
  }


}
