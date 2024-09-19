/* This may look like C code, but it is really -*-c++-*- */
/*  usb_cfw.h -- Handle the QHYCFW3 when connected via USB
 *
 *  Copyright (C) 2021 Mark J. Munkacsy

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

#ifndef _USB_CFW_H
#define _USB_CFW_H

void USBCFWInitializeStart(void);

// This is the only blocking call to usb_cfw. It will block until
// initialization is complete. It's okay to call this multiple times. 
int USBCFWInitializeEnd(void); // returns filter_count
bool USBCFWInitializationComplete(void);

// This initiates the motion and is non-blocking
void USBMoveFilterWheel(int position);

// This is a non-blocking test of the current CFW position. It's up to
// the caller to implement timing/blocking/... when repositioning the
// CFW. 
int USBCFWCurrentPosition(void);

#endif
