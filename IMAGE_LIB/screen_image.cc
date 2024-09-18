/*  screen_image.cc -- Implements X-window display of an image.
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
#include "screen_image.h"
#include <X11/Xaw/Command.h>

static unsigned long assign_color_value(Widget w,
					double intensity) {
  XcmsColor color_spec;

  color_spec.format = XcmsRGBiFormat;
  color_spec.spec.RGBi.red   = intensity;
  color_spec.spec.RGBi.green = intensity;
  color_spec.spec.RGBi.blue  = intensity;
  if(0 == XcmsAllocColor(XtDisplay(w),
			 DefaultColormap(XtDisplay(w),
					 DefaultScreen(XtDisplay(w))),
			 &color_spec, XcmsRGBFormat)) {
    fprintf(stderr, "get_color_value failed for intensity = %f\n", intensity);
  }
  return color_spec.pixel;
}

#define INTENSITY_LEVELS 250
static unsigned long pixel_value(Widget w, double intensity) {
  static int initialized = 0;
  static unsigned long pixel_array[INTENSITY_LEVELS];
  int index_value = (int) (intensity*INTENSITY_LEVELS + 0.5);

  if(!initialized) {
    int j;
    initialized = 1;

    for(j=0; j<INTENSITY_LEVELS; j++) {
      pixel_array[j] = 
	assign_color_value(w, ((double)j)/((double) INTENSITY_LEVELS));
    }
  }

  if(index_value >= INTENSITY_LEVELS) index_value = (INTENSITY_LEVELS-1);
  if(index_value < 0) index_value = 0;

  return pixel_array[index_value];
}

unsigned long new_color(Widget w, const char *color_name) {
  XColor color_spec_hw, color_spec_exact;

  if(0 == XAllocNamedColor(XtDisplay(w),
			   DefaultColormap(XtDisplay(w),
					   DefaultScreen(XtDisplay(w))),
			   color_name,
			   &color_spec_hw,
			   &color_spec_exact)) {
    fprintf(stderr, "XAllocNamedColor() failed for '%s'\n", color_name);
  }
  return color_spec_hw.pixel;
}

void
ScreenImage::DisplayImage(Image *image, ScreenImageParams params) {
  ref_image = image;
  ref_params = params;
  DisplayImage();
}

void ScreenImage::DisplayImage(void) {
  XImage *raw_image_pointer;

  raw_image_pointer =
    XCreateImage(XtDisplay(image_widget),
		 XDefaultVisual(XtDisplay(image_widget), 0),
		 pixmap_depth,
		 ZPixmap, 0,
		 // (char *) malloc(((pixmap_depth+7)/8)*
		 (char *) malloc(4*(ref_image->height+4)*(ref_image->width+4)),
		 ref_image->width,
		 ref_image->height,
		 XBitmapPad(XtDisplay(image_widget)),
		 0);
  if(raw_image_pointer == 0) {
    fprintf(stderr, "XCreateImage() failed.\n");
  } else {
    /* draw from the data array in the file into the XImage
       and stretch the brightness at the same time. We must be
       careful here because X will perform internal alignment
       in the image.  We must query the image to find out the
       number of bytes per line (per row) in the image. */
    double DarkestPixel = ref_params.black_value;
    //double image_scale = ref_params.white_value - DarkestPixel;

    //#define LINEAR
#ifndef LINEAR // not LINEAR means ARCSINH
    const double alpha = 2.0;
    const double lim0 = asinh(DarkestPixel/alpha);
    const double lim99 = asinh(ref_params.white_value/alpha);
    const double span = (lim99 - lim0);
#endif

    GC defaultGC = XDefaultGC(XtDisplay(image_widget), 0);
    int row, column;

    for(row=0; row < ref_image->height; row++) {
      for(column=0; column < ref_image->width; column++) {
	double v = ref_image->pixel(column, row);
	double v0 = 0.0;
#ifdef LINEAR
	v0 = (v - DarkestPixel)/image_scale;
#else // arcsinh()
	v0 = asinh(v/alpha);
	v0 = (v0 - lim0)/span;
#endif
	
	XPutPixel(raw_image_pointer, column, row,
		  pixel_value(image_widget, v0));
      }
    }

    /* now that we have put the picture into the Image, we
       will copy the image into the pixmap */
    XPutImage(XtDisplay(image_widget),
	      p,
	      defaultGC,
	      raw_image_pointer, 0, 0, 0, 0,
	      raw_image_pointer->width,
	      raw_image_pointer->height);
    /* at this point we can destroy the image.  It isn't
       needed for expose refreshes, since the pixmap is used
       instead. */
    XDestroyImage(raw_image_pointer);

    if(screen_circles_on) CircleStars();
  }
}

ScreenImage::ScreenImage(Image *image,
			 Widget *parent,
			 ScreenImageParams *params) {
  ref_image  = image;
  ref_parent = parent;
  ref_params = *params;
  NumberStars = 0;
  screen_circles_on = 0;
  star_click_callback = 0;
  pixel_click_callback = 0;

  XtVaGetValues(*parent, XtNdepth, &pixmap_depth, NULL);
  screen = DefaultScreen(XtDisplay(*parent));

  fprintf(stderr, "Creating pixmap with width=%d, height=%d\n",
	  image->width, image->height);
  p = XCreatePixmap(XtDisplay(*parent),
		    RootWindowOfScreen(XtScreen(*parent)),
		    image->width,
		    image->height,
		    pixmap_depth);

  image_widget = XtVaCreateManagedWidget("bitmapArea",
					 labelWidgetClass,
					 *parent,
					 XtNwidth, image->width,
					 XtNheight, image->height,
					 XtNinternalWidth, 0,
					 XtNinternalHeight, 0,
					 XtNborderWidth, 0,
					 XtNbitmap, p,
					 NULL);

  DisplayImage(ref_image, ref_params);
}

void
ScreenImage::DrawScreenImage(void) {
  XCopyArea(XtDisplay(image_widget),
	    p,			// copy from pixmap . . .
	    XtWindow(image_widget),	// to "hello" widget
	    XDefaultGC(XtDisplay(image_widget), 0),
	    0,		// src_x
	    0,		// src_y
	    ref_image->width,
	    ref_image->height,
	    0,		// dest_x
	    0);		// dest_y
}

ScreenImage::~ScreenImage(void) {
  delete ref_image;
}

void 
ScreenImage::CircleStars(void) {
  fprintf(stderr, "ScreenImage: circling %d stars\n", NumberStars);
  for(int i = 0; i < NumberStars; i++) {
    if(star_info[i].enable) {
      SetScreenCircle((int) (star_info[i].x+0.5),
		      (int) (star_info[i].y+0.5),
		      (int) (star_info[i].radius+0.5),
		      star_info[i].color);
      if(star_info[i].label && star_info[i].enable_text) {
	SetScreenText((int) (star_info[i].x+4.0),
		      (int) (star_info[i].y+4.0),
		      star_info[i].label,
		      ScreenYellow);
      }
    }
  }
}

GC *
ScreenImage::get_color_gc(ScreenColor color) {
  static GC red_box_gc;
  static GC yellow_box_gc;
  static GC green_box_gc;
  static GC cyan_box_gc;
  
  static int first_time = 1;
  
  // The first time this is executed we create a graphics context that
  // will be used whenever we want to draw red circles

  if(first_time) {
    XGCValues gc_values;
    first_time = 0;

    gc_values.foreground = new_color(image_widget, "red");
    gc_values.line_width = 1;	// 1 pixel is visible

    red_box_gc = XCreateGC(XtDisplay(image_widget),
			   p,
			   GCForeground | GCLineWidth,
			   &gc_values);

    gc_values.foreground = new_color(image_widget, "yellow");
    yellow_box_gc = XCreateGC(XtDisplay(image_widget),
			      p,
			      GCForeground,
			      &gc_values);

    gc_values.foreground = new_color(image_widget, "green");
    green_box_gc = XCreateGC(XtDisplay(image_widget),
			      p,
			      GCForeground,
			      &gc_values);

    gc_values.foreground = new_color(image_widget, "cyan");
    cyan_box_gc = XCreateGC(XtDisplay(image_widget),
			    p,
			    GCForeground,
			    &gc_values);
  }

  GC *return_value;

  switch(color) {
  case ScreenRed:
    return_value = &red_box_gc;
    break;
    
  case ScreenCyan:
    return_value = &cyan_box_gc;
    break;

  case ScreenYellow:
    return_value = &yellow_box_gc;
    break;

  case ScreenGreen:
    return_value = &green_box_gc;
    break;

  default:
    return_value = 0;
    break;
  }

  return return_value;
}
    

/********************************************************************/  
/*        draw_red_circle() puts a red circle onto the specified    */
/*        pixmap.                                                   */
/********************************************************************/  
void
ScreenImage::SetScreenCircle(int x, int y,
			     int radius,
			     ScreenColor color) {
  GC *thisColorGC = get_color_gc(color);
  
  XDrawArc(XtDisplay(image_widget), p, *thisColorGC,
	   x-radius, y-radius,	// center x,y
	   radius*2, radius*2,	// circle size: 6 pixels
	   0, 360*64);		// complete circle
}

void 
ScreenImage::SetScreenText(int x, int y,
			   const char *string,
			   ScreenColor color) {
  GC *thisColorGC = get_color_gc(color);

  XDrawString(XtDisplay(image_widget), p, *thisColorGC,
	      x, y, string, strlen(string));
}

void SIClickEvent(Widget W,
		   XtPointer client_data,
		   XEvent *event,
		   Boolean *continue_dispatch) {
  ScreenImage *si;
  si = (ScreenImage *) client_data;
  if(si) si->PerformClickCallback(event);
}

void
ScreenImage::PerformClickCallback(XEvent *event) {
  if(star_click_callback || pixel_click_callback) {
    int button = event->xbutton.button;

    if(event->xbutton.type == ButtonPress && button == Button1) {
      int ImageX = event->xbutton.x;
      int ImageY = event->xbutton.y;

      if (pixel_click_callback) {
	(pixel_click_callback)(this, ImageX, ImageY);
      } else if (star_click_callback) {
	int star_no;
	int closest_star_index = -1;
	double closest_distance = 1.0e99;
	for(star_no = 0; star_no < NumberStars; star_no++) {
	  const double dist_x = (star_info[star_no].x - ImageX);
	  const double dist_y = (star_info[star_no].y - ImageY);
	  const double dist_sq = dist_x*dist_x + dist_y*dist_y;
	  if(dist_sq < closest_distance) {
	    closest_distance = dist_sq;
	    closest_star_index = star_no;
	  }
	}

	if(closest_star_index >= 0)
	  (star_click_callback)(this, closest_star_index);
      }
    }
  } else {
    fprintf(stderr, "IMAGE_LIB/screen_image.cc: SIClickEvent: error?\n");
  }
}

void
ScreenImage::SetStarClickCallback(void (*callback)(ScreenImage *si, int star_index)) {

  star_click_callback = callback;
  XtAddEventHandler(image_widget,
		    ButtonPressMask,
		    False,
		    SIClickEvent,
		    (XtPointer)this);
}

void
ScreenImage::SetPixelClickCallback(void (*callback)(ScreenImage *si, int x, int y)) {
  pixel_click_callback = callback;
  XtAddEventHandler(image_widget,
		    ButtonPressMask,
		    False,
		    SIClickEvent,
		    (XtPointer)this);
}

