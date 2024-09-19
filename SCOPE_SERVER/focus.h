/*  focus.h -- Manages the focus motor on the mount
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
#define DIRECTION_IN 1
#define DIRECTION_OUT 0
#define NO_DIRECTION_MOVE_ABSOLUTE 2

#ifdef __cplusplus
extern "C" {
#endif
  
<<<<<<< focus.h
  //extern int focus_fd;		/* file descriptor */
  void c14focus(int direction, unsigned long duration);
  void c14focus_move(int direction,
		     unsigned long total_duration,
		     unsigned long step_size);
=======
//extern int focus_fd;		/* file descriptor */
void c14focus(int direction, unsigned long duration);
void c14focus_move(int direction,
		   unsigned long total_duration,
		   unsigned long step_size);
>>>>>>> 1.5

<<<<<<< focus.h
  long c14cum_focus_position(void);

  void esattofocus(int direction, unsigned long duration);
  long esattocum_focus_position(void);
=======
long c14cum_focus_position(void);

void esattofocus(int direction, unsigned long duration);
long esattocum_focus_position(void);
>>>>>>> 1.5

#ifdef __cplusplus
}
#endif
