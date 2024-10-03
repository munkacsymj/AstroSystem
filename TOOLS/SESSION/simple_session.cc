/*  simple_session.cc -- Main program to run a night's observing
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
#include <scope_api.h>
#include <camera_api.h>
#include "strategy.h"
#include "session.h"
#include <julian.h>
#include <stdio.h>
#include <gendefs.h>
#include <sys/time.h>
#include <sys/resource.h>

int main(int argc, char **argv) {
  // enable core dumps
  const struct rlimit coresize = { RLIM_INFINITY, RLIM_INFINITY };
  if (setrlimit(RLIMIT_CORE, &coresize)) {
    perror("Error enabling core dumps");
  }

  if(system(COMMAND_DIR "/rebuild_strategy_database")) {
    fprintf(stderr, "Error return from rebuild_strategy_database.\n");
  }

  SessionOptions opts;

  SetDefaultOptions(opts);

  opts.do_focus = 0;
  opts.default_dark_count = 5;
  opts.park_at_end = 0;
  opts.update_mount_model = 1;

  JULIAN now(time(0));

  connect_to_scope();
  connect_to_camera();

  fprintf(stderr, "Turning on mount dual-axis tracking.\n");
  //SetDualAxisTracking(true);

  Session  session(now,
		   argv[1],
		   opts);

  session.execute();
  DisconnectINDI();
  return 0;
}
