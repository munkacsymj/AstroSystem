/* This may look like C code, but it is really -*-c++-*- */
/*  scope_interface.h -- helper functions for scope_status
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
#ifndef _SCOPE_INTERFACE_H
#define _SCOPE_INTERFACE_H

int GetWormValue(void);
int GetTrackingValue(void);
double GetHourAngle(void);	// returns HA in hours
double GetElevationAngle(void);	// returns EL in degrees
double GetAZAngle(void);	// returns AZ in degrees (0..360)
int    GetSideOfMount(void);	// -1 == east, +1 == west
void GetSafetyLimit(double *eastern_limit,
		    double *western_limit); // angles in degrees

struct ScopeStatus;
ScopeStatus *CreateScopeStatus(void);
void DeleteScopeStatus(ScopeStatus *s);

int GetGoToValue(const ScopeStatus *s);	    // 1=slewing, 0=not-slewing
int GetAlignedValue(const ScopeStatus *s);  // 1=aligned,0=not
int GetModelInUse(const ScopeStatus *s);    // 1=model-in-use
int GetRAAlarm(const ScopeStatus *s);       // 1=RA limit reached
int PECDataAvailable(const ScopeStatus *s); // 1=PEC data avail
int PECInUse(const ScopeStatus *s);         // 1=PEC active

#endif
