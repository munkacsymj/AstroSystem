/*  fits_to_png.cc -- Program to convert a FITS image to a .png image
 *
 *  Copyright (C) 2017 Mark J. Munkacsy
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

#include <iostream>
#include <stdio.h>
#include <unistd.h> 		// for getopt(), getpid()
#include <stdlib.h>		// for atof()
#include <Image.h>
#include <HGSC.h>
#include <Magick++.h>
#include <gendefs.h>

#define QuantumRange ((Magick::Quantum) 65535)

void draw_circles(const char *hgsc_starname,
		  Image *image,
		  bool color_circles,
		  bool display_starnames,
		  bool circle_only_brightest,
		  bool circle_only_comps,
		  Magick::Image *m_image);

void usage(void) {
      fprintf(stderr,
	      "usage: fits_to_png [-s flat.fits] [-d dark.fits] [-u nn -l nn] -i image.fits -o image.png\n");
      exit(-2);
}

int main(int argc, char **argv) {
  int ch;			// option character
  Image *image = 0;
  Image *dark = 0;
  Image *flat = 0;
  double   max_pixel_value = -1.0;
  double   min_pixel_value = -1.0;
  double   span_interval = -1.0;
  double   value_offset = -999.0;
  bool circle_stars = false;
  bool color_circles = false;
  bool circle_only_comps = false;
  bool display_starnames = false;
  bool circle_only_brightest = false;
  char *hgsc_starname = 0; // used to find correct hgsc catalog

  char *image_filename = 0;	// filename of the .fits image file
  char *output_filename = 0;

  // Command line options:
  // -i imagefile.fits
  // -o outputfile.png     Write fits_to_png into different file
  // -u upper-scaling-limit (white pixel)
  // -l lower-scaling-limit (black pixel)
  // -q span_interval (in asinh() space)
  // -v value_offset (in asinh() space)
  // -c                    Circle stars
  // -a                    Apply color rules to star circles
  // -x                    Display starnames
  // -n starname           HGSC starname
  // -b                    Circle the 10 brightest only
  // -z                    Circle only comp/check stars and variables

  while((ch = getopt(argc, argv, "zbxcan:i:o:u:l:d:s:q:v:")) != -1) {
    switch(ch) {
    case 'z':
      circle_only_comps = true;
      break;

    case 'b':
      circle_only_brightest = true;
      break;

    case 'c':
      circle_stars = true;
      break;

    case 'a':
      color_circles = true;
      break;

    case 'x':
      display_starnames = true;
      break;

    case 'n':
      hgsc_starname = optarg;
      break;

    case 'v':
      value_offset = atof(optarg);
      break;

    case 'q':
      span_interval = atof(optarg);
      break;

    case 'u':
      max_pixel_value = (double) atoi(optarg);
      break;

    case 's':			// scale image (flat field)
      flat = new Image(optarg);
      if(!flat) {
	fprintf(stderr, "Cannot open flatfield image %s\n", optarg);
	flat = 0;
      }
      break;

    case 'l':
      min_pixel_value = (double) atoi(optarg);
      break;

    case 'i':			// image file name
      if(image != 0) {
	fprintf(stderr, "fits_to_png: only one image file permitted.\n");
	exit(2);
      }
      fprintf(stderr, "fits_to_png: image file = '%s'\n", optarg);
      image_filename = optarg;
      image = new Image(image_filename);
      break;

    case 'd':			// dark file name
      dark = new Image(optarg);

      fprintf(stderr, "fits_to_png: dark file = '%s'\n", optarg);
      break;

    case 'o':
      output_filename = optarg;
      break;

    case '?':
    default:
      usage();
   }
  }

  Magick::InitializeMagick(*argv);

  //****************************************************************

#if 0
  {
    // load an image
    Magick::Image image("/tmp/test.jpg");
    int w = image.columns();
    int h = image.rows();

    // get a "pixel cache" for the entire image
    Magick::PixelPacket *pixels = image.getPixels(0, 0, w, h);

    // now you can access single pixels like a vector
    int row = 0;
    int column = 0;
    Magick::Color color = pixels[w * row + column];

    // if you make changes, don't forget to save them to the
    // underlying image

    const unsigned char *s = (const unsigned char *) pixels;
    for (int n=0; n < 16; n++) {
      for (int j=0; j < 16; j++) {
	fprintf(stderr, "%02x ", *s++);
      }
      fprintf(stderr, "\n");
    }
    
    fprintf(stderr, "initial value (RGB) = (%d,%d,%d)\n",
	    pixels[0].red, pixels[0].green, pixels[0].blue);
    pixels[0].red = 65535;
    pixels[0].green = 65535;
    pixels[0].blue = 65535;
    pixels[1].red = 0;
    pixels[1].green = 0;
    pixels[1].blue = 0;
    pixels[2].red = 65535;
    pixels[2].green = 0;
    pixels[2].blue = 0;
    pixels[3].red = 0;
    pixels[3].green = 0;
    pixels[3].blue = 65535;
    pixels[4].red = 0;
    pixels[4].green = 65535;
    pixels[4].blue = 0;
    image.syncPixels();

    // ...and maybe write the image to file.
    image.write("/tmp/test_modified.png");
  }
  exit(0);
#endif

  //****************************************************************

  // both input file and output file are mandatory
  if(image_filename == 0 || output_filename == 0) {
    usage();
  }

  if(max_pixel_value == -1 || min_pixel_value == -1) {
    double median = image->statistics()->MedianPixel;
    min_pixel_value = median - 55.0;
    if(min_pixel_value < 0.0) min_pixel_value = 0.0;
    max_pixel_value = min_pixel_value + 4400.0;
  }
  fprintf(stderr, "Median pixel value = %.1lf\n",
	  image->statistics()->MedianPixel);
  fprintf(stderr, "Using min_pixel = %.1lf, max_pixel = %.1lf\n",
	  min_pixel_value, max_pixel_value);

  // dark-subtract and flat-field the image
  if (dark)
    image->subtract(dark);
  if (flat)
    image->scale(flat);

  const double alpha = 2.0;
  // const double alpha = (max_pixel_value - min_pixel_value)/2200.0;
  const double lim0 = asinh(min_pixel_value/alpha);
  //const double lim0 = asinh(0.0);
  const double lim99 = asinh((max_pixel_value-min_pixel_value)/alpha);
  const double span = (lim99 - lim0);
  fprintf(stderr, "span = %lf\n", span);

  double min_value = 9999.9;
  double max_value = 0.0;
  
  // "scaled" starts off with the asinh-converted pixel values in
  // it. 
  Image scaled(image->height, image->width);

  for(int row=0; row < image->height; row++) {
    for(int column=0; column < image->width; column++) {
      double v = image->pixel(column, row) - min_pixel_value;
      //if (v < 0.0) v = 0.0;
      double v0 = asinh(v/alpha);
      v0 = (v0 - lim0)/span;
      //v0 = v;

      if (v0 > max_value) max_value = v0;
      if (v0 < min_value) min_value = v0;

      scaled.pixel(column, row) = v0;
    }
  }

  //min_value = lim0;
  //max_value = lim99;
  fprintf(stderr, "min value = %lf, max_value = %lf\n",
	  min_value, max_value);
  double value_span = (max_value - min_value);

  if (span_interval >= 0.0) {
    fprintf(stderr, "Using span = %lf instead of %lf\n",
	    span_interval, value_span);
    value_span = span_interval;
  }

  Magick::Image m_image(Magick::Geometry(image->width,
					 image->height), "gray");

  m_image.classType(Magick::DirectClass);
  m_image.colorSpace(Magick::RGBColorspace);
  m_image.magick("RGB");
  m_image.modifyImage();
  Magick::PixelPacket *pixel_cache = m_image.getPixels(0,0,
						   image->width,image->height);

#if 0
  const unsigned char *s = (const unsigned char *) pixel_cache;
  for (int n=0; n < 16; n++) {
    for (int j=0; j < 16; j++) {
      fprintf(stderr, "%02x ", *s++);
    }
    fprintf(stderr, "\n");
  }
#endif

  if (value_offset > -900.0) {
    min_value += value_offset;
  }

  Image converted_image(image->height, image->width);
  
  for(int row=0; row < image->height; row++) {
    for(int column=0; column < image->width; column++) {
      double pixel = scaled.pixel(column, row) - min_value;
      if (pixel < 0.0) pixel = 0.0;
      unsigned int val = (unsigned int)
	(65535.0 * pixel / value_span);
      if (val < 0) val = 0;
      if (val > 65535) val = 65535;
      
      //double d_val = (scaled.pixel(column, row) - min_value)/ value_span;
      pixel_cache[image->width*row+column].red = val;
      pixel_cache[image->width*row+column].blue = val;
      pixel_cache[image->width*row+column].green = val;

      converted_image.pixel(column, row) = val;
    }
  }

  fprintf(stderr, "Final image median pixel = %.0lf\n",
	  converted_image.statistics()->MedianPixel);

  //std::cout << "val: " << pixel_cache[image->width*50+50].red << std::endl;
  
  m_image.syncPixels();

  if (circle_stars) {
    draw_circles(hgsc_starname, image, color_circles, display_starnames,
		 circle_only_brightest, circle_only_comps, &m_image);
  }
  
  m_image.write(output_filename);
  //m_image.display();
}

  

void draw_circles(const char *hgsc_starname,
		  Image *image,
		  bool color_circles,
		  bool display_starnames,
		  bool circle_only_brightest,
		  bool circle_only_comps,
		  Magick::Image *m_image) {

  Magick::Color variable_circle_color("red");
  Magick::Color comp_star_circle_color("green");
  Magick::Color correlated_circle_color("orange");
  Magick::Color anon_star_circle_color("yellow");
  Magick::Color no_fill(0, 0, 0, QuantumRange); // transparent

  if (!color_circles) {
    variable_circle_color = Magick::Color("yellow");
    comp_star_circle_color = Magick::Color("yellow");
    anon_star_circle_color = Magick::Color("yellow");
    correlated_circle_color = Magick::Color("yellow");
  }

  m_image->fillColor(no_fill);
  m_image->strokeWidth(1.0);

  if (hgsc_starname == 0 || *hgsc_starname == 0) {
    fprintf(stderr, "Missing starname (-n starname)\n");
    usage();
  }

  char HGSCfilename[132];
  sprintf(HGSCfilename, CATALOG_DIR "/%s", hgsc_starname);
  FILE *HGSC_fp = fopen(HGSCfilename, "r");
  if (!HGSC_fp) {
    fprintf(stderr, "Cannot open catalog file for %s\n", hgsc_starname);
    usage();
  }

  IStarList *list = image->PassiveGetIStarList();
  HGSCList Catalog(HGSC_fp);

  int num_circles_drawn = 0;

  if (circle_only_brightest) {
    IStarList::IStarOneStar *dimmest_in_list = 0;
    double dimmest_magnitude = 99.9;
    
    std::list<IStarList::IStarOneStar *> brightest;
    for(int i=0; i < list->NumStars; i++) {
      IStarList::IStarOneStar *oneStar = list->FindByIndex(i);

      if (oneStar->validity_flags & PHOTOMETRY_VALID) {
	if (oneStar->photometry < dimmest_magnitude ||
	    brightest.size() < 10) {
	  brightest.push_back(oneStar);
	  if (brightest.size() > 10) {
	    if (dimmest_in_list) brightest.remove(dimmest_in_list);
	    
	    std::list<IStarList::IStarOneStar *>::iterator it;
	    dimmest_magnitude = 99.9;
	    for (it = brightest.begin(); it != brightest.end(); it++) {
	      if ((*it)->photometry < dimmest_magnitude) {
		dimmest_magnitude = (*it)->photometry;
		dimmest_in_list = (*it);
	      }
	    }
	  }
	}
      }
    }

    std::list<IStarList::IStarOneStar *>::iterator it;
    for (it = brightest.begin(); it != brightest.end(); it++) {
      IStarList::IStarOneStar *oneStar = (*it);
      
      const int x = (int) (oneStar->nlls_x + 0.5);
      const int y = (int) (oneStar->nlls_y + 0.5);
      const int radius = 5; // pixels

      m_image->strokeColor(anon_star_circle_color);
      Magick::DrawableArc circle(x-radius, y-radius,
				 x+radius, y+radius,
				 0, 360);
      
      m_image->draw(circle);
      num_circles_drawn++;
    }
  } else {
    // don't just circle the brightest
    for(int i=0; i < list->NumStars; i++) {
      IStarList::IStarOneStar *oneStar = list->FindByIndex(i);
      HGSC *cat_entry = 0;
      bool do_draw = true;

      if (oneStar->validity_flags & CORRELATED) {
	cat_entry = Catalog.FindByLabel(oneStar->StarName);
      }

      const int x = (int) (oneStar->nlls_x + 0.5);
      const int y = (int) (oneStar->nlls_y + 0.5);
      const int radius = 5; // pixels

      const char *this_name = oneStar->StarName;
      // if this star was correlated and we could find the correlated
      // HGSC entry...
      if (cat_entry) {
	bool is_comp = (cat_entry->is_check || cat_entry->is_comp);
	bool is_submittable = cat_entry->do_submit;

	m_image->strokeColor(correlated_circle_color);
	if (is_comp) {
	  m_image->strokeColor(comp_star_circle_color);
	}
	if (is_submittable) {
	  m_image->strokeColor(variable_circle_color);
	}

	if (circle_only_comps &&
	    (is_comp == false && is_submittable == false)) {
	  do_draw = false;
	}
      
      } else {
	m_image->strokeColor(anon_star_circle_color);
	if (circle_only_comps) do_draw = false;
      }

      if (do_draw) {
	Magick::DrawableArc circle(x-radius, y-radius,
				   x+radius, y+radius,
				   0, 360);
      
	m_image->draw(circle);
	num_circles_drawn++;
      }
    }
  }
}

      
    
  
