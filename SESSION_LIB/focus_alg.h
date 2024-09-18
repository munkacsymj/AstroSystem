/* This may look like C code, but it is really -*-c++-*- */
/*  focus_alg.h -- operations to manage focus using measured bluring
 *  of star images 
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
#ifndef _FOCUS_ALG_H
#define _FOCUS_ALG_H

#include <julian.h>
#include "session.h"

// This call must be the first invocation to focus_alg.  If the file
// is not empty, it will be read to discover the current state of the
// focus algorithms.  If the file doesn't exist, it will be created
// and the focus algorithm will initialize itself. If the file exists
// but does not contain a focus state, an error message will be
// generated and a zero will be returned.  If everything went okay, a
// +1 will be returned.
int
SetFocusStateFile(const char *filename);

void
AddImageToFocus(JULIAN julian,
		double temp_deg_C,
		Session *session,
		const char *filename,
		const char *darkfile,
		const char *flatfile,
		double *x_blur = 0,
		double *y_blur = 0);

void
AddImageToFocus(JULIAN julian, // exposure time
		Session *session,
		double temp_deg_C,
		double measure_weight,
		long   focuser_ticks,
		double blur_measure);

void
SlewNotify(JULIAN julian);

void
focus_analyze(Session *session); 

#endif
		   
