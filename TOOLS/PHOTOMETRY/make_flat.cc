/*  make_flat.cc -- performs the actual arithmatic to turn a dome flat
 *  image into a flat frame.
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
#include <stdio.h>
#include <unistd.h> 		// for getopt()
#include <stdlib.h>		// for atof(), atoi()
#include <list>
#include <Image.h>		// get Image

// Command-line options:
//
// -d dark_frame.fits		// filename of dark frame
// -i flat_frame.fits		// filename of flat frame
// -o calibration_flat.fits     // filename of output file
// -b bias_frame.fits           // filename of bias frame
//
//

void CarryForwardKeywords(ImageInfo *source,
			  Image &final_image);

void usage(void) {
  fprintf(stderr, "make_flat -b bias_frame.fits -i flat_frame.fits [-d dark_frame.fits] [-l] [-g] -o output.fits\n");
  //fprintf(stderr, "     -l     perform linearity corrections\n");
  //fprintf(stderr, "     -g     perform shutter speed gradient corrections\n");
  exit(-2);
}

int main(int argc, char **argv) {
  int ch;			// option character
  Image *dark_image = 0;
  //Image *bias_frame = 0;
  char *output_filename = 0;
  //bool correct_linearity = false;
  //bool correct_shutter_gradient = false;

  while((ch = getopt(argc, argv, "d:o:")) != -1) {
    switch(ch) {
    case 'd':			// darkfile name
      dark_image = new Image(optarg); // create image from dark file
      break;

#if 0
    case 'l':
      correct_linearity = true;
      break;

    case 'g':
      correct_shutter_gradient = true;
      break;

    case 'b':
      bias_frame = new Image(optarg);
      break;

    case 'i':
      flat_frame = new Image(optarg);
      break;

#endif
    case 'o':
      output_filename = optarg;
      break;

    case '?':
    default:
      usage();
    }
  }

  if(output_filename == 0) usage();
  argc -= optind;
  argv += optind;

  Image *Output = nullptr;
  // remember this for later, when we need to copy over FITS keywords...
  const char *first_flat_filename = *argv;

  while(argc--) {
    Image i(*argv++);
    if (dark_image) i.subtract(dark_image);
    if (Output == nullptr) {
      Output = new Image(i.height, i.width);
    } else if(i.height != Output->height or
	      i.width != Output->width) {
      fprintf(stderr, "Inconsistent image sizes.\n");
      exit(-2);
    }
    Output->add(&i);
  }

  // divide every cell by the median of the entire image
  double overall_median = Output->statistics()->MedianPixel;
  fprintf(stderr, "median of original flat frame is %f\n",
	  Output->statistics()->MedianPixel);
  Output->scale(1.0 / overall_median);

  int row, col;
  // in the first two rows and columns of the CB245 camera, ignore
  // these pixels, since they seem artificially dark and trying to
  // flat-field them just skews overall statistics without adding any
  // usable pixels (since they're screwed up in any data images we
  // apply the flatfield to).
  for(row = 0; row < Output->height; row++)
    for(col = 0; col < Output->width; col++) {
      // if(row < 2 || col < 2) Output->pixel(col, row) = 1.0;
      if(Output->pixel(col, row) < 0.1)
	Output->pixel(col, row) = 0.1;
      if(Output->pixel(col, row) > 10.0)
	Output->pixel(col, row) = 10.0;
    }
      
  fprintf(stderr, "median of final flat = %f\n",
	  Output->statistics()->MedianPixel);
  Image first(first_flat_filename);
  ImageInfo *first_info = first.GetImageInfo();
  CarryForwardKeywords(first_info,
		       *Output);
  
  Output->WriteFITSFloat(output_filename);
  return 0;
}

static std::list<std::string> keywords {
    "CAMERA",
    "FOCALLEN",
    "TELESCOP",
    "SITELAT",
    "SITELON",
    "PURPOSE",
    "NORTH-UP",
    "ROTATION",
    "OFFSET",
    "CAMGAIN",
    "RA_NOM",
    "DEC_NOM",
    "READMODE",
    "FILTER",
    "FRAMEX",
    "FRAMEY",
    "BINNING",
    "EXPOSURE",
    "DATAMAX" };

void CarryForwardKeywords(ImageInfo *source,
			  Image &final_image) {
  ImageInfo *final_info = final_image.GetImageInfo();
  if (final_info == nullptr) {
    final_info = final_image.CreateImageInfo();
  }
  
  for (auto s : keywords) {
    if (source->KeywordPresent(s)) {
      final_info->SetValue(s, source->GetValueLiteral(s));
    }
  }
}
