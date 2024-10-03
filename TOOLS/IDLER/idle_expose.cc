/*  idle_expose.cc -- Keep camera doing things in the background
 *
 *  Copyright (C) 2023 Mark J. Munkacsy
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
#include "camera_api.h"
#include "scope_api.h"

void usage(void) {
  fprintf(stderr, "Usage: idle_expose\n");
  exit(-2);
}

int main(int argc, char **argv) {
  connect_to_camera();
  connect_to_scope();
  exposure_flags flags("photometry");

  while(1) {
    fprintf(stderr, "idle_expose: sleeping\n");
    sleep(60);
    fprintf(stderr, "idle_expose: exposure active\n");

    (void) expose_image(10.0, flags, "IDLE");
  }

  DisconnectINDI();
  return 0;
}
    
