/*  convert_coords.cc -- Convert from deg-min-sec to radians
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
#include <unistd.h>		// pick up sleep(), getopt()
#include <stdlib.h>		// pick up atof()
#include <stdio.h>
#include <dec_ra.h>

int main(int argc, char **argv) {
  int conversion_status;

  if(argc != 3) {
    fprintf(stderr, "Usage: convert_coords DD:MM.M HH:MM:SS\n");
    exit(2);
  }

  DEC_RA commanded_pos(argv[1], argv[2], conversion_status);
  if(conversion_status != STATUS_OK) {
    fprintf(stderr, "convert_coords: arguments wouldn't parse.\n");
    exit(2);
  }
    
  printf("Dec = %f, RA = %f\n",
	 commanded_pos.dec(),
	 commanded_pos.ra_radians());
}
  
