/*  ccd_message_handler.h -- Server handles message sent for camera control
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
#ifndef _CCD_MESSAGE_HANDLER_H
#define _CCD_MESSAGE_HANDLER_H

void initialize_ccd(void);
int handle_message(int socket_fd);

void MoveFilterWheel(int position);
void LogTag(const char *message);
double GetCurrentChipTemp(void);
double GetCurrentCoolerPWM(void);

// Pair of functions to mediate cross-thread access to the camera
// "owner" is an integer identifying the thread that is grabbing the
// lock. It is *not* an error to Lock multiple times from the same thread.
void GetCameraLock(void);
void ReleaseCameraLock(void);
void CameraLockInit(void);

#endif
