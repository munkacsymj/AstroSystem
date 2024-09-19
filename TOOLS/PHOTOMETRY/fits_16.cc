/*  fits_16.cc: create a "normal" fits file (single HDU)
 *
 *  Copyright (C) 2021 Mark J. Munkacsy
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
#include <unistd.h> 		// for getopt()
#include <stdlib.h>		// for atof(), atoi()
#include <Image.h>		// get Image

// Command-line options:
//
// -i dark60.fits -o dark60_16.fits     // filename of output file
//
//

void usage(void) {
  fprintf(stderr, "fits_16 -i dark60.fits -o dark60_16.fits\n");
  exit(-2);
}

int main(int argc, char **argv) {
  int ch;			// option character
  char *input_filename = nullptr;
  char *output_filename = nullptr;

  while((ch = getopt(argc, argv, "i:o:")) != -1) {
    switch(ch) {
    case 'o':
      output_filename = optarg;
      break;

    case 'i':
      input_filename = optarg;
      break;

    case '?':
    default:
      usage();
    }
  }

  if(output_filename == nullptr || input_filename == nullptr) usage();

  Image i(input_filename);
  Image out(i.height, i.width);

  for (int x = 0; x<i.width; x++) {
    for (int y = 0; y<i.height; y++) {
      out.pixel(x,y) = i.pixel(x,y);
    }
  }

  ImageInfo *info = i.GetImageInfo();
  if (info) {
    ImageInfo *tgt_info = out.CreateImageInfo();
    tgt_info->PullFrom(info);
  }
  out.WriteFITS16(output_filename, false);
  return 0;
}
