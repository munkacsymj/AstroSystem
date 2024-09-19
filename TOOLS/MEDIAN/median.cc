/*  median.cc -- Five programs that take images and add, subtract,
 *  average, and median-combine them.
 *
 *  Copyright (C) 2007, 2021 Mark J. Munkacsy
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
#include <string.h>
#include <stdio.h>
#include <libgen.h>		// for basename()
#include <unistd.h> 		// for getopt()
#include <stdlib.h>		// for atof()
#include <Image.h>
#include <list>
#include <string>

// MEDIAN_AVERAGE means taking the average of each pixel of the set of
// images after rejecting the brightest and the dimmest value of each pixel.
Image *median_image(Image **i_array, int num_images, int med_avg);
void CarryForwardKeywords(Image **i_array,
			  int num_images,
			  Image *final_image);

int main(int argc, char **argv) {
  int ch;			// option character
  char *image_filename = 0;	// filename of the output .fits image file
  char *flatfield_filename = 0;
  char *dark_filename = 0;
  bool write_float = false;
  bool suppress_statistics = false;
  bool linearize = false;
  bool remove_shutter_gradient = false;
  
  // This program can be invoked by any of several names. Each name
  // corresponds to a different operation that will be performed.
  enum { EXEC_MEDIAN,
	 EXEC_AVERAGE,
	 EXEC_MEDIAN_AVERAGE,
	 EXEC_SUBTRACT,
	 EXEC_ADD,
	 EXEC_UNKNOWN } exec_mode = EXEC_UNKNOWN;

  char *exec_name = basename(argv[0]);
  if(strcmp(exec_name, "median") == 0) {
    exec_mode = EXEC_MEDIAN;
  }
  if(strcmp(exec_name, "medianaverage") == 0) {
    exec_mode = EXEC_MEDIAN_AVERAGE;
  }
  if(strcmp(exec_name, "average") == 0) {
    exec_mode = EXEC_AVERAGE;
  }
  if(strcmp(exec_name, "subtract") == 0) {
    exec_mode = EXEC_SUBTRACT;
  }
  if(strcmp(exec_name, "add") == 0) {
    exec_mode = EXEC_ADD;
  }

  if(exec_mode == EXEC_UNKNOWN) {
    fprintf(stderr, "median: unknown invoked name = '%s'\n", argv[0]);
    exit(2);
  }

  // Command line options:
  //
  // [-d bias.fits] [-n] [-f] -s flatfield_file.fits -o filename.fits   Image file (output)
  // all other arguments are taken as names of files to be included in
  // the *median* operation. "-f" specifies output file to be written
  // in floating point FITS. "-n" suppresses calculation and printing of image statistics.
  // [-l] linearize to reverse effects of ST-9 nonlinearity
  // [-g] remove shutter gradient
  //

  while((ch = getopt(argc, argv, "glnd:fs:o:")) != -1) {
    switch(ch) {
    case 'g':
      remove_shutter_gradient = true;
      break;
      
    case 'l':
      linearize = true;
      break;
      
    case 'n':
      suppress_statistics = true;
      break;
      
    case 'f':			// floating point final image
      write_float = true;
      break;
      
    case 'o':			// image filename
      image_filename = optarg;
      break;

    case 'd':
      dark_filename = optarg;	// dark (bias) filename
      break;

    case 's':			// scale (flatfield) filename
      flatfield_filename = optarg;
      break;

    case '?':
    default:
      fprintf(stderr,
	      "usage: %s [-l] [-g] [-s flatfield.fits] -o outputimage_filename.fits \n",
	      argv[0]);
      return 2;			// error return
    }
  }
  argc -= optind;
  argv += optind;

  if(image_filename == 0) {
    fprintf(stderr,
	    "usage: %s [-l] [-g] [-d bias.fits] [-s flatfield.fits] -o outputimage_filename.fits \n",
	    exec_name);
    return 2;			// error return
  }

  ////////////////////////////////////////////////////////////////
  // create an array of images from the remaining arguments
  ////////////////////////////////////////////////////////////////
  if(argc < 3 && exec_mode == EXEC_MEDIAN) {
    fprintf(stderr,
	    "usage: median: at least 3 files must be included in median\n");
    return 2;
  }

  Image **image_array = (Image **) malloc(argc * sizeof(Image *));
  Image *final = 0;
  Image *bias = 0;

  if (dark_filename) {
    bias = new Image(dark_filename);
  }

  if(!image_array) {
    fprintf(stderr, "median: cannot allocate memory\n");
    return 2;
  }
  int image_count;
  for(image_count = 0; image_count < argc; image_count++) {
    // fprintf(stderr, "Reading '%s'\n", argv[image_count]);
    Image *read_image = new Image(argv[image_count]);
    // if we have a bias (dark) image, subtract it from each
    if (linearize) {
      read_image->linearize();
    }
    if (bias) {
      read_image->subtract(bias);
    }
    if (remove_shutter_gradient) {
      ImageInfo *info = read_image->GetImageInfo();
      if (info and info->ExposureDurationValid()) {
	double exposure_time = info->GetExposureDuration();
	read_image->RemoveShutterGradient(exposure_time);
      } else {
	fprintf(stderr, "Error: Cannot remove shutter gradient.\n");
      }
    }
    image_array[image_count] = read_image;
  }
  fprintf(stderr, "%s: %d images read.\n", exec_name, image_count);

  switch(exec_mode) {
  case EXEC_MEDIAN:
    final = median_image(image_array, image_count, 0);
    break;

  case EXEC_MEDIAN_AVERAGE:
    final = median_image(image_array, image_count, 1);
    break;

  case EXEC_ADD:
  case EXEC_AVERAGE:
    // Will sum by adding to the 0'th array element.
    if(argc < 1) {
      fprintf(stderr, "%s: not enough files specified.\n", exec_name);
      exit(2);
    }
    for(image_count = 1; image_count < argc; image_count++) {
      image_array[0]->add(image_array[image_count]);
    }
    if(exec_mode == EXEC_AVERAGE) {
      image_array[0]->scale(1.0 / (double) argc);
    }
    final = image_array[0];
    break;

  case EXEC_SUBTRACT:
    // Will subtract from the 0th element
    if(argc < 2) {
      fprintf(stderr, "%s: not enough files specified.\n", exec_name);
      exit(2);
    }
    for(image_count = 1; image_count < argc; image_count++) {
      image_array[0]->subtract(image_array[image_count]);
    }
    final = image_array[0];
    break;

  case EXEC_UNKNOWN:
    fprintf(stderr, "Cannot handle unknown operation type.\n");
    break;
  }

  if(flatfield_filename) {
    Image *flat = new Image(flatfield_filename);
    if(!flat) {
      fprintf(stderr, "Cannot read flatfield file\n");
    } else {
      final->scale(flat);
    }
  }

  if (bias) {
    final->add(bias);
  }

  CarryForwardKeywords(image_array, argc, final);
  fprintf(stderr, "writing final answer\n");
  if (write_float) {
    final->WriteFITSFloat(image_filename);
  } else {
    final->WriteFITS32(image_filename);
  }
    
  if (not suppress_statistics) {
    Statistics *s = final->statistics();
    fprintf(stderr, "Darkest = %lf\n", s->DarkestPixel);
    fprintf(stderr, "Brightest = %lf\n", s->BrightestPixel);
    fprintf(stderr, "Average = %lf\n", s->AveragePixel);
    fprintf(stderr, "Median = %lf\n", s->MedianPixel);
    fprintf(stderr, "Stddev = %lf\n", s->StdDev);
  }
}

Image *median_image(Image **i_array, int num_images, int med_avg) {
  // First verify that all images have the same size
  int w, h;
  w = i_array[0]->width;
  h = i_array[0]->height;

  int j;
  for(j=0; j<num_images; j++) {
    if(i_array[j]->width != w ||
       i_array[j]->height != h) {
      fprintf(stderr, "median_image: size of image %d mismatch\n",
	      j+1);
      return 0;
    }
  }

  // Okay, all images match sizes
  Image *output = new Image(h, w);

  int x, y;
  double *values = (double *) malloc(num_images * sizeof(double));
  if(!values) {
    fprintf(stderr, "median: malloc failed.\n");
    return 0;
  }
  const int middle_item = num_images/2;
  for(y = 0; y < h; y++) {
    for(x = 0; x < w; x++) {
      for(j=0; j<num_images; j++) {
	values[j] = i_array[j]->pixel(x, y);

	int k = j-1;
	while(k >= 0) {
	  if(values[k] > values[k+1]) {
	    double t = values[k+1];
	    values[k+1] = values[k];
	    values[k] = t;
	  } else {
	    break;
	  }
	  k--;
	}
      }
      if(med_avg) {
	double sum = 0.0;
	for(int k=1; k < (num_images-1); k++) {
	  sum += values[k];
	}
	output->pixel(x, y) = (sum/(num_images-2));
      } else {
	output->pixel(x, y) = values[middle_item];
      }
    }
  }
  free(values);

  CarryForwardKeywords(i_array, num_images, output);

  return output;
}

static std::list<std::string> keywords {
  "FRAMEX",
    "FRAMEY",
    "BINNING",
    "OFFSET",
    "CAMGAIN",
    "READMODE",
    "FILTER",
    "EXPOSURE",
    "DATAMAX" };

void CarryForwardKeywords(Image **i_array,
			  int num_images,
			  Image *final_image) {
  ImageInfo *final_info = final_image->GetImageInfo();
  if (final_info == nullptr) {
    final_info = final_image->CreateImageInfo();
  }
  
  for (auto s : keywords) {
    bool all_images_share_keyword = true;
    std::string value = "XXX";
    
    for (int n=0; n<num_images; n++) {
      ImageInfo *i_info = i_array[n]->GetImageInfo();
      if (i_info->KeywordPresent(s)) {
	if (value == "XXX") {
	  value = i_info->GetValueLiteral(s);
	} else {
	  if (i_info->GetValueLiteral(s) != value) {
	    all_images_share_keyword = false;
	  }
	}
      } else {
	all_images_share_keyword = false;
      }
    }

    if (all_images_share_keyword) {
      final_info->SetValue(s, value);
    }
  }
}
