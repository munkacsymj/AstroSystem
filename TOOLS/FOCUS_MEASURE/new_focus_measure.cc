/*  new_focus_measure.cc -- Program to estimate star blur by stacking stars
 *  in an image and finding the 1/2-power point of the flux distribution.
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
#include <Image.h>
#include <IStarList.h>
#include <unistd.h>		// getopt()
#include <stdio.h>
#include "aperture_phot.h"
#include <gendefs.h>

#define HIST_ARRAY_SIZE 400
#define MAX_PIXELS_RADII 5
#define SUB_PIXEL_FACTOR 20

#define PROG_NAME "new_focus_measure"

struct cell_info {
  double radius;
  double total_flux;
};

cell_info x_array[HIST_ARRAY_SIZE];
cell_info y_array[HIST_ARRAY_SIZE];

void print_region(Image *image, double x_ref, double y_ref) {
  fprintf(stderr, "-----------------\n");
  fprintf(stderr, "Xcenter = %.2lf, Ycenter = %.2lf\n", x_ref, y_ref);
  int x, y;
  x = (int) x_ref - 2;
  y = (int) y_ref - 2;

  fprintf(stderr, "      %5d   %5d   %5d   %5d   %5d\n",
	  x, x+1, x+2, x+3, x+4);
  int i = 0;
  for(i = 0; i<5; i++) {
    fprintf(stderr, "%3d   %5d   %5d   %5d   %5d   %5d\n",
	    y+i,
	    (int) image->pixel(x, y+i),
	    (int) image->pixel(x+1, y+i),
	    (int) image->pixel(x+2, y+i),
	    (int) image->pixel(x+3, y+i),
	    (int) image->pixel(x+4, y+i));
  }
  fprintf(stderr, "\n");
}

void AddToArray(cell_info *array,
		double radius,
		double flux) {
  double index_real = radius *HIST_ARRAY_SIZE/MAX_PIXELS_RADII;
  int index_low = (int) index_real;
  double interp_factor = index_real - index_low;

  // fprintf(stderr, "put %.1lf into cell %d\n", (1.0-interp_factor)*flux, index_low);
  // fprintf(stderr, "put %.1lf into cell %d\n", (interp_factor)*flux, index_low+1);
  array[index_low].total_flux += ((1.0-interp_factor)*flux);
  array[index_low+1].total_flux += (interp_factor*flux);
}

int main(int argc, char **argv) {
  int option_char;
  char *image_file = 0;
  Image *image = 0;
  char *dark = 0;
  char *flat = 0;
  FILE *output_fp = 0;
  int print_array = 0;		// debuggging info
  double total_flux= 0.0;

  while((option_char = getopt(argc, argv, "ai:d:s:o:")) > 0) {
    switch (option_char) {
    case 'a':
      print_array++;
      break;

    case 's':			// scale image (flat field)
      flat = optarg;
      break;

    case 'i':			// image file name
      if(image_file != 0) {
	fprintf(stderr, "%s: only one image file permitted.\n", PROG_NAME);
	exit(2);
      }
      fprintf(stderr, "%s: image file = '%s'\n", PROG_NAME, optarg);
      image_file = optarg;
      break;

    case 'd':			// dark file name
      dark = optarg;
      fprintf(stderr, "%s: dark file = '%s'\n", PROG_NAME, optarg);
      break;

    case 'o':
      output_fp = fopen(optarg, "w");
      if(!output_fp) {
	fprintf(stderr, "%s: cannot open output file %s\n", PROG_NAME, optarg);
      }
      break;

    case '?':			// invalid argument
    default:
      fprintf(stderr, "Invalid argument.\n");
      exit(2);
    }
  }

  if(output_fp == 0) output_fp = stdout;

  if(image_file == 0) {
    fprintf(stderr, "%s: no image specified with -i\n", PROG_NAME);
    exit(-2);
  }

  char dark_param[128];
  char flat_param[128];

  if(dark) {
    sprintf(dark_param, "-d %s ", dark);
  } else {
    dark_param[0] = 0;
  }
  
  if(flat) {
    sprintf(flat_param, "-s %s ", flat);
  } else {
    flat_param[0] = 0;
  }

  {
    char command[512];
    sprintf(command, "find_stars %s %s -i %s", dark_param, flat_param, image_file);
    if(system(command)) {
      fprintf(stderr, "find_stars command failed.\n");
    }
    image = new Image(image_file);
    IStarList *orig_star_list = image->GetIStarList();
    const char *this_image_name = "/tmp/imageq0.fits";
    unlink(this_image_name); // in case already exists
    image->WriteFITS(this_image_name);
    orig_star_list->SaveIntoFITSFile(this_image_name, 1);

    char photometry_command[256];
    sprintf(photometry_command, COMMAND_DIR "/photometry -i %s\n",
	    this_image_name);
    if(system(photometry_command)) {
      fprintf(stderr, "photometry command failed.\n");
    }

    delete image;
    image = new Image(this_image_name);
  }

  Statistics *stats = image->statistics();
  IStarList *star_list = image->GetIStarList();

  // clear out the resulting arrays
  int i;
  for(i=0; i<HIST_ARRAY_SIZE; i++) {
    y_array[i].radius = x_array[i].radius =
      i * ((double) MAX_PIXELS_RADII)/((double) HIST_ARRAY_SIZE);
    y_array[i].total_flux = x_array[i].total_flux = 0.0;
  }

  // loop through all stars
  int star;
  double ref_flux = stats->MedianPixel;
  int stars_used = 0;

  for(int v=0; v<star_list->NumStars; v++)
    aperture_measure(image, v, star_list);

  for(star = 0; star < star_list->NumStars; star++) {
  // for(star = 0; star<10; star++) {
    IStarList::IStarOneStar *this_star = star_list->FindByIndex(star);

    // fprintf(stderr, "star %s: nlls_count = %.1lf\n", this_star->StarName, this_star->nlls_counts);
    
    if((this_star->validity_flags & COUNTS_VALID) == 0) continue;
    if((this_star->validity_flags & NLLS_FOR_XY) == 0) continue;
    if(this_star->nlls_counts < 200) continue;
    stars_used++;
    //double star_weight = 1000.0/this_star->nlls_counts;
    double star_weight = 1.0;

    double ref_x = this_star->nlls_x;
    double ref_y = this_star->nlls_y;
    int int_ref_x = (int) (ref_x + 0.5);
    int int_ref_y = (int) (ref_y + 0.5);

    // print_region(image, ref_x, ref_y);

    int pixel_x, pixel_y;
    for(pixel_y = int_ref_y - MAX_PIXELS_RADII;
	pixel_y <= int_ref_y + MAX_PIXELS_RADII; pixel_y++) {
      for(pixel_x = int_ref_x - MAX_PIXELS_RADII;
	  pixel_x <= int_ref_x + MAX_PIXELS_RADII; pixel_x++) {

	const double total_pixel_value =
	  star_weight * (image->pixel(pixel_x, pixel_y) - ref_flux);

	int del_x, del_y;
	for(del_y = 0; del_y < SUB_PIXEL_FACTOR; del_y++) {
	  double y_precise = pixel_y +
	    ((del_y - (SUB_PIXEL_FACTOR-1))/(2.0*SUB_PIXEL_FACTOR));
	  // if we fall off the edge of the image, skip the pixel
	  if(y_precise < 0.0) continue;
	  if(y_precise > image->height) continue;

	  double offset_y = y_precise - ref_y;
	  for(del_x = 0; del_x < SUB_PIXEL_FACTOR; del_x++) {
	    double x_precise = pixel_x +
	      ((del_x - (SUB_PIXEL_FACTOR-1))/(2.0*SUB_PIXEL_FACTOR));
	    double offset_x = x_precise - ref_x;
	    double radius = sqrt(offset_y*offset_y + offset_x*offset_x);
	    if(radius > MAX_PIXELS_RADII) continue;

	    double x_fraction = 0.707;
	    double y_fraction = 0.707;
	    if(radius > 0.0) {
	      y_fraction = fabs(offset_y/radius);
	      x_fraction = fabs(offset_x/radius);
	    }
	
	    AddToArray(x_array, radius, x_fraction * total_pixel_value);
	    AddToArray(y_array, radius, y_fraction * total_pixel_value);
	  }
	}
      }
    }
  } // end for all stars

  double median_x = 0.0;
  double median_y = 0.0;
  
  if(stars_used >= 1) {

    if(print_array) {
      fprintf(stderr, "Radius  x-cell   x-cum    y-cell   y-cum\n");
      int j;
      double cum_x = 0.0;
      double cum_y = 0.0;
      for(j=0; j<HIST_ARRAY_SIZE; j++) {
	cum_x += x_array[j].total_flux;
	cum_y += y_array[j].total_flux;
      
	fprintf(stderr, "%6.2lf  %.0lf  %.0lf  %.0lf  %.0lf\n",
		j*((double) MAX_PIXELS_RADII)/((double) HIST_ARRAY_SIZE),
		x_array[j].total_flux, cum_x,
		y_array[j].total_flux, cum_y);
      }
    }

    double cum_x = 0.0;
    double cum_y = 0.0;
    int j;
    for(j=0; j<HIST_ARRAY_SIZE; j++) {
      cum_x += x_array[j].total_flux;
      cum_y += y_array[j].total_flux;
    }
    total_flux = cum_x + cum_y;

    for(j=0; j<HIST_ARRAY_SIZE; j++) {
      median_x += x_array[j].total_flux;
      if(median_x >= cum_x/2.0) {
	median_x = j*((double) MAX_PIXELS_RADII)/((double) HIST_ARRAY_SIZE);
	break;
      }
    }

    for(j=0; j<HIST_ARRAY_SIZE; j++) {
      median_y += y_array[j].total_flux;
      if(median_y >= cum_y/2.0) {
	median_y = j*((double) MAX_PIXELS_RADII)/((double) HIST_ARRAY_SIZE);
	break;
      }
    }
  } else {
    median_x = median_y = -1.0;
  }

  fprintf(stderr, "new_focus_measure: %d stars (%d used)\n",
	  star_list->NumStars, stars_used);

  fprintf(output_fp, "X starwidth: %.2lf   Y starwidth: %.2lf\n",
	  median_x, median_y);
  fprintf(output_fp, "SNR: %.2lf\n",
	  total_flux/stats->StdDev);
}
