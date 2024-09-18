/* This may look like C code, but it is really -*-c++-*- */
/*  image_notify.h -- Interprocess communication to notify process
 *  when new image is available from the camera
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
#ifndef _IMAGE_NOTIFY_H
#define _IMAGE_NOTIFY_H
#include <X11/Intrinsic.h>

/****************************************************************/
/*        Service-provider calls				*/
/* (The service-provider *receives* the notification.)		*/
/****************************************************************/

// This call registers the process executing the call as a service
// provider that needs to be notified whenever a library user sends a
// "NofityServiceProvider()" call.
void RegisterAsProvider(XtAppContext context,
			void (*callback)(char *));
// RegisterAsProviderRaw() does the same thing, but instead of
// generating an X11 Xt-friendly callback, this will call the
// "callback" function in the context of a Unix signal handler. Do NOT
// interact with X11 or Gtk from within that signal handler, except to
// generate an X11 or Gtk signal.
void RegisterAsProviderRaw(void (*callback)(char *));
char *ProvideCurrentFilename(void);

/****************************************************************/
/*        Library-user calls					*/
/* (The library-user *generates* the notification.)		*/
/****************************************************************/

void NotifyServiceProvider(const char *filename);

#endif

