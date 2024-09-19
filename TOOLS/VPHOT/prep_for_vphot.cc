/*  prep_for_vphot.cc -- Prepare a FITS file for import into VPhot
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
#include <unistd.h>		// pick up sleep(), getopt()
#include <stdlib.h>		// pick up atof()
#include <stdio.h>
#include <string.h>
#include <Image.h>


static void usage(void) {
  fprintf(stderr,
	  "usage: prep_for_vphot -i image.fits\n");
  exit(-2);
}
  
int main(int argc, char **argv) {
  int option_char;
  const char *image_filename = 0;
  Image *image = 0;

  while((option_char = getopt(argc, argv, "i:")) > 0) {
    switch (option_char) {
    case 'i':
      image_filename = optarg;
      image = new Image(image_filename);
      break;

    case '?':			// invalid argument
    default:
      fprintf(stderr, "Invalid argument.\n");
      exit(2);
    }
  }

  // make sure that we have  the image to be
  // modified
  if (image == 0) usage();

  // fix keywords:
  // OBJECT: taken from the filename if no OBJECT keyword exists
  // OBJCTRA: object RA
  // OBJCTDEC: object DEC
  // FILTER: single letter taken from the FILTER keyword

  ImageInfo *info = image->GetImageInfo();
  if (info == nullptr) {
    fprintf(stderr, "Aborting: Image has no ImageInfo.\n");
    exit(2);
  }

  if (not info->ObjectValid()) {
    char object_name[512];
    if (strlen(image_filename) >= sizeof(object_name)) {
      fprintf(stderr, "Aborting: filename too long.\n");
      exit(2);
    }
    const char *s = image_filename;
    char *d = object_name;

    // There may be '/' in the filename. Advance s to point after the
    // final '/' character.
    const char *final_slash = nullptr;
    while(*s) {
      if (*s == '/') final_slash = s;
      s++;
    }

    s = image_filename;
    if (final_slash != nullptr) s = final_slash+1;
    
    while(*s and *s != '_') {
      if (*s == '-') {
	*d++ = ' ';
      } else {
	*d++ = *s;
      }
      s++;
    }
    *d = 0;

    info->SetObject(object_name);
  }

  if (not info->NominalDecRAValid()) {
    fprintf(stderr, "Aborting: Dec/RA missing from image.\n");
    exit(2);
  }
  DEC_RA *location = info->GetNominalDecRA();
  
  const char *dec_string = location->string_fulldec_of();
  const char *ra_string = location->string_ra_of();

  info->SetValueString("OBJCTRA", ra_string);
  info->SetValueString("OBJCTDEC", dec_string);

  if (info->FilterValid()) {
    Filter filter = info->GetFilter();
    char filter_string[2];
    filter_string[0] = filter.NameOf()[0];
    filter_string[1] = 0;
    info->SetValueString("FILTER", filter_string);
  }

  image->WriteFITSFloat(image_filename, false);
  return (0);
}

  
  
