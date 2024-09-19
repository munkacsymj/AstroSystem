/*  show_sequence.cc -- Program to display multiple images of same object
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
#include <Xm/DrawingA.h>
#include <Xm/Form.h>
#include <Xm/FileSB.h>

#include <Image.h>

#include "mini_win.h"
#include "screen_image.h"
#include <gendefs.h>

static const int mini_win_size = 90;

ScreenImageParams params;
ScreenImage *BigPicture;
Image       *BigImage;
char        *BigImageFilename;
int center_x = 0;
int center_y = 0;
int total_image_width;
int total_image_height;
int num_mini_wins = 0;
MiniWin **WinArray;
char *stack_filename = 0;
char *dark_filename = 0;
char *flat_filename = 0;
int ok_to_find_stars = 0;
double q_find_stars = 1.5;

void ShowBusy(void);
void ShowReady(void);
////////////////////////////////////////////////////////////////
//        CALLBACK DECLARATIONS
////////////////////////////////////////////////////////////////
void NewPixelValue_callback(Widget w,
			    XtPointer client_data,
			    XtPointer call_data);
void CircleStarsCallback(Widget w,
			 XtPointer client_data,
			 XtPointer call_data);
void StackCallback(Widget w,
		   XtPointer client_data,
		   XtPointer call_data);
void FindStarsCallback(Widget w,
		       XtPointer client_data,
		       XtPointer call_data);
void QEntryCallback(Widget w,
		    XtPointer client_data,
		    XtPointer call_data);

void RedrawAllWindows(void);

Widget toplevel,		// top level
    manager,			// top-level manager
    RightSide,			// right-side RowColumn
    LeftSide,			// left-side RowColumn
    RefreshButton,		// Refresh button
    ExposureLabel,
    FileArea,
    ImageSpecifyButton,
    DarkSpecifyButton,
    FlatSpecifyButton,
    AutoImageButton,
    ImageArea,
    BlackEntry,
    WhiteEntry,
    Magnifier,
    ExitButton;			// Exit button

Display *display;
// Screen *screen;
XtAppContext context;

             int pixmap_depth;
const static int image_max_width  = 512;
const static int image_max_height = 512;

void MagExposeCallback(Widget W, XtPointer client_data, XtPointer call_data);
void OpenFileCallback(Widget W, XtPointer client_data, XtPointer call_data);
void OpenImageCallback(Widget W, XtPointer client_data, XtPointer call_data);
void DwgClickEvent(Widget W,
		   XtPointer client_data,
		   XEvent *event,
		   Boolean *continue_dispatch);
void NewPixelValue_callback(Widget w,
			    XtPointer client_data,
			    XtPointer call_data);
void DwgExposeCallback(Widget W, XtPointer client_data, XtPointer call_data);
void RefreshBlackWhite(void);
void RefreshCoolerData(XtPointer client_data, XtIntervalId *timerid);
void RefreshCoolerCallback(XtPointer client_data, XtIntervalId *timerid);
void PrintButtonCallback(Widget W, XtPointer Client_Data, XtPointer call_data);
void ExitCallback(Widget W, XtPointer Client_Data, XtPointer call_data);
void RefreshImage(Widget W, XtPointer Client_Data, XtPointer call_data);
void DisplayImageFile(const char *ImageFilename);
void DisplayAuxData(const char *ImageFilename);
void RefreshImageAndAux(const char *ImageFilename);
void SetupMagnifier(void);
void update_grid(Pixmap *p);

//
// For X properties, see /usr/lib/X11/app-defaults/show_sequence
//

int
main(int argc, char **argv) {
  int option_char;
  Image *dark = 0;
  Image *flat = 0;

  while((option_char = getopt(argc, argv, ":d:s:o:")) > 0) {
    switch (option_char) {
    case 's':			// scale image (flat field)
      flat = new Image(optarg);
      if(!flat) {
	fprintf(stderr, "Cannot open flatfield image %s\n", optarg);
	flat = 0;
      }
      flat_filename = optarg;
      break;

    case 'o':			// output file for stacking
      stack_filename = optarg;
      break;

    case 'd':			// dark file name
      if(dark) {
	Image *new_dark = new Image(optarg);
	dark->add(new_dark);
      } else {
	dark = new Image(optarg);
	dark_filename = optarg;
      }
      fprintf(stderr, "show_image: dark file = '%s'\n", optarg);
      break;

    case '?':			// invalid argument
    default:
      fprintf(stderr, "Invalid argument.\n");
      exit(2);
    }
  }

  argc -= optind;
  argv += optind;

  if(argc > 20) {
    fprintf(stderr, "show_sequence: too many images (20 max)\n");
    exit(-2);
  }

  toplevel = XtAppInitialize(&context, "show_sequence",
			     NULL, 0, &argc, argv, NULL, NULL, 0);

  display = XtDisplay(toplevel);
  // screen  = DefaultScreen(display);

  manager = XtVaCreateManagedWidget("manager",
				    xmRowColumnWidgetClass, toplevel,
				    XmNorientation, XmHORIZONTAL,
				    NULL);

  XtVaGetValues(manager, XtNdepth, &pixmap_depth, NULL);
  ////////////////////////////////////////////////////////////////
  //        LEFT SIDE STUFF
  ////////////////////////////////////////////////////////////////

  LeftSide = XtVaCreateManagedWidget("LeftSide",
				     xmRowColumnWidgetClass, manager,
				     XmNorientation, XmVERTICAL,
				     NULL);

  RightSide = XtVaCreateManagedWidget("RightSide",
				      xmRowColumnWidgetClass, manager, 
				      XmNorientation, XmVERTICAL,
				      XmNpacking, XmPACK_COLUMN,
				      XmNnumColumns, 4,
				      NULL);

  Widget TopButtons = XtVaCreateManagedWidget("TopButtons",
					      xmRowColumnWidgetClass,
					      LeftSide,
					      XmNorientation, XmVERTICAL,
					      XmNpacking, XmPACK_COLUMN,
					      XmNnumColumns, 3,
					      NULL);

  ExitButton =
    XtVaCreateManagedWidget("ExitButton",
			    xmPushButtonWidgetClass, TopButtons,
			    XmNwidth, 30,
			    NULL);
  XtAddCallback(ExitButton, XmNactivateCallback, ExitCallback, NULL);

  Widget MinMaxArea;
  MinMaxArea = XtVaCreateManagedWidget("MinMaxArea",
				       xmRowColumnWidgetClass,
				       TopButtons,
				       XmNorientation, XmHORIZONTAL,
				       NULL);

  BlackEntry = XtVaCreateManagedWidget("BlackEntry",
				       xmTextFieldWidgetClass,
				       MinMaxArea,
				       XmNvalue, "1.0",
				       XmNcolumns, 8,
				       XmNuserData, (XtPointer) &params.black_value,
				       NULL);
  WhiteEntry = XtVaCreateManagedWidget("WhiteEntry",
				       xmTextFieldWidgetClass,
				       MinMaxArea,
				       XmNcolumns, 8,
				       XmNvalue, "1.0",
				       XmNuserData, (XtPointer) &params.white_value,
				       NULL);
  XtAddCallback(BlackEntry,
		XmNactivateCallback,
		NewPixelValue_callback,
		(XtPointer) &params.black_value);
  XtAddCallback(WhiteEntry,
		XmNactivateCallback,
		NewPixelValue_callback,
		(XtPointer) &params.white_value);
  XtAddCallback(BlackEntry,
		XmNlosingFocusCallback,
		NewPixelValue_callback,
		(XtPointer) &params.black_value);
  XtAddCallback(WhiteEntry,
		XmNlosingFocusCallback,
		NewPixelValue_callback,
		(XtPointer) &params.white_value);
  
  Widget StackButton = XtVaCreateManagedWidget("StackButton",
					xmPushButtonWidgetClass,
					TopButtons,
					       XmNwidth, 30,
					NULL);
  Widget FindStarsButton = XtVaCreateManagedWidget("FindStarsButton",
					    xmPushButtonWidgetClass,
					    TopButtons,
					       XmNwidth, 30,
					    NULL);
  Widget CircleStarsToggle = XtVaCreateManagedWidget("CircleStarsToggle",
					      xmToggleButtonWidgetClass,
					      TopButtons,
					      NULL);

  char Qtemp[16];
  sprintf(Qtemp, "%.2f", q_find_stars);
  Widget QEntry = XtVaCreateManagedWidget("QEntry",
					  xmTextFieldWidgetClass,
					  TopButtons,
					  XmNvalue, Qtemp,
					  XmNcolumns, 8,
					  NULL);


  XtAddCallback(CircleStarsToggle,
		XmNvalueChangedCallback,
		CircleStarsCallback, 0);
  XtAddCallback(StackButton,
		XmNactivateCallback,
		StackCallback, 0);
  XtAddCallback(FindStarsButton,
		XmNactivateCallback,
		FindStarsCallback, 0);
  XtAddCallback(QEntry,
		XmNactivateCallback,
		QEntryCallback, 0);
  XtAddCallback(QEntry,
		XmNlosingFocusCallback,
		QEntryCallback, 0);

  WinArray = new MiniWin * [argc];
  
  if(argc < 1) {
    fprintf(stderr, "show_sequence: no images to display\n");
    exit(-2);
  }

  BigImageFilename = argv[0];
  BigImage = new Image(BigImageFilename);

  if(dark) BigImage->subtract(dark);
  if(flat) BigImage->scale(flat);
    
  total_image_width = BigImage->width;
  total_image_height = BigImage->height;
  params.black_value = BigImage->statistics()->MedianPixel - 20.0;
  params.white_value = 200.0 + params.black_value;

  fprintf(stderr, "stretching image between %.1f and %.1f\n",
	  params.black_value, params.white_value);
  BigPicture = new ScreenImage(BigImage,
			       &LeftSide,
			       &params);

  XtAddEventHandler(BigPicture->GetImageWidget(),
		    ButtonPressMask,
		    False,
		    DwgClickEvent,
		    0);

  num_mini_wins = argc;
  {
    int image_num;
    for(image_num = 0; image_num < argc; image_num++) {
      WinArray[image_num] = new MiniWin(argv[image_num],
					dark,
					flat,
					&RightSide,
					&params,
					mini_win_size,
					mini_win_size);
    }
  }
				 
  XtRealizeWidget(toplevel);

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

/****************************************************************/
/*        NewPixelValue Callback				*/
/****************************************************************/
void NewPixelValue_callback(Widget w,
			    XtPointer client_data,
			    XtPointer call_data) {
  char *new_string = XmTextFieldGetString(w);
  double *pixel_value = (double *) client_data;
  int new_pixel_value;
  char *string_end;

  new_pixel_value = strtol(new_string, &string_end, 10);
  // What could have gone wrong?
  // 1. If no conversion was performed, string_end == new_string
  if(string_end == new_string ||
     (*string_end != '\0' &&
      *string_end != ' ' &&
      *string_end != '\t')) {
    fprintf(stderr, "Illegal pixel value entry");
    new_pixel_value = (int) *pixel_value; // re-display old value
  }
  *pixel_value = new_pixel_value;

  if(params.black_value >= params.white_value) {
    // trouble!
    params.black_value = params.white_value - 1;
  }

  RedrawAllWindows();

  XtFree(new_string);
}

void RefreshBlackWhite(void) {
  char buffer[80]; 

  sprintf(buffer, "%d", (int) params.white_value);
  XmTextFieldSetString(WhiteEntry, buffer);
  sprintf(buffer, "%d", (int) params.black_value);
  XmTextFieldSetString(BlackEntry, buffer);
}    

void DwgClickEvent(Widget W,
		   XtPointer client_data,
		   XEvent *event,
		   Boolean *continue_dispatch) {
  int button = event->xbutton.button;

  if(event->xbutton.type == ButtonPress && button == Button1) {
    // redraw all windows.
    center_x = event->xbutton.x;
    center_y = event->xbutton.y;
    RedrawAllWindows();
  }
}

void RedrawAllWindows(void) {
  int i;
  int top = center_y - mini_win_size/2;
  int left = center_x - mini_win_size/2;

  if(top < 0) top = 0;
  if(left < 0) left = 0;
  if(top >= total_image_height - mini_win_size)
    top = total_image_height - mini_win_size - 1;
  if(left >= total_image_width - mini_win_size)
    left = total_image_width - mini_win_size - 1;

  ShowBusy();
  for(i=0; i<num_mini_wins; i++) {
    WinArray[i]->SetTopLeftAndRedraw(top, left);
    WinArray[i]->SetParams(&params);
  }

  BigPicture->DisplayImage(BigImage, params);
  BigPicture->DrawScreenImage();
  ShowReady();
}

void CircleStarsCallback(Widget w,
			 XtPointer client_data,
			 XtPointer call_data) {
  if(XmToggleButtonGetState(w)) {
    // circle stars turned on
    IStarList list(BigImageFilename);
    static StarCenters *star_stuff = 0;
    if(star_stuff) delete [] star_stuff;
    star_stuff = new StarCenters[list.NumStars];
    fprintf(stderr, "Circling %d stars\n", list.NumStars);
    for(int i = 0; i < list.NumStars; i++) {
      star_stuff[i].x = list.FindByIndex(i)->nlls_x;
      star_stuff[i].y = list.FindByIndex(i)->nlls_y;
      star_stuff[i].radius = 3;
      star_stuff[i].enable = 1;
      star_stuff[i].enable_text = 0;
      star_stuff[i].color = ScreenRed;
      star_stuff[i].label = 0;
    }
    BigPicture->SetStarInfo(star_stuff, list.NumStars);
    BigPicture->SetStarCircles(1);
  } else {
    // circle stars turned off
    BigPicture->SetStarInfo(0, 0);
    fprintf(stderr, "Turning off star circles\n");
  }

  BigPicture->DisplayImage(BigImage, params);
  BigPicture->DrawScreenImage();
  ShowReady();
  XFlush(display);
}

void StackCallback(Widget w,
		   XtPointer client_data,
		   XtPointer call_data) {
  char stack_command[1024];
  char temp[128];
  if(stack_filename == 0) {
    fprintf(stderr,
	    "show_sequence: cannot stack without output filename (-o)\n");
    return;
  }

  sprintf(stack_command, "stack -t -e -o %s ", stack_filename);

  if(flat_filename) {
    sprintf(temp, "-s %s ", flat_filename);
    strcat(stack_command, temp);
  }
  if(dark_filename) {
    sprintf(temp, "-d %s ", dark_filename);
    strcat(stack_command, temp);
  }
  
  for(int i=0; i<num_mini_wins; i++) {
    if(WinArray[i]->IsSelected()) {
      sprintf(temp, "%s ", WinArray[i]->Image_filename());
      strcat(stack_command, temp);
    }
  }

  ShowBusy();
  // fprintf(stderr, "show_sequence: executing '%s'\n", stack_command);
  if(system(stack_command)) {
    fprintf(stderr, "show_sequence: stack command failed.\n");
  }
  Image *stacked_image = new Image(stack_filename);
  if(!stacked_image) {
    fprintf(stderr, "show_sequence: cannot open stack output file %s\n",
	    stack_filename);
  } else {
    delete BigImage;
    BigImage = stacked_image;
    BigImageFilename = stack_filename;
    fprintf(stderr, "Displaying stacked image.\n");
    BigPicture->DisplayImage(BigImage, params);
    BigPicture->DrawScreenImage();
    ok_to_find_stars = 1;
  }
  ShowReady();
}
  
void FindStarsCallback(Widget w,
		       XtPointer client_data,
		       XtPointer call_data) {
  if(ok_to_find_stars) {
    char commands[128];
    sprintf(commands, COMMAND_DIR "/find_stars -q %f -i %s",
	    q_find_stars, stack_filename);
    fprintf(stderr, "show_sequence: %s\n", commands);
    ShowBusy();
    if(system(commands)) {
      fprintf(stderr, "find_stars command failed.\n");
    }
    ShowReady();
  }
}

void QEntryCallback(Widget w,
		    XtPointer client_data,
		    XtPointer call_data) {
  char *new_string = XmTextFieldGetString(w);
  double new_q_value;
  char *string_end;

  new_q_value = strtod(new_string, &string_end);
  // What could have gone wrong?
  // 1. If no conversion was performed, string_end == new_string
  if(string_end == new_string ||
     (*string_end != '\0' &&
      *string_end != ' ' &&
      *string_end != '\t')) {
    fprintf(stderr, "Illegal q value entry");
    new_q_value = q_find_stars; // re-display old value
  }
  q_find_stars = new_q_value;
  XtFree(new_string);
}

void ShowBusy(void) {
  static int first_time = 1;
  static Cursor crosshair_cursor;
  if(first_time) {
    crosshair_cursor = XCreateFontCursor(display, XC_watch);
    first_time = 0;
  }
  XDefineCursor(display, XtWindow(toplevel), crosshair_cursor);
  XFlush(display);
}

void ShowReady(void) {
  static int first_time = 1;
  static Cursor crosshair_cursor;
  if(first_time) {
    crosshair_cursor = XCreateFontCursor(display, XC_arrow);
    first_time = 0;
  }
  XDefineCursor(display, XtWindow(toplevel), crosshair_cursor);
  XFlush(display);
}
