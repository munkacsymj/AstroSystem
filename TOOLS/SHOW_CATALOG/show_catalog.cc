/*  show_catalog.cc -- Plots a catalog in a window as if it was an image.
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
#include "Image.h"
#include "HGSC.h"
#include "TCS.h"
#include <named_stars.h>

#include <X11/StringDefs.h>
#include <X11/Intrinsic.h>
#include <X11/Core.h>
#include <X11/Xcms.h>
#include <X11/Xaw/Box.h>
#include <X11/Xaw/Command.h>

#include <unistd.h>		// getopt()
#include <string.h>		// strdup()
#include <stdio.h>
#include <stdlib.h>		// malloc()
#include <screen_image.h>
#include <math.h>		// cos(), fabs()
#include <gendefs.h>

////////////////////////////////////////////////////////////////
//      show_catalog: This program is called to create a
//      "fake" image on the screen that holds the stars
//      contained in the cataloged area around the named
//      star.
////////////////////////////////////////////////////////////////

static DEC_RA Reference_pos;
TCStoDecRA *transform;
TCStoImage *ImageTransform;
void convert_to_xy(char *name, DEC_RA &loc, double &x, double &y);
void RefreshDisplay(ScreenImage *si, HGSCList *hgsc, double mag_limit);
void StarClick(ScreenImage *si, int star_index);
void quit_callback(Widget W, XtPointer Client_Data, XtPointer call_data);
double mag_limit = 19.9;	// default magnitude; stars dimmer
				// than this won't be plotted

HGSCList *hgsc;

StarCenters *star_info = 0;
XtAppContext app_context;

int main(int argc, char **argv) {
  int option_char;
  char *starname = 0;
  char *scalename = strdup("ST9");

  while((option_char = getopt(argc, argv, "m:s:tn:")) > 0) {
    switch (option_char) {
    case 'm':
      mag_limit = atof(optarg);
      break;

    case 's':			// display scale
      scalename = strdup(optarg);
      break;

    case 'n':			// name of star
      starname = optarg;
      break;

    case '?':			// invalid argument
    default:
      fprintf(stderr, "Invalid argument.\n");
      exit(2);
    }
  }

  argc -= optind;
  argv += optind;

  // if there's no image to display, just quit
  if(starname == 0) exit(0);

  if(strcmp(scalename, "ST9") == 0) {
    ImageTransform = new TCStoImage(1.52, PCS{256,256});
  } else if(strcmp(scalename, "d") == 0) {
    ImageTransform = new TCStoImage(3.0, PCS{256,256});
  } else {
    fprintf(stderr, "Unrecognized scale: %s\n", scalename);
    exit(-1);
  }

  NamedStar named_star(starname);
  if(!named_star.IsKnown()) {
    fprintf(stderr, "Don't know of star named '%s'\n", starname);
    exit(2);
  }

  Reference_pos = named_star.Location();

  fprintf(stderr, "argc = %d\n", argc);
  if(argc >= 1) {
    double north_delta = 0.0;
    double east_delta = 0.0;
    while(argc) {
      argc--;
      char last_letter;
      const int len = strlen(argv[argc]);
      char *last_letter_ptr = argv[argc] + (len-1);

      last_letter = *last_letter_ptr;
      *last_letter_ptr = 0;

      double converted_value = atof(argv[argc]);

      fprintf(stderr, "letter = '%c', val=%.2f\n",
	      last_letter, converted_value);

      switch(last_letter) {
      case 'n':
      case 'N':
	north_delta = converted_value;
	break;

      case 's':
      case 'S':
	north_delta = -converted_value;
	break;

      case 'e':
      case 'E':
	east_delta = converted_value;
	break;

      case 'w':
      case 'W':
	east_delta = -converted_value;
	break;

      default:
	fprintf(stderr, "Motion must end with one of N, S, E, or W\n");
	exit(2);
      }
    }

    north_delta *= ((2.0 * M_PI)/360.0)/60.0;
    east_delta  *= ((2.0 * M_PI)/360.0)/60.0;
    Reference_pos.increment(north_delta, east_delta);
  }

  transform = new TCStoDecRA(Reference_pos);

  ImageTransform->print(stderr);
  // Find the stars in the resulting image
  // IStarList *list = image->GetIStarList();
  // fprintf(stderr, "Image has %d stars.\n", list->NumStars);

  char HGSCfilename[132];
  sprintf(HGSCfilename, CATALOG_DIR "/%s", starname);
    
  FILE *hgsc_fp = fopen(HGSCfilename, "r");
  if(!hgsc_fp) {
    fprintf(stderr, "Correlate: cannot open '%s'\n", HGSCfilename);
    return 0;
  }

  hgsc = new HGSCList(hgsc_fp);
  fclose(hgsc_fp);

  star_info = new StarCenters[hgsc->length()];

  // Now perform all the "X" stuff.
  Widget topLevel, box_widget, stop_button;
  int pixmap_depth;

  XtSetLanguageProc(NULL, (XtLanguageProc)NULL, NULL);

  topLevel = XtVaAppInitialize(&app_context,
			       "ShowCatalog",
			       NULL, 0,
			       &argc, argv,
			       NULL,
			       NULL);

  box_widget = XtCreateManagedWidget("box area", boxWidgetClass,
				     topLevel,
				     NULL, 0); /* no args */

  stop_button = XtVaCreateManagedWidget("stop button",
					commandWidgetClass,
					box_widget,
					XtNlabel, "Quit",
					NULL);

  XtAddCallback(stop_button, XtNcallback, quit_callback, NULL);

  XtVaGetValues(box_widget, XtNdepth, &pixmap_depth, NULL);

  // Create a fake image to provide a canvas on which to draw
#define I_WIDTH 512
#define I_HEIGHT 512

  Image fake_image(I_HEIGHT, I_WIDTH);

  ScreenImageParams params;
  params.black_value = 0.0;
  params.white_value = 1.0;	// dummy values
  ScreenImage *si = new ScreenImage(&fake_image, &box_widget, &params);

  si->SetStarClickCallback(StarClick);

  XtRealizeWidget(topLevel);

  RefreshDisplay(si, hgsc, mag_limit);

  si->DrawScreenImage();
  
  XtAppMainLoop(app_context);
}

void RefreshDisplay(ScreenImage *si, HGSCList *hgsc, double mag_limit) {
  HGSCIterator It(*hgsc);
  HGSC *OneStar;
  int i = 0;
  for(OneStar = It.First(); OneStar; OneStar = It.Next()) {
    if(OneStar->magnitude > mag_limit) continue;

    double x, y;
    convert_to_xy(OneStar->label, OneStar->location, x, y);
    int radius = (int) (0.5 + (18.0 - OneStar->magnitude)/2.0);
    if(radius < 1) radius = 1;
    if(radius > 5) radius = 5;

    star_info[i].x           = x;
    star_info[i].y           = y;
    star_info[i].radius      = radius;
    star_info[i].enable      = 1;
    star_info[i].label       = OneStar->label;
    star_info[i].enable_text = 1;
    star_info[i].color       = (OneStar->is_check || OneStar->is_comp) ?
      ScreenRed : ScreenCyan;

    i++;

  }
  si->SetStarCircles(1);	// enable circles
  si->SetStarInfo(star_info, hgsc->length());
  si->DisplayImage();
}

void convert_to_xy(char *name, DEC_RA &location, double &x, double &y) {
  TCS t = transform->toTCS(location);
  PCS p = ImageTransform->toPCS(t);

  x = p.x;
  y = p.y;
  /* fprintf(stderr,
     "%s, x = %f, y = %f, DEC = %f, RA = %f, TCSx = %f, TCSy = %f\n",
     name,
     x, y,
     location.dec(), location.ra_radians(),
     t.x, t.y); */
}
    
void StarClick(ScreenImage *si, int star_index) {
  if(star_index >= 0) {
    star_info[star_index].enable_text = !star_info[star_index].enable_text;
    fprintf(stderr, "StarClick: star index %d toggled\n", star_index);
    si->DisplayImage();
    si->DrawScreenImage();
  }
}
  
/****************************************************************/
/*        Quit Callback						*/
/****************************************************************/
void quit_callback(Widget W, XtPointer Client_Data, XtPointer call_data) {
  XtAppSetExitFlag(app_context);
}

