/*  find_blob.cc -- Finds a bright star "blob"
 *
 *  Copyright (C) 2015, 2020 Mark J. Munkacsy

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
#include <unistd.h>		// getopt()
#include <stdio.h>
#include <list>

struct Blob {
  double center_row;
  double center_column;
  double pixel_radius; // 0.0 means just a row hit
  double total_flux;
  bool eaten_already;
  bool PointIsInsideBlob(double row, double column);
  bool BlobIsValid(void);
};

bool
Blob::PointIsInsideBlob(double row, double column) {
  const double delta_row = center_row - row;
  const double delta_col = center_column - column;

  return (pixel_radius*pixel_radius >=
	  delta_row*delta_row + delta_col*delta_col);
}

bool
Blob::BlobIsValid(void) {
  int avg_flux = total_flux/(pixel_radius*pixel_radius);
  return (pixel_radius < 30.0 &&
	  total_flux > 10000.0 &&
	  avg_flux > 100.0 &&
	  center_row > 5.0 &&
	  center_column > 5.0 &&
	  center_row < 505.0 &&
	  center_column < 505.0);
}

void usage(void) {
  fprintf(stderr,
	  "Usage: find_blob [-d dark.fits] [-s flat.fits] -i image.fits\n");
  exit(-2);
}

int main(int argc, char **argv) {
  int option_char;
  Image *image = 0;
  Image *dark = 0;
  int num_darks = 0;
  Image *flat = 0;
  char *image_filename = 0;

  while((option_char = getopt(argc, argv, "i:d:s:")) > 0) {
    switch (option_char) {
    case 's':			// scale image (flat field)
      flat = new Image(optarg);
      if(!flat) {
	fprintf(stderr, "Cannot open flatfield image %s\n", optarg);
	flat = 0;
      }
      break;

    case 'i':			// image file name
      if(image != 0) {
	fprintf(stderr, "find_blob: only one image file permitted.\n");
	usage();
      }
      fprintf(stderr, "find_blob: image file = '%s'\n", optarg);
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
      usage();
    }
  }

  // if there's no image to display, just quit
  if(image == 0) {
    fprintf(stderr, "find_blob: no image specified.\n");
    usage();
  }

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

  Statistics *stats = image->statistics();
  double threshold = stats->MedianPixel + stats->StdDev*4;

  fprintf(stderr, "Median pixel = %lf, StdDev = %lf, threshold = %lf\n",
	  stats->MedianPixel, stats->StdDev, threshold);

  std::list <Blob *> all_blobs;

  for (int row = 0; row < image->height; row++) {
    int start_blob = -1;
    double flux_sum;
    int end_blob;
    for (int column = 0; column < image->width; column++) {
      if (image->pixel(column, row) > threshold) {
	if (start_blob < 0) {
	  // starting new blob
	  start_blob = end_blob = column;
	  flux_sum = (image->pixel(column, row) - stats->MedianPixel);
	} else {
	  // adding to blob
	  end_blob = column;
	  flux_sum += (image->pixel(column, row) - stats->MedianPixel);
	}
      } else {
	if (start_blob >= 0) {
	  if (end_blob - start_blob >= 2) {
	    // this ends a blob
	    Blob *b = new Blob;
	    b->center_row = row;
	    b->center_column = (end_blob + start_blob)/2.0;
	    b->pixel_radius = 0.0;
	    b->total_flux = flux_sum;
	    b->eaten_already = false;
	    all_blobs.push_back(b);
	    fprintf(stderr, "blob in row %d running from col %d through %d\n",
		    row, start_blob, end_blob);
	  } 
	  // lone hot pixel
	  start_blob = -1;
	}
      }
    }
  }

  fprintf(stderr, "Total of %ld raw blobs found\n", all_blobs.size());

  std::list<Blob *>::iterator it;
  for (it = all_blobs.begin(); it != all_blobs.end(); it++) {
    Blob *b = (*it);
    if (b->eaten_already) continue;
    const double delta_radius = 2.0;
    b->total_flux = 0.0;

    double prior_flux;
    do {
      prior_flux = b->total_flux;

      b->total_flux = 0;
      double x_moment = 0.0;
      double y_moment = 0.0;
      b->pixel_radius += delta_radius;
      const double r_sq = b->pixel_radius*b->pixel_radius;

      int col_start = (int) (b->center_column - b->pixel_radius);
      if (col_start < 0) col_start = 0;

      int col_end = (int) (b->center_column + b->pixel_radius + 0.99);
      if (col_end >= image->width) col_end = image->width-1;

      int row_start = (int) (b->center_row - b->pixel_radius);
      if (row_start < 0) row_start = 0;

      int row_end = (int) (b->center_row + b->pixel_radius + 0.99);
      if (row_end >= image->height) row_end = image->height-1;

      for (int col = col_start; col <= col_end; col++) {
	const double offset_x = col - b->center_column;
	for (int row = row_start; row <= row_end; row++) {
	  const double offset_y = row - b->center_row;

	  // ignore the point if too far away
	  if (offset_x*offset_x + offset_y*offset_y > r_sq) continue;

	  const double net_flux = image->pixel(col, row) - stats->MedianPixel;
	  b->total_flux += net_flux;
	  x_moment += offset_x*net_flux;
	  y_moment += offset_y*net_flux;
	}
      }
      // recalculate center
      b->center_column += x_moment/b->total_flux;
      b->center_row    += y_moment/b->total_flux;
    } while(prior_flux < b->total_flux*0.95);

    bool invalid = !b->BlobIsValid();
    
    fprintf(stderr, "blob center at (%.1lf, %.1lf), flux = %lf, radius = %d%s\n",
	    b->center_column, b->center_row,
	    b->total_flux,
	    (int) (b->pixel_radius + 0.5),
	    (invalid ? " **INVALID**" : ""));

    if (invalid) continue;
    
    std::list<Blob *>::iterator it0;
    for (it0 = all_blobs.begin(); it0 != all_blobs.end(); it0++) {
      Blob *bb = (*it0);
      if (b == bb || bb->eaten_already) continue;
      if (b->PointIsInsideBlob(bb->center_row, bb->center_column)) {
	bb->eaten_already = true;
      }
    }
  }
  fprintf(stderr, "------------------- final list --------------\n");
  int row_min = 9999;
  int row_max = 0;
  int col_min = 9999;
  int col_max = 0;
  Blob *best_blob = 0;
  double best_flux = 0.0;
  
  for (it = all_blobs.begin(); it != all_blobs.end(); it++) {
    Blob *b = (*it);
    if (b->eaten_already) continue;
    if (!b->BlobIsValid()) continue;
    int x_min = b->center_column - b->pixel_radius;
    int x_max = b->center_column + b->pixel_radius;
    int y_min = b->center_row - b->pixel_radius;
    int y_max = b->center_row + b->pixel_radius;

    if (x_min < col_min) col_min = x_min;
    if (x_max > col_max) col_max = x_max;
    if (y_min < row_min) row_min = y_min;
    if (y_max > row_max) row_max = y_max;
    
    fprintf(stderr, "blob center at (%.1lf, %.1lf), flux = %lf, radius = %d\n",
	    b->center_column, b->center_row,
	    b->total_flux,
	    (int) (b->pixel_radius + 0.5));
    if (b->total_flux > best_flux) {
      best_flux = b->total_flux;
      best_blob = b;
    }
  }

  fprintf(stdout, "RESULT ");
  if (best_blob) {
    fprintf(stdout, "%.1lf %.1lf\n",
	    best_blob->center_column, best_blob->center_row);
  } else {
    fprintf(stdout, "INVALID\n");
  }

#if 0
  FILE *fp_csv = fopen("/tmp/blob.csv", "w");
  if (row_min < 0) row_min = 0;
  if (row_max >= image->height) row_max = image->height-1;
  if (col_min < 0) col_min = 0;
  if (col_max >= image->width) col_max = image->width-1;


  for (int col = col_min; col <= col_max; col++) {
    fprintf(fp_csv, ",%d", col);
  }
  for (int row = row_min; row <= row_max; row++) {
    fprintf(fp_csv, "\n%d", row);
    for (int col = col_min; col <= col_max; col++) {
      fprintf(fp_csv, ",%d", (int) (0.5+image->pixel(col, row)));
    }
  }
  fprintf(fp_csv, "\n");
  fclose(fp_csv);
#endif
}
