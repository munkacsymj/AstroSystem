/*  mini_win.h -- Implements class MiniWin to support show_sequence
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
#ifndef _MINI_WIN_H
#define _MINI_WIN_H

#include <Image.h>
#include <X11/Intrinsic.h>
#include <X11/Core.h>

#include "screen_image.h"


class MiniWin {
public:
  MiniWin(char *image_file,
	  Image *dark,
	  Image *flat,
	  Widget *parent,
	  ScreenImageParams *params,
	  int mini_width,
	  int mini_height);
  ~MiniWin(void);

  void SetTopLeftAndRedraw(int top, int left);
  void SetParams(ScreenImageParams *params);
  void ReDraw(void);

  // returns 1 if user set the select toggle
  int IsSelected(void);

  char *Image_filename(void) { return Image_file; }

  
private:
  int CurrentTop, CurrentLeft;
  char *Image_file;
  Image *Dark;
  Image *Flat;
  Widget *Parent;
  int min_width;
  int min_height;

  Image *min_image;
  ScreenImageParams *ref_params;

  Widget MainManager;
  Widget InfoManager;
  Widget SelectButton;
  Widget NameLabel;
  Widget MedianLabel;

  ScreenImage *screen_image;
};

#endif
