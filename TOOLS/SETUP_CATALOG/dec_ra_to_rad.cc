/*  dec_ra_to_rad.cc -- converts dec/ra in deg/min/sec to radians
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
#include <dec_ra.h>

void bad(void) {
  fprintf(stderr, "Bad format. Need: DEC RA, (e.g. 38:14.5 01:01:01.2)\n");
}

int main(int argc, char **argv) {
  char buffer[120];

  fprintf(stderr, "Enter dec/ra pairs, DEC first RA second.\n");
  while(fgets(buffer, sizeof(buffer), stdin)) {

    char string1[sizeof(buffer)];
    char string2[sizeof(buffer)];

    int num_string = sscanf(buffer, "%s %s", string1, string2);
    if(num_string != 2) {
      bad();
    } else {
    
      int status;
      DEC_RA pos(string1, string2, status);

      if(status != STATUS_OK) {
	bad();
      } else {
	fprintf(stdout, "Dec = %f, RA = %f\n",
		pos.dec(), pos.ra_radians());
      }
    }
  }
}
