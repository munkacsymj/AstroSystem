/*  graph_star.cc -- used to graph the lightcurve of a star
 *  in the observation database
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
#include <stdlib.h>		// free()
#include <stdio.h>
#include <unistd.h>		// getopt()
#include <ctype.h>		// isspace()
#include <string.h>		// strcmp()

#include <X11/Intrinsic.h>
#include <X11/Xcms.h>
#include <X11/cursorfont.h>
#include <X11/StringDefs.h>
#include <X11/Core.h>

#include <Xm/Xm.h>
#include <Xm/Label.h>
#include <Xm/Scale.h>
#include <Xm/TextF.h>
#include <Xm/Text.h>
#include <Xm/RowColumn.h>
#include <Xm/PushB.h>
#include <Xm/ToggleB.h>
#include <Xm/Separator.h>
#include <Xm/ScrolledW.h>
#include <Xm/DrawingA.h>
#include <Xm/Form.h>

#include <Image.h>

void ShowBusy(void);
void ShowReady(void);
void ReadAllData(void);
void StarDataRefresh(void);
void InitializeGraphics(void);
////////////////////////////////////////////////////////////////
//        CALLBACK DECLARATIONS
////////////////////////////////////////////////////////////////
void RedrawAllWindows(void);

Widget toplevel,		// top level
    manager,			// top-level manager
    LeftSide,			// left-side RowColumn
    RefreshButton,		// Refresh button
    ProgressLabel,
    StarNameLabel,
    StarTextWidget,
    GraphCanvas,		// graph drawing area
    ExitButton;			// Exit button

Display *display;
// Screen *screen;
XtAppContext context;

const static int pixmap_depth = 8;

void DwgClickEvent(Widget W,
		   XtPointer client_data,
		   XEvent *event,
		   Boolean *continue_dispatch);
void DwgExposeCallback(Widget W, XtPointer client_data, XtPointer call_data);
void ExitCallback(Widget W, XtPointer Client_Data, XtPointer call_data);
void NextCallback(Widget W, XtPointer Client_Data, XtPointer call_data);
void RefreshImage(Widget W, XtPointer Client_Data, XtPointer call_data);

//
// For X properties, see /usr/lib/X11/app-defaults/graph_star
//

int
main(int argc, char **argv) {
  int option_char;

  while((option_char = getopt(argc, argv, "")) > 0) {
    switch (option_char) {
    case '?':			// invalid argument
    default:
      fprintf(stderr, "Invalid argument.\n");
      exit(2);
    }
  }

  toplevel = XtAppInitialize(&context, "graph_star",
			     NULL, 0, &argc, argv, NULL, NULL, 0);

  display = XtDisplay(toplevel);
  // screen  = DefaultScreen(display);

  manager = XtVaCreateManagedWidget("manager",
				    xmRowColumnWidgetClass, toplevel,
				    XmNorientation, XmHORIZONTAL,
				    NULL);

  ////////////////////////////////////////////////////////////////
  //        LEFT SIDE STUFF
  ////////////////////////////////////////////////////////////////

  LeftSide = XtVaCreateManagedWidget("LeftSide",
				     xmRowColumnWidgetClass, manager,
				     XmNorientation, XmVERTICAL,
				     NULL);

  //  StarTextWidget = XmCreateScrolledText(manager,
  //					"StarText",
  //					0, 0);
  Widget ScrollArea = XtVaCreateManagedWidget("ScrollArea",
					      xmScrolledWindowWidgetClass,
					      manager,
					      XmNscrollingPolicy, XmAUTOMATIC,
					      XmNvisualPolicy, XmCONSTANT,
					      NULL);

  StarTextWidget = XtVaCreateManagedWidget("StarText",
					   xmTextWidgetClass,
					   ScrollArea,
					   NULL);

  Widget TopButtons = XtVaCreateManagedWidget("TopButtons",
					      xmRowColumnWidgetClass,
					      LeftSide,
					      XmNorientation, XmHORIZONTAL,
					      NULL);

  GraphCanvas = XtVaCreateManagedWidget("GraphCanvas",
					xmDrawingAreaWidgetClass, LeftSide,
					XmNresizePolicy, XmRESIZE_NONE,
					NULL);

  Widget BottomButtons = XtVaCreateManagedWidget("BottomButtons",
						 xmRowColumnWidgetClass,
						 LeftSide,
						 XmNorientation, XmHORIZONTAL,
						 NULL);

  Widget NextButton = XtVaCreateManagedWidget("NextButton",
					      xmPushButtonWidgetClass,
					      BottomButtons,
					      XmNwidth, 30,
					      NULL);
  XtAddCallback(NextButton, XmNactivateCallback, NextCallback, NULL);
						 

  ExitButton =
    XtVaCreateManagedWidget("ExitButton",
			    xmPushButtonWidgetClass, TopButtons,
			    XmNwidth, 30,
			    NULL);
  XtAddCallback(ExitButton, XmNactivateCallback, ExitCallback, NULL);

  ProgressLabel = XtVaCreateManagedWidget("ProgressLabel",
					  xmLabelWidgetClass,
					  TopButtons,
					  XmNwidth, 40,
					  NULL);

  StarNameLabel = XtVaCreateManagedWidget("StarNameLabel",
					  xmLabelWidgetClass,
					  TopButtons,
					  XmNwidth, 150,
					  NULL);

  XtRealizeWidget(toplevel);

  InitializeGraphics();
  ReadAllData();

  NextCallback(0, 0, 0);
  StarDataRefresh();

  XtAppMainLoop(context);
}

////////////////////////////////////////////////////////////////
//        CALLBACKS
////////////////////////////////////////////////////////////////

/****************************************************************/
/*        Exit Callback						*/
/****************************************************************/
void ExitCallback(Widget W, XtPointer Client_Data, XtPointer call_data) {
  XtAppSetExitFlag(context);
}

class InputLine {
public:
  InputLine(char *input);
  ~InputLine(void) {
    delete [] Keywords;
    delete [] Values;
  }

  int NumPairs;

  char *PairKeyword(int i) { return Keywords[i]; }
  char *PairValue(int i) { return Values[i]; }

private:
  char **Keywords;
  char **Values;
};

InputLine::InputLine(char *input) {
  int field_count = 0;
  char *s;

  for(s = input; *s; s++) {
    if(*s == '=') field_count++;
    if(isalpha(*s) && islower(*s)) *s = toupper(*s);
  }

  NumPairs = field_count;
  Keywords = new char *[field_count];
  Values   = new char *[field_count];

  field_count = 0;
  s = input;
  while(field_count < NumPairs) {
    while(isspace(*s)) s++;
    Keywords[field_count] = s;
    while(*s && *s != '=') s++;
    if(*s != '=') {
      fprintf(stderr, "graph_star: parse error: %s\n", input);
      return;
    }
    *s++ = 0;

    Values[field_count] = s;
    while(*s && !isspace(*s)) s++;
    *s++ = 0;
    field_count++;
  }
}
      
struct one_obs {
  struct one_obs *next;

  struct StarData *star;
  double obs_date;
  double obs_magnitude;
} *head_obs = 0;

struct StarData {
  struct StarData *next;

  struct one_obs **obs_list;
  int             num_obs;
  int             obs_so_far;
  int             already_done;
  char           *starname;
} *head_star = 0;

StarData **star_array = 0;
int total_num_stars = 0;
int num_stars_remaining;
int total_num_obs = 0;
struct one_obs **obs_array = 0;
int current_selected_star = -1;

GC gc_black, gc_red, gc_blue;

void InitializeGraphics(void) {
  Pixel red_pixel;
  Pixel blue_pixel;
  Colormap cmap;
  XColor temp_color;

  XtVaGetValues(GraphCanvas, XtNcolormap, &cmap, NULL);
  XAllocNamedColor(XtDisplay(GraphCanvas),
		   cmap,
		   "red",
		   &temp_color, &temp_color);
  red_pixel = temp_color.pixel;
  XAllocNamedColor(XtDisplay(GraphCanvas),
		   cmap,
		   "blue",
		   &temp_color, &temp_color);
  blue_pixel = temp_color.pixel;

  gc_black = XCreateGC(XtDisplay(GraphCanvas), XtWindow(GraphCanvas),
		       0, 0);
  XGCValues gc_values;
  gc_values.foreground = red_pixel;
  gc_red = XCreateGC(XtDisplay(GraphCanvas), XtWindow(GraphCanvas),
		     GCForeground, &gc_values);

  gc_values.foreground = blue_pixel;
  gc_blue = XCreateGC(XtDisplay(GraphCanvas), XtWindow(GraphCanvas),
		      GCForeground, &gc_values);
}

/*****************************************************************
  DrawGraph()
  ****************************************************************/

const int G_margin_T = 6;
const int G_margin_B = 26;
const int G_margin_L = 40;
const int G_margin_R = 6;

double days_per_pixel;
double mag_per_pixel;
double time_first, time_last, mag_dim, mag_bright;

int CanvasX(double o_time) {
  return G_margin_L + (int) (0.5 + (o_time - time_first)/days_per_pixel);
}

int CanvasY(double o_mag) {
  return G_margin_T + (int) (0.5 + (o_mag - mag_bright)/mag_per_pixel);
}

void DrawGraph(StarData *star) {
  Dimension G_width, G_height;

  // clear the canvas
  XClearWindow(XtDisplay(GraphCanvas), XtWindow(GraphCanvas));

  XtVaGetValues(GraphCanvas,
		XmNwidth, &G_width,
		XmNheight, &G_height,
		NULL);

  time_first = star->obs_list[0]->obs_date;
  time_last  = star->obs_list[0]->obs_date;
  mag_dim    = star->obs_list[0]->obs_magnitude;
  mag_bright = star->obs_list[0]->obs_magnitude;

  int i;
  for(i=1; i<star->num_obs; i++) {
    if(star->obs_list[i]->obs_date < time_first)
      time_first = star->obs_list[i]->obs_date;
    if(star->obs_list[i]->obs_date > time_last)
      time_last  = star->obs_list[i]->obs_date;
    if(star->obs_list[i]->obs_magnitude > mag_dim)
      mag_dim = star->obs_list[i]->obs_magnitude;
    if(star->obs_list[i]->obs_magnitude < mag_bright)
      mag_bright = star->obs_list[i]->obs_magnitude;
  }

  days_per_pixel = (time_last - time_first)/(G_width-G_margin_L-G_margin_R);
  double del_mag = (mag_dim - mag_bright);
  // expand magnitude range by 10%
  mag_dim += (del_mag/10.0);
  mag_bright -= (del_mag/10.0);
  del_mag = (mag_dim - mag_bright);
  if(del_mag < 2.0) {
    double shortcoming = 2.0 - del_mag;
    mag_dim += shortcoming/2.0;
    mag_bright -= shortcoming/2.0;
    del_mag = (mag_dim - mag_bright);
  }

  fprintf(stderr, "Bright = %.1f, dim = %.1f, del = %.1f\n",
	  mag_bright, mag_dim, del_mag);
  mag_per_pixel = del_mag/(G_height-G_margin_T-G_margin_B);

  // draw bounding rectangle
  XDrawRectangle(XtDisplay(GraphCanvas), XtWindow(GraphCanvas),
		 gc_black,
		 G_margin_L, G_margin_T,
		 G_width - G_margin_L - G_margin_R,
		 G_height - G_margin_T - G_margin_B);

  // vertical axis tick marks
  int TickX;
  for(TickX = (int) (0.5 + (mag_bright - 1.0));
      TickX <= (int) mag_dim;
      TickX++) {
    if(TickX >= mag_bright && TickX <= mag_dim) {
      XDrawLine(XtDisplay(GraphCanvas), XtWindow(GraphCanvas),
		gc_black,
		CanvasX(time_first)-3, CanvasY(TickX),
		CanvasX(time_first)+3, CanvasY(TickX));
      char buffer[16];
      sprintf(buffer, "%.1f", (double) TickX);
      XDrawString(XtDisplay(GraphCanvas), XtWindow(GraphCanvas),
		  gc_black,
		  CanvasX(time_first)-10, CanvasY(TickX),
		  buffer,
		  strlen(buffer));
    }
  }

  XPoint *points = (XPoint *) malloc(sizeof(XPoint) * star->num_obs);

  for(i=0; i<star->num_obs; i++) {
    const int x = CanvasX(star->obs_list[i]->obs_date);
    points[i].x = x;
    const int y = CanvasY(star->obs_list[i]->obs_magnitude);
    points[i].y = y;

    XDrawRectangle(XtDisplay(GraphCanvas), XtWindow(GraphCanvas),
		   gc_red,
		   x-2, y-2,
		   4, 4);
  }
  XDrawLines(XtDisplay(GraphCanvas), XtWindow(GraphCanvas),
	     gc_blue,
	     points,
	     star->num_obs,
	     CoordModeOrigin);
  free(points);

}

/*****************************************************************
  Refresh()
  ****************************************************************/

void StarDataRefresh(void) {
  StarData *star = star_array[current_selected_star];

  char buffer[32];
  sprintf(buffer, "%d/%d", num_stars_remaining, total_num_stars);

  XmString x_string;
  x_string = XmStringCreateLocalized(buffer);
  XtVaSetValues(ProgressLabel, XmNlabelString, x_string, NULL);
  XmStringFree(x_string);

  x_string = XmStringCreateLocalized(star->starname);
  XtVaSetValues(StarNameLabel, XmNlabelString, x_string, NULL);
  XmStringFree(x_string);

  // Sort the observations for this star by date, earliest first
  {
    
    int i, j;
  top:
    for(i=0; i<star->num_obs-1; i++) {
      j = i+1;
      if(star->obs_list[i]->obs_date >
	 star->obs_list[j]->obs_date) {
	struct one_obs *temp;
	temp = star->obs_list[j];
	star->obs_list[j] = star->obs_list[i];
	star->obs_list[i] = temp;
	goto top;
      }
    }
  }

  char *text_string = (char *) malloc(60*star->num_obs);
  text_string[0] = 0;

  {
    for(int i=0; i<star->num_obs; i++) {
      char temp[60];
      sprintf(temp, "%.1f     %9.3f\n",
	      star->obs_list[i]->obs_date,
	      star->obs_list[i]->obs_magnitude);
      strcat(text_string, temp);
    }
  }
  XmTextSetString(StarTextWidget, text_string);

  free(text_string);

  DrawGraph(star);
}

/****************************************************************/
/*        Next Callback						*/
/****************************************************************/

void NextCallback(Widget W, XtPointer Client_Data, XtPointer call_data) {
  while(++current_selected_star < total_num_stars) {
    num_stars_remaining--;
    StarData *star = star_array[current_selected_star];
    if(star->num_obs >= 4 &&
       star->already_done == 0) {
      star->already_done = 1;
      StarDataRefresh();
      return;
    }
  }
}

struct StarData *LookupStar(char *star_name) {
  struct StarData *star;

  for(star = head_star; star; star=star->next) {
    if(strcmp(star->starname, star_name) == 0) {
      return star;
    }
  }

  // new star!
  star = new StarData;
  star->next = head_star;
  head_star = star;

  star->obs_list   = 0;
  star->num_obs    = 0;
  star->obs_so_far = 0;
  star->already_done = 0;
  star->starname   = strdup(star_name);

  total_num_stars++;

  return star;
}

void FinalizeStars(void) {
  fprintf(stderr, "Finalizing star list... ");
  star_array = new StarData *[total_num_stars];

  obs_array = new struct one_obs *[total_num_obs];
  int obs_array_index = 0;

  struct StarData *star;
  int i = 0;

  for(star = head_star; star; star=star->next) {
    star_array[i++] = star;
    star->obs_list = &(obs_array[obs_array_index]);
    obs_array_index += star->num_obs;
  }

  fprintf(stderr, " %d stars found.\nFinalizing observations... ",
	  total_num_stars);

  {
    struct one_obs *obs;

    for(obs = head_obs; obs; obs=obs->next) {
      star = obs->star;
      star->obs_list[star->obs_so_far++] = obs;
    }
  }

  fprintf(stderr, " %d observations found.\n", total_num_obs);

  fprintf(stderr, "Starting consistency check ...");

  static int failed = 0;
  for(i=0; i<total_num_stars; i++) {
    star = star_array[i];
    int j;
    for(j=0; j<star->num_obs; j++) {
      if(star->obs_list[j]->star != star) {
	if(!failed) {
	  fprintf(stderr, "\n     failed.\n");
	  failed = 1;
	}
      }
    }
  }
  if(!failed) fprintf(stderr, " Passed.\n");
  num_stars_remaining = total_num_stars;
}
  
void NewObservation(char *star_name,
		    double current_time,
		    double current_magnitude) {
  struct one_obs *new_one = new one_obs;

  new_one->next          = head_obs;
  head_obs               = new_one;
  new_one->obs_date      = current_time;
  new_one->obs_magnitude = current_magnitude;
  new_one->star          = LookupStar(star_name);
  new_one->star->num_obs++;

  total_num_obs++;
}

void ReadAllData(void) {
  FILE *fp = fopen("/usr/local/ASTRO/ARCHIVE/archive.dat", "r");
  char buffer[256];
  double current_time;
  double current_magnitude;
  char *star_name = 0;

  while(fgets(buffer, sizeof(buffer), fp)) {
    InputLine LineData(buffer);

    int n;
    for(n=0; n< LineData.NumPairs; n++) {
      if(strcmp(LineData.PairKeyword(n), "T") == 0) {
	sscanf(LineData.PairValue(n), "%lf", &current_time);
      }
      if(strcmp(LineData.PairKeyword(n), "S") == 0) {
	star_name = LineData.PairValue(n);
      }
    }
    for(n=0; n< LineData.NumPairs; n++) {
      if(strcmp(LineData.PairKeyword(n), "MV") == 0) {
	if(strcmp(LineData.PairValue(n), "NaN") != 0) {
	  sscanf(LineData.PairValue(n), "%lf", &current_magnitude);

	  NewObservation(star_name, current_time, current_magnitude);
	}
      }
    }
  }

  fclose(fp);
  FinalizeStars();
}
      
