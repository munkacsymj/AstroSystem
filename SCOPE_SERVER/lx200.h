/*  lx200.h -- Manages serial link communications with a Gemini/LX200
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
#ifdef __cplusplus
extern "C" {
#endif

void initialize_lx200(void);
int write_mount(int fd, const void *buffer, long buf_len);
int read_mount(int fd, void *buffer, long count);

  // Set this to 1 if you want a logfile to be created containing
  // all comms to/from the mount.
extern int write_log;

#ifdef __cplusplus
}
#endif
