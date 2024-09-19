/*  scope_monitor.cc -- Main program to put up window with Gemini mount info
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
#include <scope_api.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <X11/StringDefs.h>
#include <X11/Intrinsic.h>
#include <X11/Core.h>
#include <Xm/Separator.h>
#include <Xm/RowColumn.h>
#include <Xm/Label.h>
#include <Xm/Form.h>
#include <Xm/ToggleB.h>
#include <Xm/TextF.h>
#include "scope_interface.h"

Widget topLevelW,
       MasterW,
       WormLabel,
       WormW,
       TrackingLabel,
       TrackingW,
       GoToLabel,
       GoToW,
       AlignedLabel,
       AlignedW,
       ModelInUseLabel,
       ModelInUseW,
       RAAlarmLabel,
       RAAlarmW,
       PECLabel,
       PECW,
       RALimitW,
       TimeToLimitW,
       Sep1,
       BottomForm,
       HAW,
       SideOfMountW,
       AZW,
       ELW;
  
void SetWormValue(int value);
void SetTrackingValue(int value);
void SetTimeToLimit(int value);
void SetHourAngle(double hours); // +/- 0-12 hours
void SetAZAngle(double degrees); // +/- 0-180 degrees
void SetELAngle(double degrees); // 0..90 deg
void SetSideOfMount(int side);	// -1 == east, +1 == west
void SetWarning(int warning) ;	// 1=warning, 0=normal
void RefreshData(XtPointer client_data, XtIntervalId *timerid);
void RefreshDataCallback(XtPointer client_data, XtIntervalId *timerid);
void SetGoTo(int slewing_active);
void SetAligned(int aligned);
void SetModelInUse(int model_in_use);
void SetPEC(const ScopeStatus *s);
void SetRAAlarm(int RA_limit_reached);

static Display *d;
static XtAppContext app_context;

int main(int argc, char **argv) {
  Widget TopForm;

  XtSetLanguageProc(NULL, (XtLanguageProc)NULL, NULL);

  MasterW = XtVaAppInitialize(&app_context,
			      "Scope_monitor",
			      NULL, 0,
			      &argc, argv,
			      NULL,
			      NULL);

  topLevelW = XtVaCreateManagedWidget("toplevel",
				      xmRowColumnWidgetClass,
				      MasterW,
				      NULL);

  XtVaCreateManagedWidget("toplabel",
			  xmLabelWidgetClass,
			  topLevelW,
			  NULL);

  Sep1 = XtVaCreateManagedWidget("sep1",
				 xmSeparatorWidgetClass,
				 topLevelW,
				 NULL);

  TopForm = XtVaCreateManagedWidget("topForm",
				    xmFormWidgetClass,
				    topLevelW,
				    XmNfractionBase, 2,
				    NULL);

  WormLabel = XtVaCreateManagedWidget("WormLabel",
				      xmLabelWidgetClass,
				      TopForm,
				      XmNtopAttachment, XmATTACH_FORM,
				      XmNleftAttachment, XmATTACH_FORM,
				      NULL);

  WormW = XtVaCreateManagedWidget("WormValue",
				  xmLabelWidgetClass,
				  TopForm,
				  XmNtopAttachment, XmATTACH_FORM,
				  XmNrightAttachment, XmATTACH_FORM,
				  // XmNleftWidget, WormLabel,
				  NULL);

  TrackingLabel = XtVaCreateManagedWidget("TrackingLabel",
					  xmLabelWidgetClass,
					  TopForm,
					  XmNtopAttachment, XmATTACH_WIDGET,
					  XmNtopWidget, WormLabel,
					  XmNleftAttachment, XmATTACH_FORM,
					  NULL);

  TrackingW = XtVaCreateManagedWidget("TrackingValue",
				      xmLabelWidgetClass,
				      TopForm,
				      XmNtopAttachment, XmATTACH_WIDGET,
				      XmNtopWidget, WormW,
				      XmNrightAttachment, XmATTACH_FORM,
				      NULL);

  GoToLabel = XtVaCreateManagedWidget("GoToLabel",
				      xmLabelWidgetClass,
				      TopForm,
				      XmNtopAttachment, XmATTACH_WIDGET,
				      XmNtopWidget, TrackingLabel,
				      XmNleftAttachment, XmATTACH_FORM,
				      NULL);

  GoToW = XtVaCreateManagedWidget("GoToValue",
				  xmLabelWidgetClass,
				  TopForm,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, TrackingW,
				  XmNrightAttachment, XmATTACH_FORM,
				  NULL);

  AlignedLabel = XtVaCreateManagedWidget("AlignedLabel",
					 xmLabelWidgetClass,
					 TopForm,
					 XmNtopAttachment, XmATTACH_WIDGET,
					 XmNtopWidget, GoToLabel,
					 XmNleftAttachment, XmATTACH_FORM,
					 NULL);

  AlignedW = XtVaCreateManagedWidget("AlignedValue",
				     xmLabelWidgetClass,
				     TopForm,
				     XmNtopAttachment, XmATTACH_WIDGET,
				     XmNtopWidget, GoToW,
				     XmNrightAttachment, XmATTACH_FORM,
				     NULL);

  ModelInUseLabel = XtVaCreateManagedWidget("ModelInUseLabel",
					    xmLabelWidgetClass,
					    TopForm,
					    XmNtopAttachment, XmATTACH_WIDGET,
					    XmNtopWidget, AlignedLabel,
					    XmNleftAttachment, XmATTACH_FORM,
					    NULL);

  ModelInUseW = XtVaCreateManagedWidget("ModelInUseValue",
					xmLabelWidgetClass,
					TopForm,
					XmNtopAttachment, XmATTACH_WIDGET,
					XmNtopWidget, AlignedW,
					XmNrightAttachment, XmATTACH_FORM,
					NULL);

  PECLabel = XtVaCreateManagedWidget("PECLabel",
				     xmLabelWidgetClass,
				     TopForm,
				     XmNtopAttachment, XmATTACH_WIDGET,
				     XmNtopWidget, ModelInUseLabel,
				     XmNleftAttachment, XmATTACH_FORM,
				     NULL);

  PECW = XtVaCreateManagedWidget("PECValue",
				 xmLabelWidgetClass,
				 TopForm,
				 XmNtopAttachment, XmATTACH_WIDGET,
				 XmNtopWidget, ModelInUseW,
				 XmNrightAttachment, XmATTACH_FORM,
				 NULL);

  RAAlarmLabel = XtVaCreateManagedWidget("RAAlarmLabel",
					 xmLabelWidgetClass,
					 TopForm,
					 XmNtopAttachment, XmATTACH_WIDGET,
					 XmNtopWidget, PECLabel,
					 XmNleftAttachment, XmATTACH_FORM,
					 NULL);

  RAAlarmW = XtVaCreateManagedWidget("RAAlarmValue",
				     xmLabelWidgetClass,
				     TopForm,
				     XmNtopAttachment, XmATTACH_WIDGET,
				     XmNtopWidget, PECW,
				     XmNrightAttachment, XmATTACH_FORM,
				     NULL);

  XtVaCreateManagedWidget("sep2",
			  xmSeparatorWidgetClass,
			  topLevelW,
			  NULL);

  TimeToLimitW = XtVaCreateManagedWidget("TimeToLimitLabel",
					 xmLabelWidgetClass,
					 topLevelW,
					 NULL);

  RALimitW = XtVaCreateManagedWidget("RALimitWarning",
				     xmLabelWidgetClass,
				     topLevelW,
				     NULL);

  XtVaCreateManagedWidget("sep3",
			  xmSeparatorWidgetClass,
			  topLevelW,
			  NULL);

  BottomForm = XtVaCreateManagedWidget("bottomForm",
				       xmFormWidgetClass,
				       topLevelW,
				       XmNfractionBase, 2,
				       NULL);

  HAW = XtVaCreateManagedWidget("HALabel",
				xmLabelWidgetClass,
				BottomForm,
				XmNtopAttachment, XmATTACH_FORM,
				XmNleftAttachment, XmATTACH_FORM,
				NULL);

  SideOfMountW = XtVaCreateManagedWidget("SideOfMountValue",
					 xmLabelWidgetClass,
					 BottomForm,
					 XmNtopAttachment, XmATTACH_FORM,
					 XmNrightAttachment, XmATTACH_FORM,
					 NULL);

  AZW = XtVaCreateManagedWidget("AZLabel",
				xmLabelWidgetClass,
				BottomForm,
				XmNtopAttachment, XmATTACH_WIDGET,
				XmNtopWidget, HAW,
				XmNleftAttachment, XmATTACH_FORM,
				NULL);

  ELW = XtVaCreateManagedWidget("ELLabel",
				xmLabelWidgetClass,
				BottomForm,
				XmNtopAttachment, XmATTACH_WIDGET,
				XmNtopWidget, AZW,
				XmNleftAttachment, XmATTACH_FORM,
				NULL);

  SetWormValue(123);
  SetTrackingValue(131);
  SetTimeToLimit(15);
  SetHourAngle(-3.5);
  SetAZAngle(-45.2);
  SetELAngle(85.4);
  SetSideOfMount(-1);
  SetWarning(0);

  (void) XtAppAddTimeOut(app_context,
			 1000,	// 1-second cycle
			 RefreshDataCallback,
			 0);

  connect_to_scope();		// connect I/O to the MI250

  {
    double east_lim, west_lim;
    GetSafetyLimit(&east_lim, &west_lim);
    fprintf(stderr, "eastern safety limit = %lf\n", east_lim);
    fprintf(stderr, "western safety limit = %lf\n", west_lim);
  }

  XtRealizeWidget(MasterW);
  XtAppMainLoop(app_context);
  return 0;
}

static XmString worm_string;
void SetWormValue(int value) {
  static int worm_string_set = 0;
  static char worm_val[80];

  if(worm_string_set) {
    XmStringFree(worm_string);
  }

  worm_string_set = 1;
  sprintf(worm_val, "%04d", value);
  worm_string = XmStringCreateLocalized(worm_val);
  
  XtVaSetValues(WormW, XmNlabelString, worm_string, NULL);
}

static XmString GoTo_string;
void SetGoTo(int slewing_active) {
  static int GoTo_string_set = 0;
  char *GoTo_val;

  if(GoTo_string_set) {
    XmStringFree(GoTo_string);
  }

  GoTo_string_set = 1;
  if(slewing_active) GoTo_val = (char *) "Slewing";
  else               GoTo_val = (char *) "N/A";

  GoTo_string = XmStringCreateLocalized(GoTo_val);
  XtVaSetValues(GoToW, XmNlabelString, GoTo_string, NULL);
}

static XmString Aligned_string;
void SetAligned(int aligned) {
  static int Aligned_string_set = 0;
  char *Aligned_val;

  if(Aligned_string_set) {
    XmStringFree(Aligned_string);
  }

  Aligned_string_set = 1;
  if(aligned) Aligned_val = (char *) "Yes";
  else        Aligned_val = (char *) "No";

  Aligned_string = XmStringCreateLocalized(Aligned_val);
  XtVaSetValues(AlignedW, XmNlabelString, Aligned_string, NULL);
}

static XmString ModelInUse_string;
void SetModelInUse(int model_in_use) {
  static int ModelInUse_string_set = 0;
  char *ModelInUse_val;

  if(ModelInUse_string_set) {
    XmStringFree(ModelInUse_string);
  }

  ModelInUse_string_set = 1;
  if(model_in_use) ModelInUse_val = (char *) "Yes";
  else             ModelInUse_val = (char *) "No";

  ModelInUse_string = XmStringCreateLocalized(ModelInUse_val);
  XtVaSetValues(ModelInUseW, XmNlabelString, ModelInUse_string, NULL);
}

static XmString PEC_string;
void SetPEC(const ScopeStatus *s) {
  static int PEC_string_set = 0;
  char *PEC_val;

  if(PEC_string_set) {
    XmStringFree(PEC_string);
  }

  PEC_string_set = 1;
  if(PECInUse(s))              PEC_val = (char *) "InUse";
  else if(PECDataAvailable(s)) PEC_val = (char *) "DataAvail";
  else                         PEC_val = (char *) "No";

  PEC_string = XmStringCreateLocalized(PEC_val);
  XtVaSetValues(PECW, XmNlabelString, PEC_string, NULL);
}

static XmString RAAlarm_string;
void SetRAAlarm(int RA_limit_reached) {
  static int RAAlarm_string_set = 0;
  char *RAAlarm_val;

  if(RAAlarm_string_set) {
    XmStringFree(RAAlarm_string);
  }

  RAAlarm_string_set = 1;
  if(RA_limit_reached) RAAlarm_val = (char *) "*ALARM*";
  else                 RAAlarm_val = (char *) "Ok";

  RAAlarm_string = XmStringCreateLocalized(RAAlarm_val);
  XtVaSetValues(RAAlarmW, XmNlabelString, RAAlarm_string, NULL);
}

static XmString tracking_string;
void SetTrackingValue(int value) {
  static int tracking_string_set = 0;
  static int previous_value = -1;
  char *tracking_seq;

  if(value == previous_value) return;

  if(tracking_string_set) {
    XmStringFree(tracking_string);
  }

  tracking_string_set = 1;
  if(value == 131) tracking_seq = (char *) "Sidereal";
  else if(value == 132) tracking_seq = (char *) "King Rate";
  else if(value == 133) tracking_seq = (char *) "Lunar";
  else if(value == 134) tracking_seq = (char *) "Solar";
  else if(value == 135) tracking_seq = (char *) "Terrestrial";
  else if(value == 136) tracking_seq = (char *) "Closed loop";
  else if(value == 137) tracking_seq = (char *) "User Def";
  else tracking_seq = (char *) "unknown";

  tracking_string = XmStringCreateLocalized(tracking_seq);

  XtVaSetValues(TrackingW, XmNlabelString, tracking_string, NULL);
}

static XmString limit_time_string;
void SetTimeToLimit(int value) {
  static int limit_string_set = 0;
  static char limit_val[80];

  if(limit_string_set) {
    XmStringFree(limit_time_string);
  }

  limit_string_set = 1;
  sprintf(limit_val, "Time to limit: %d mins", value);
  limit_time_string = XmStringCreateLocalized(limit_val);
  
  XtVaSetValues(TimeToLimitW, XmNlabelString, limit_time_string, NULL);
}

static XmString hour_angle_string;
void SetHourAngle(double hours) { // +/- 0-12 hours
  static int ha_string_set = 0;
  static char ha_val[80];

  if(ha_string_set) {
    XmStringFree(hour_angle_string);
  }

  ha_string_set = 1;

  {
    int hour_int;
    int min_int;
    char sign;

    if(hours < 0.0) {
      sign = '-';
      hours = -hours;
    } else sign = '+';

    hour_int = (int) hours;
    min_int = (int) ((hours - hour_int) * 60.0 + 0.5);

    sprintf(ha_val, "HA: %c%02dh%02dm", sign, hour_int, min_int);
  }
  
  hour_angle_string = XmStringCreateLocalized(ha_val);
  
  XtVaSetValues(HAW, XmNlabelString, hour_angle_string, NULL);
}

static XmString az_string;
void SetAZAngle(double degrees) { // +/- 0-180 degrees
  static int az_string_set = 0;
  static char az_val[80];

  if(az_string_set) {
    XmStringFree(az_string);
  }

  az_string_set = 1;

  sprintf(az_val, "AZ: %+03d deg", (int) degrees);
  
  az_string = XmStringCreateLocalized(az_val);
  
  XtVaSetValues(AZW, XmNlabelString, az_string, NULL);
}

static XmString el_string;
void SetELAngle(double degrees) { // 0..90 deg
  static int el_string_set = 0;
  static char el_val[80];

  if(el_string_set) {
    XmStringFree(el_string);
  }

  el_string_set = 1;

  sprintf(el_val, "EL:  %02d deg", (int) degrees);
  
  el_string = XmStringCreateLocalized(el_val);
  
  XtVaSetValues(ELW, XmNlabelString, el_string, NULL);
}

static XmString mount_side_string;
void SetSideOfMount(int side) { // -1 == east, +1 == west
  static int side_string_set = 0;
  static int previous_value = 0;
  char *s;

  if(previous_value == side) return; // do nothing
  previous_value = side;

  if(side_string_set) {
    XmStringFree(mount_side_string);
  }
  side_string_set = 1;

  if(side < 0) s = (char *) "East";
  else s = (char *) "West";
  
  mount_side_string = XmStringCreateLocalized(s);
  
  XtVaSetValues(SideOfMountW, XmNlabelString, mount_side_string, NULL);
}

static XmString nowarning_string;
static XmString warning_string;
void SetWarning(int warning) { // 1=warning, 0=normal
  static int setup = 0;
  static int previous_value = 99;

  if(warning == previous_value) return;

  if(!setup) {
    setup = 1;
    nowarning_string = XmStringCreateLocalized((char *) "");
    warning_string   = XmStringCreateLocalized((char *) "NEAR LIMIT");
  }

  previous_value = warning;
  XtVaSetValues(RALimitW, XmNlabelString,
		(warning ? warning_string : nowarning_string), NULL);
}

void RefreshDataCallback(XtPointer client_data, XtIntervalId *timerid) {
  RefreshData(client_data, timerid);
  (void) XtAppAddTimeOut(app_context,
			 1000,	// 1-second cycle
			 RefreshDataCallback,
			 0);
}

void RefreshData(XtPointer client_data, XtIntervalId *timerid) {
  static int cycle = 0;

  cycle = (cycle+1) % 5;

  // things done once/second
  SetWormValue(GetWormValue());
  SetHourAngle(GetHourAngle());
  SetELAngle(GetElevationAngle());
  SetAZAngle(GetAZAngle());
  SetHourAngle(GetHourAngle());

  // things done once/5-seconds
  if(cycle == 0) {
    ScopeStatus *status = CreateScopeStatus();
    
    SetTrackingValue(GetTrackingValue());
    SetSideOfMount(GetSideOfMount());

    if(status) {
      SetGoTo(GetGoToValue(status));
      SetAligned(GetAlignedValue(status));
      SetModelInUse(GetModelInUse(status));
      SetPEC(status);
      SetRAAlarm(GetRAAlarm(status));

      DeleteScopeStatus(status);
    }
  }
}
