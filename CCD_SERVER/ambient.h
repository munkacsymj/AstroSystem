/* This may look like C code, but it is really -*-c++-*- */
/*  ambient.h -- Provides ambient camera case temperature
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

#ifndef _AMBIENT_H
#define _AMBIENT_H

void AmbientInitialize(void);
bool AmbientTempAvail(void);
double AmbientCurrentDegC(void);

#endif
