/*  mini_win.cc -- Implements class MiniWin to support show_sequence
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
#include <Xm/Xm.h>
#include <Xm/SeparatoG.h>
#include <Xm/Label.h>
#include <Xm/RowColumn.h>
#include <Xm/ToggleB.h>
#include <libgen.h>		// basename()
#include "mini_win.h"

MiniWin::MiniWin(char *image_file,
		 Image *dark,
		 Image *flat,
		 Widget *parent,
		 ScreenImageParams *params,
		 int mini_width,
		 int mini_height) {

  min_width = mini_width;
  min_height = mini_height;
  Image_file = strdup(image_file);
  Dark = dark;
  Flat = flat;
  Parent = parent;
  min_image = 0;
  screen_image = 0;
  ref_params = params;
  CurrentTop = CurrentLeft = -1; // force a redraw

  MainManager = XtVaCreateManagedWidget("MainManager",
					xmRowColumnWidgetClass,
					*parent,
					XmNorientation, XmVERTICAL,
					XmNmarginHeight, 0,
					NULL);

  /*
  XtVaCreateManagedWidget("ImageSeparator",
			  xmSeparatorGadgetClass,
			  MainManager,
			  XmNorientation, XmHORIZONTAL,
			  0);
			  */

  InfoManager = XtVaCreateManagedWidget("InfoManager",
					xmRowColumnWidgetClass,
					MainManager,
					XmNorientation, XmVERTICAL,
					NULL);

#define SELECT_STRING "Select"
  SelectButton = XtVaCreateManagedWidget("SelectButton",
					 xmToggleButtonWidgetClass,
					 InfoManager,
					 XmNfillOnSelect, TRUE,
					 XmNmarginHeight, 0,
					 XmNmarginWidth, 0,
					 XmNindicatorOn, 0x11,
					 XmNindicatorType, 1, //XmN_OF_MANY,
					 XtVaTypedArg, XmNselectColor, XmRString, "red", strlen("red")+1,
					 XtVaTypedArg, XmNlabelString, XmRString, SELECT_STRING, strlen(SELECT_STRING)+1,
					 NULL);

  {
    XmString rootfilename;
    char *root_string_area = strdup(image_file);
    char *simple_name = basename(root_string_area);
    {
      char *s;
      for(s=simple_name; *s; s++) {
	if(strcmp(s, ".fits") == 0) {
	  *s = 0;
	  break;
	}
      }
    }
	
    rootfilename = XmStringCreateLocalized(simple_name);
    NameLabel = XtVaCreateManagedWidget("NameLabel",
					xmLabelWidgetClass,
					InfoManager,
					XmNlabelString, rootfilename,
					XmNmarginHeight, 0,
					XmNmarginWidth, 0,
					NULL);
    free(root_string_area);
    XmStringFree(rootfilename);
  }

  MedianLabel = XtVaCreateManagedWidget("MedianLabel",
					xmLabelWidgetClass,
					InfoManager,
					XmNmarginHeight, 0,
					XmNmarginWidth, 0,
					NULL);

  SetTopLeftAndRedraw(0, 0);
}

void
MiniWin::SetTopLeftAndRedraw(int top, int left) {
  if(top != CurrentTop || left != CurrentLeft) {
    Image i(Image_file);

    if(Dark) i.subtract(Dark);
    if(Flat) i.scale(Flat);

    if(min_image) delete min_image;
  
    min_image = i.CreateSubImage(top,
				 left,
				 min_height,
				 min_width);

    if(screen_image) {
      screen_image->DisplayImage(min_image, *ref_params);
      screen_image->DrawScreenImage();

    } else {
      screen_image = new ScreenImage(min_image, &MainManager, ref_params);
    }

    {
      char median_string[32];
      sprintf(median_string, "%d", (int) i.statistics()->MedianPixel);
      XmString xmstring = XmStringCreateLocalized(median_string);
      XtVaSetValues(MedianLabel, XmNlabelString, xmstring, NULL);
      XmStringFree(xmstring);
    }
  }
}

void
MiniWin::SetParams(ScreenImageParams *params) {
  ref_params = params;
  ReDraw();
}

void
MiniWin::ReDraw(void) {
  if(screen_image) {
    screen_image->DisplayImage(min_image, *ref_params);
    screen_image->DrawScreenImage();
  }
}

MiniWin::~MiniWin(void) {
  if(screen_image) delete screen_image;
  if(min_image) delete min_image;
  free(Image_file);
}

  // returns 1 if user set the select toggle
int
MiniWin::IsSelected(void) {
  return XmToggleButtonGetState(SelectButton);
}
