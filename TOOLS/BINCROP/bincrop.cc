/*  bincrop.cc -- Create new image by binning/cropping
 *
 *  Copyright (C) 2022 Mark J. Munkacsy
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
#include <list>

#include <Image.h>
#include <camera_api.h>

void CarryForwardKeywords(ImageInfo *source,
			  Image &final_image);

void usage(void) {
  fprintf(stderr, "usage: bincrop [-n] -i input.fits -o output.fits -P profile\n");
  fprintf(stderr, "    -n    normalize to average value of 1.0\n");
  exit(-2);
}

int main(int argc, char **argv) {
  int ch;			// option character
  char *image_filename = nullptr;	// filename of the input .fits image file
  char *output_filename = nullptr;
  char *profile_name = nullptr;
  bool normalize = false;

  // Command line options:
  //
  // -P profile -i input.fits -o filename.fits   
  //

  while((ch = getopt(argc, argv, "nP:i:o:")) != -1) {
    switch(ch) {
    case 'n':
      normalize = true;
      break;

    case 'P':
      profile_name = optarg;
      break;

    case 'o':			// image filename
      output_filename = optarg;
      break;

    case 'i':			// image filename
      image_filename = optarg;
      break;

    case '?':
    default:
      usage();
      /*NOTREACHED*/
    }
  }

  if(image_filename == 0 or
     output_filename == 0 or
     profile_name == 0) {
    usage();
    /*NOTREACHED*/
  }

  Image input(image_filename);
  ImageInfo *i_info = input.GetImageInfo();

  exposure_flags flags(profile_name);
  const int target_binning = flags.GetBinning();
  // Next four variables describe height & edges using UNBINNED pixels!
  const int target_left_edge = flags.subframe.box_left;
  const int target_top_edge = flags.subframe.box_top;
  const int target_bottom_edge = flags.subframe.box_bottom;
  const int target_width = flags.subframe.box_width(); 
  const int target_height = flags.subframe.box_height();

  const int input_binning = i_info->GetBinning();
  const int input_left_edge = (i_info->FrameXYValid() ? i_info->GetFrameX() : 0);
  const int input_bottom_edge = (i_info->FrameXYValid() ? i_info->GetFrameY() : 0);
  const int input_top_edge = input.height - 1 - input_bottom_edge;
  const int input_width = input.width;
  const int input_height = input.height;

#if 1
  fprintf(stderr, "input_binning = %d, width = %d, height = %d\n",
	  input_binning, input_width, input_height);
  fprintf(stderr, "input_left_edge = %d, input_top_edge = %d\n",
	  input_left_edge, input_top_edge);
  fprintf(stderr, "target_binning = %d, width = %d, height = %d\n",
	  target_binning, target_width, target_height);
  fprintf(stderr, "target_left_edge = %d, target_top_edge = %d\n",
	  target_left_edge, target_top_edge);
#endif
			 

  Image output(target_height/target_binning,
	       target_width/target_binning);
  CarryForwardKeywords(i_info, output);

  // Legality checks
  if (target_binning < input_binning or
      input_left_edge > target_left_edge or
      input_top_edge < target_top_edge or
      input_left_edge + input_width*input_binning <
      target_left_edge + target_width or
      input_top_edge + input_height*input_binning <
      target_top_edge + target_height) {
    fprintf(stderr, "Cannot convert from input format to output format.\n");
    fprintf(stderr, "Input: height = %d, width = %d, left = %d, top = %d, bin = %d\n",
	    input_height, input_width,
	    input_left_edge, input_top_edge,
	    input_binning);
    fprintf(stderr, "Output: height = %d, width = %d, left = %d, top = %d, bin = %d\n",
	    target_height, target_width,
	    target_left_edge, target_top_edge,
	    target_binning);
    exit(-2);
  }

  const int bin_ratio = target_binning/input_binning;


  for (int x = 0; x < output.width; x++) {
    int src_x = x*bin_ratio + (target_left_edge - input_left_edge)/input_binning;
    for (int y=0; y < output.height; y++) {
      int src_y = y*bin_ratio + (target_bottom_edge - input_bottom_edge)/input_binning;
      double sum = 0.0;
      for (int xx=0; xx < bin_ratio; xx++) {
	for (int yy=0; yy < bin_ratio; yy++) {
	  sum += input.pixel(src_x+xx, src_y+yy);
	}
      }
      output.pixel(x, y) = sum;
    }
  }

  if (normalize) {
    Statistics *stats = output.statistics();
    const double avg = stats->AveragePixel;
    for (int x=0; x<output.width; x++) {
      for (int y=0; y<output.height; y++) {
	output.pixel(x,y) /= avg;
      }
    }
  }

  ImageInfo *o_info = output.GetImageInfo();
  o_info->SetBinning(target_binning);
  o_info->SetFrameXY(target_left_edge, target_height - 1 - target_top_edge);

  output.WriteFITSFloatUncompressed(output_filename);
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
