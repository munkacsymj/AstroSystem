/*  show_image.cc -- Program to display an image in a window
 *
 *  Copyright (C) 2007, 2020 Mark J. Munkacsy
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

#include <X11/StringDefs.h>
#include <X11/Intrinsic.h>
#include <X11/Core.h>
#include <X11/Xcms.h>
#include <X11/Xaw/Box.h>
#include <X11/Xaw/Command.h>

#include <unistd.h>		// getopt()
#include <stdio.h>
#include <stdlib.h>		// malloc()
#include <screen_image.h>
#include <bad_pixels.h>

void StarClick(ScreenImage *si, int star_index);
void quit_callback(Widget W, XtPointer Client_Data, XtPointer call_data);
void RefreshDisplay(ScreenImage *si, bool circle_stars, bool show_bad_pixels);

StarCenters *star_info = 0;
XtAppContext app_context;

bool show_star_label = true;

int main(int argc, char **argv) {
  int option_char;
  Image *image = 0;
  Image *dark = 0;
  Image *flat = 0;
  double   max_pixel_value = -1.0;
  double   min_pixel_value = -1.0;
  int   num_darks = 0;
  int   circle_stars = 0;
  char *image_filename = 0;
  bool show_bad_pixels = false;

  while((option_char = getopt(argc, argv, "bCcu:l:i:d:s:")) > 0) {
    switch (option_char) {
    case 'u':
      max_pixel_value = atoi(optarg);
      break;

    case 'b':
      show_bad_pixels = true;
      break;

    case 's':			// scale image (flat field)
      flat = new Image(optarg);
      if(!flat) {
	fprintf(stderr, "Cannot open flatfield image %s\n", optarg);
	flat = 0;
      }
      break;

    case 'l':
      min_pixel_value = atoi(optarg);
      break;

    case 'C':
      circle_stars++;
      show_star_label = false;
      break;

    case 'c':			// circle stars
      circle_stars++;
      break;

    case 'i':			// image file name
      if(image != 0) {
	fprintf(stderr, "show_image: only one image file permitted.\n");
	exit(2);
      }
      fprintf(stderr, "show_image: image file = '%s'\n", optarg);
      image_filename = optarg;
      image = new Image(image_filename);
      break;

    case 'd':			// dark file name
      num_darks++;
      if(dark) {
	Image *new_dark = new Image(optarg);
	dark->add(new_dark);
      } else {
	dark = new Image(optarg);
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

  // if there's no image to display, just quit
  if(image == 0) exit(0);

  // subtract the dark image.  If more than one dark was provided,
  // average them together
  if(dark) {
    // we summed the darks together earlier, during argument
    // processing. Now divide by the number of darks processed (to
    // create the "average" dark)
    if(num_darks > 1) {
      dark->scale(1.0/num_darks);
    }
    // Now subtract the dark from the image
    image->subtract(dark);
  }

  if(flat) {
    image->scale(flat);
  }
  
  // Find the stars in the resulting image
  // IStarList *list = image->GetIStarList();
  // fprintf(stderr, "Image has %d stars.\n", list->NumStars);

  // Create the composite star
  // fprintf(stderr, "Focus factor = %f\n", image->composite_fwhm());

  // Now perform all the "X" stuff.
  Widget topLevel, box_widget, stop_button;
  int pixmap_depth;

  XtSetLanguageProc(NULL, (XtLanguageProc)NULL, NULL);

  topLevel = XtVaAppInitialize(&app_context,
			       "ShowImage",
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

  ScreenImageParams params;
  params.black_value = min_pixel_value;
  params.white_value = max_pixel_value;

  if(min_pixel_value < 0) {
    params.black_value = image->statistics()->DarkestPixel;
  }
  if(max_pixel_value < 0) {
    const double median = image->statistics()->MedianPixel;
    params.white_value = image->statistics()->DarkestPixel +
      3.5*(median-image->statistics()->DarkestPixel);
  }
  fprintf(stderr, "Brightest pixel is %.1lf\n",
	  image->statistics()->BrightestPixel);
  fprintf(stderr, "Darkest pixel is %.1lf\n",
	  image->statistics()->DarkestPixel);
  fprintf(stderr, "Median pixel is %.1lf\n",
	  image->statistics()->MedianPixel);
  fprintf(stderr, "Average pixel is %.2lf\n",
	  image->statistics()->AveragePixel);
  fprintf(stderr, "Pixel stddev is %.4lf\n",
	  image->statistics()->StdDev);

  ScreenImage *si = new ScreenImage(image, &box_widget, &params);

  si->SetStarClickCallback(StarClick);

  XtRealizeWidget(topLevel);

  if(circle_stars || show_bad_pixels) RefreshDisplay(si, circle_stars, show_bad_pixels);

  si->DisplayImage();

  XtAppMainLoop(app_context);
}

void RefreshDisplay(ScreenImage *si, bool circle_stars, bool show_bad_pixels) {
  int num_circles = 0;
  BadPixels bp;
  IStarList *starlist = si->GetImage()->GetIStarList();
  if (circle_stars) num_circles += starlist->NumStars;
  if (show_bad_pixels) num_circles += 2*(bp.GetDefects()->size());
  
  star_info = new StarCenters[num_circles];
  
  num_circles = 0;

  if (circle_stars) {
    for(int i=0; i< starlist->NumStars; i++) {
      IStarList::IStarOneStar *OneStar = starlist->FindByIndex(i);

      int radius = (int) (0.5 + (18.0 - OneStar->magnitude)/2.0);
      if(radius < 1) radius = 1;
      if(radius > 5) radius = 5;

      radius = 5;

      star_info[i].x           = OneStar->nlls_x;
      star_info[i].y           = OneStar->nlls_y;
      star_info[i].radius      = radius;
      star_info[i].color       = ScreenYellow;
      star_info[i].enable      = 1;
      star_info[i].label       = OneStar->StarName;
      star_info[i].enable_text = (show_star_label && OneStar->StarName[0] != 'S');
    }
    num_circles = starlist->NumStars;
  }

  if (show_bad_pixels) {
    for (auto b : *bp.GetDefects()) {
      star_info[num_circles].x = b->col;
      star_info[num_circles].y = b->row_start;
      star_info[num_circles].radius = 2;
      star_info[num_circles].color = ScreenCyan;
      star_info[num_circles].enable = 1;
      star_info[num_circles].label = "";
      star_info[num_circles].enable_text = 0;
      num_circles++;
      if (!b->single_pixel) {
	star_info[num_circles-1].color = ScreenGreen;
	star_info[num_circles] = star_info[num_circles-1];
	star_info[num_circles].y = b->row_end;
	num_circles++;
      }
    }
  }

  si->SetStarCircles(1);	// enable circles
  si->SetStarInfo(star_info, num_circles);
  si->DisplayImage();
}

void PrintPixels(ScreenImage *si, int x, int y) {
  const int diameter = 10;
  if (x-diameter < 0 ||
      y-diameter < 0 ||
      x+diameter >= si->GetImage()->width ||
      y+diameter >= si->GetImage()->height) return;

  for (int i = x-diameter; i < x+diameter; i++) {
    for (int j = y-diameter; j < y+diameter; j++) {
      printf("%6d ", (int) si->GetImage()->pixel(i, j));
    }
    printf("\n");
  }
}

void StarClick(ScreenImage *si, int star_index) {
  if(star_index >= 0) {
    star_info[star_index].enable_text = !star_info[star_index].enable_text;
    fprintf(stderr, "StarClick: star %s index %d toggled at (x,y) = (%.1lf,%.1lf)\n",
	    star_info[star_index].label,
	    star_index, star_info[star_index].x, star_info[star_index].y);
    si->DisplayImage();
    si->DrawScreenImage();
    PrintPixels(si,
		int(star_info[star_index].x+0.5),
		int(star_info[star_index].y+0.5));
  }
}
  
/****************************************************************/
/*        Quit Callback						*/
/****************************************************************/
void quit_callback(Widget W, XtPointer Client_Data, XtPointer call_data) {
  XtAppSetExitFlag(app_context);
}
