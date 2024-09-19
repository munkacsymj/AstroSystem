/*  convert_fits.cc -- Dark-subtract and flat-field correct an image
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
#include <libgen.h>		// for basename()
#include <unistd.h> 		// for getopt()
#include <stdlib.h>		// for atof()
#include <Image.h>

void usage(void) {
  fprintf(stderr, "usage: convert_fits -f format_string -i raw.fits -o newfile.fits\n");
  fprintf(stderr, "       format_string: 16,32,64 - word size\n");
  fprintf(stderr, "                      i - integer\n");
  fprintf(stderr, "                      f - float\n");
  fprintf(stderr, "                      u,-  - unsigned (u) or signed(-)\n");
  fprintf(stderr, "                      z - compressed\n");
  exit(-2);
}

struct Format {
  bool compressed{0};
  bool un_signed{1};
  bool integer{1};
  int  wordsize{16};
};

void PrintFormat(Format &f, FILE *fp) {
  fprintf(fp, "Converting to %d-bit ", f.wordsize);
  fprintf(fp, "%s ", (f.integer ? "integer" : "float"));
  fprintf(fp, "(%s) ", (f.un_signed ? "unsigned" : "signed"));
  fprintf(fp, "%s\n", (f.compressed ? "(compressed)" : "(uncompressed)"));
}

void UnsupportedFormat(Format &f) {
  fprintf(stderr, "This is an unsupported format. No file written.\n");
  exit(-2);
}

Format ParseFormat(const char *s) {
  Format f;
  while(*s) {
    if (*s == 'z') f.compressed = true;
    else if (*s == 'u') f.un_signed = true;
    else if (*s == '-') f.un_signed = false;
    else if (*s == 'i') f.integer = true;
    else if (*s == 'f') f.integer = false;
    else if (*s == '1' && *(s+1) == '6') {
      s++;
      f.wordsize = 16;
    } else if (*s == '3' && *(s+1) == '2') {
      s++;
      f.wordsize = 32;
    } else if (*s == '6' && *(s+1) == '4') {
      s++;
      f.wordsize = 64;
    } else {
      fprintf(stderr, "convert_fits: unrecognized format character: %c\n", *s);
      return f;
    }
    s++;
  }
  return f;
}

int main(int argc, char **argv) {
  int ch;			// option character
  char *image_filename = nullptr;	// filename of the output .fits image file
  char *output_filename = nullptr;
  Format f;

  while((ch = getopt(argc, argv, "o:i:f:")) != -1) {
    switch(ch) {
    case 'f':
      f = ParseFormat(optarg);
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

  if(image_filename == nullptr or output_filename == nullptr) {
    usage();
    /*NOTREACHED*/
  }

  PrintFormat(f, stderr);
  Image *raw = new Image(image_filename);
  if (f.integer and f.un_signed) {
    if (f.wordsize == 16) {
      raw->WriteFITS16(output_filename, f.compressed);
    } else if (f.wordsize == 32) {
      raw->WriteFITS32(output_filename, f.compressed);
    } else {
      UnsupportedFormat(f);
    }
  } else if (not f.integer) {
    raw->WriteFITSFloat(output_filename, f.compressed);
  } else {
    UnsupportedFormat(f);
  }
  return 0;
}
