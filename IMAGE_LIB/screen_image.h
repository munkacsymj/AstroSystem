/* This may look like C code, but it is really -*-c++-*- */
/*  screen_image.h -- Implements X-window display of an image.
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
#ifndef _SCREEN_IMAGE_H
#define _SCREEN_IMAGE_H

#include "Image.h"

#include <unistd.h>		// getopt()
#include <stdio.h>
#include <stdlib.h>		// malloc()
#include <X11/StringDefs.h>
#include <X11/Intrinsic.h>
#include <X11/Core.h>
#include <X11/Xcms.h>

class ScreenImageParams {
public:
  double black_value;
  double white_value;
};

enum ScreenColor { ScreenRed, ScreenYellow, ScreenCyan, ScreenGreen };

class StarCenters {
public:
  double x, y;			/* star center in pixel coordinates */
  ScreenColor color;		// circle color
  double radius;		/* circle radius */
  int enable;			/* enable display of star circle */
  int enable_text;		/* enable display of label text */
  const char *label;		/* star's textual label */
};

class ScreenImage {
public:
  ScreenImage(Image *image,
	      Widget *parent,
	      ScreenImageParams *params);
  ~ScreenImage(void);

  Image *GetImage(void) { return ref_image; }
  Widget GetImageWidget(void) { return image_widget; }

  // call DisplayImage to change the screen params or the image or star circles
  void DisplayImage(Image *image, ScreenImageParams params);
  void DisplayImage(void);

  // force a redraw (refresh); no changes are made
  void DrawScreenImage(void);

  // Turn star circles on/off
  inline void SetStarCircles(int circles_enabled) {
    screen_circles_on = circles_enabled; }

  void SetStarInfo(StarCenters *stars, int number_stars) {
    NumberStars = number_stars;
    star_info = stars;
  }

  void SetStarClickCallback(void (*callback)(ScreenImage *si, int star_index));
  void SetPixelClickCallback(void (*callback)(ScreenImage *si, int x, int y));

  void PerformClickCallback(XEvent *event);

private:
  Image            *ref_image;
  Widget           *ref_parent;
  Widget            image_widget;
  ScreenImageParams ref_params;
  Pixmap            p;
  int               screen;
  int               NumberStars;
  StarCenters       *star_info;
  int               screen_circles_on;
  int               pixmap_depth;
  void              (*star_click_callback)(ScreenImage *si, int star_index);
  void              (*pixel_click_callback)(ScreenImage *si, int x, int y);

  void CircleStars(void);

  GC * get_color_gc(ScreenColor color);
  void SetScreenCircle(int x, int y, int radius, ScreenColor color);
  void SetScreenText(int x, int y, const char *string, ScreenColor color);
};

#endif
