/*  scope_interface.cc -- Helper functions for scope_status program
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
#include <string.h>
#include "scope_interface.h"
#include <scope_api.h>

void scope_error(char *response, ScopeResponseStatus Status) {
  const char *type = "<invalid>";

  if(Status == Okay) type = "Okay";
  if(Status == TimeOut) type = "TimeOut";
  if(Status == Aborted) type = "Aborted";

  fprintf(stderr, "ERROR: %s, string = '%s'\n", type, response);
}

/****************************************************************/
/*        Create & DeleteScopeStatus				*/
/****************************************************************/
struct ScopeStatus {
  int ScopeStatusWord;
  int PECStatusWord;
};

ScopeStatus *CreateScopeStatus(void) {
  char message[32];
  char response[32];
  ScopeResponseStatus status;
  ScopeStatus *scopeStatusWord = new ScopeStatus;

  BuildMI250Command(message,
		    MI250_GET,
		    99);	// Get status word value
  if(scope_message(message,
		   RunFast,
		   StringResponse,
		   response,
		   sizeof(response),
		   &status)) {
    fprintf(stderr, "scope_interface.cc: error quering for status word\n");
    return 0;
  }
  if(sscanf(response, "%d", &scopeStatusWord->ScopeStatusWord) != 1) {
    fprintf(stderr,
	    "scope_interface.cc: CreateScopeStatus cannot parse scope response: '%s'\n",
	    response);
    return 0;
  }

  BuildMI250Command(message,
		    MI250_GET,
		    509);
  if(scope_message(message,
		   RunFast,
		   StringResponse,
		   response,
		   sizeof(response),
		   &status)) {
    fprintf(stderr, "scope_interface.cc: error querying for PEC word\n");
    return 0;
  }
  if(sscanf(response, "%d", &scopeStatusWord->PECStatusWord) != 1) {
    fprintf(stderr, "scope_interface.cc: CreateScopeStatus cannot parse scope PEC response '%s'\n", response);
    return 0;
  }
  
  return scopeStatusWord;
}

void DeleteScopeStatus(ScopeStatus *s) {
  delete s;
}

/****************************************************************/
/*        GetGoToValue(const ScopeStatus *s)			*/
/****************************************************************/
int GetGoToValue(const ScopeStatus *s) {// 1=slewing, 0=not-slewing
  return ((s->ScopeStatusWord) & 0x08) ? 1 : 0;
}
/****************************************************************/
/*        GetAlignedValue(const ScopeStatus *s)			*/
/****************************************************************/
int GetAlignedValue(const ScopeStatus *s) {  // 1=aligned,0=not
  return ((s->ScopeStatusWord) & 0x01) ? 1 : 0;
}
/****************************************************************/
/*        GetModelInUse(const ScopeStatus *s)			*/
/****************************************************************/
int GetModelInUse(const ScopeStatus *s) {   // 1=model-in-use
  return ((s->ScopeStatusWord) & 0x02) ? 1 : 0;
}
/****************************************************************/
/*        GetRAAlarm(const ScopeStatus *s)			*/
/****************************************************************/
int GetRAAlarm(const ScopeStatus *s) {  // 1=RA limit reached
  return ((s->ScopeStatusWord) & 0x10) ? 1 : 0;
}
/****************************************************************/
/*        GetPECDataAvailable(const ScopeStatus *s)		*/
/****************************************************************/
int PECDataAvailable(const ScopeStatus *s) { // 1=PEC data avail
  return ((s->PECStatusWord) & 0x20) ? 1 : 0;
}
/****************************************************************/
/*        GetPECInUse(const ScopeStatus *s)			*/
/****************************************************************/
int PECInUse(const ScopeStatus *s) { // 1=PEC active
  return ((s->PECStatusWord) & 0x01) ? 1 : 0;
}

/****************************************************************/
/*        GetWormValue(void)					*/
/****************************************************************/
int GetWormValue(void) {
  char message[32];
  char response[32];
  ScopeResponseStatus status;
  long worm_position;

  BuildMI250Command(message,
		    MI250_GET,
		    501);	// Get PEC value
  if(scope_message(message,
		   RunFast,
		   StringResponse,
		   response,
		   sizeof(response),
		   &status)) {
    fprintf(stderr, "scope_interface.cc: error quering for worm position\n");
    return -1;
  }
  if(sscanf(response, "%ld", &worm_position) != 1) {
    fprintf(stderr,
	    "scope_interface.cc: PEC_Position cannot parse scope response: '%s'\n",
	    response);
    return -1;
  } else {
    return worm_position;
  }
  /*NOTREACHED*/
}

/****************************************************************/
/*        GetTrackingValue(void)				*/
/****************************************************************/
int GetTrackingValue(void) {
  char message[32];
  char response[32];
  ScopeResponseStatus status;
  long tracking_value;

  BuildMI250Command(message,
		    MI250_GET,
		    130);	// Get tracking value
  if(scope_message(message,
		   RunFast,
		   StringResponse,
		   response,
		   strlen(response),
		   &status)) {
    fprintf(stderr, "scope_interface.cc: error quering for tracking value\n");
    return -1;
  }
  if(sscanf(response, "%ld", &tracking_value) != 1) {
    fprintf(stderr,
	    "scope_interface.cc: GetTrackingValue cannot parse scope response: '%s'\n",
	    response);
    return -1;
  } else {
    return tracking_value;
  }
  /*NOTREACHED*/
}

/****************************************************************/
/*        GetHourAngle(void)					*/
/****************************************************************/
double GetHourAngle(void) {	// returns HA in hours
  char response[32];
  ScopeResponseStatus status;
  int raw_ha_hours, raw_ha_min, raw_ha_sec;

  if(scope_message(":GH#",	// Get HA
		   RunFast,
		   StringResponse,
		   response,
		   strlen(response),
		   &status)) {
    fprintf(stderr, "scope_interface.cc: error quering for HA\n");
    return -1;
  }
  if(sscanf(response, "%d:%d:%d",
	    &raw_ha_hours, &raw_ha_min, &raw_ha_sec) != 3) {
    fprintf(stderr,
	    "scope_interface.cc: GetHourAngle cannot parse scope response: '%s'\n",
	    response);
    return -1;
  } else {
    int negative = (response[0] == '-');
    if(negative) raw_ha_hours = -raw_ha_hours;

    
    return (negative ? -1 : 1)*
      (raw_ha_hours + raw_ha_min/60.0 + raw_ha_sec/3600.0);
  }
  /*NOTREACHED*/
}

/****************************************************************/
/*        GetElevationAngle(void)				*/
/****************************************************************/
double GetElevationAngle(void) {	// returns EL in degrees
  char response[32];
  ScopeResponseStatus status;
  int raw_alt_deg, raw_alt_min, raw_alt_sec;

  if(scope_message(":GA#",	// Get Altitude
		   RunFast,
		   StringResponse,
		   response,
		   strlen(response),
		   &status)) {
    fprintf(stderr, "scope_interface.cc: error quering for ALT\n");
    return -1;
  }
  if(sscanf(response, "%d:%d:%d",
	    &raw_alt_deg, &raw_alt_min, &raw_alt_sec) != 3) {
    fprintf(stderr,
	    "scope_interface.cc: GetHourAngle cannot parse scope response: '%s'\n",
	    response);
    return -1;
  } else {
    int negative = (response[0] == '-');
    if(negative) raw_alt_deg = -raw_alt_deg;

    
    return (negative ? -1 : 1)*
      (raw_alt_deg + raw_alt_min/60.0 + raw_alt_sec/3600.0);
  }
  /*NOTREACHED*/
}

/****************************************************************/
/*        GetAZAngle(void)					*/
/****************************************************************/
double GetAZAngle(void) {	// returns AZ in degrees (0..360)
  char response[32];
  ScopeResponseStatus status;
  int raw_az_deg, raw_az_min, raw_az_sec;

  if(scope_message(":GZ#",	// Get Altitude
		   RunFast,
		   StringResponse,
		   response,
		   strlen(response),
		   &status)) {
    fprintf(stderr, "scope_interface.cc: error quering for AZ\n");
    return -1;
  }
  if(sscanf(response, "%d:%d:%d",
	    &raw_az_deg, &raw_az_min, &raw_az_sec) != 3) {
    fprintf(stderr,
	    "scope_interface.cc: GetHourAngle cannot parse scope response: '%s'\n",
	    response);
    return -1;
  } else {
    return (raw_az_deg + raw_az_min/60.0 + raw_az_sec/3600.0);
  }
  /*NOTREACHED*/
}

/****************************************************************/
/*        GetSideOfMount(void)					*/
/****************************************************************/
int    GetSideOfMount(void) {	// -1 == east, +1 == west
  return (scope_on_west_side_of_pier() ? 1 : -1);
}

/****************************************************************/
/*        GetSafetyLimit()					*/
/****************************************************************/
void GetSafetyLimit(double *eastern_limit,
		    double *western_limit) { // angles in degrees
  char message[32];
  char response[32];
  ScopeResponseStatus status;
  int west_deg, west_min, east_deg, east_min;

  BuildMI250Command(message,
		    MI250_GET,
		    220);	// Get PEC value
  if(scope_message(message,
		   RunFast,
		   StringResponse,
		   response,
		   strlen(response),
		   &status)) {
    fprintf(stderr, "scope_interface.cc: error quering for worm position\n");
    return;
  }
  if(sscanf(response, "%dd%d;%dd%d",
	    &east_deg, &east_min, &west_deg, &west_min) != 4) {
    fprintf(stderr,
	    "scope_interface.cc: GetSafetyLimit cannot parse scope response: '%s'\n",
	    response);
    return;
  } else {
    // force conversion to double and decimal degrees. All angles positive?
    *eastern_limit = east_deg + (east_min/60.0);
    *western_limit = west_deg + (west_min/60.0);
  }
  /*NOTREACHED*/
}
