/*  fits_to_gif.cc -- Program to convert a FITS image to a .gif image
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
#include <sys/types.h>		// for getpid()
#include <stdio.h>
#include <unistd.h> 		// for getopt(), getpid()
#include <stdlib.h>		// for atof()
#include <Image.h>
#include <fitsio.h>

void usage(void) {
      fprintf(stderr,
	      "usage: fits_to_gif [-s flat.fits] [-d dark.fits] [-u nn -l nn] -i image.fits -o image.gif\n");
      exit(-2);
}

int main(int argc, char **argv) {
  int ch;			// option character
  Image *image = 0;
  //Image *dark = 0;
  Image *flat = 0;
  double   max_pixel_value = -1.0;
  double   min_pixel_value = -1.0;

  char *image_filename = 0;	// filename of the .fits image file
  char *output_filename = 0;

  // Command line options:
  // -i imagefile.fits
  // -o outputfile.gif     Write fits_to_gif into different file
  // -u upper-scaling-limit (white pixel)
  // -l lower-scaling-limit (black pixel)

  while((ch = getopt(argc, argv, "i:o:u:l:d:s:")) != -1) {
    switch(ch) {
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
	fprintf(stderr, "fits_to_gif: only one image file permitted.\n");
	exit(2);
      }
      fprintf(stderr, "fits_to_gif: image file = '%s'\n", optarg);
      image_filename = optarg;
      image = new Image(image_filename);
      break;

      //case 'd':			// dark file name
      //      dark = new Image(optarg);

      //      fprintf(stderr, "fits_to_gif: dark file = '%s'\n", optarg);
      //      break;

    case 'o':
      output_filename = optarg;
      break;

    case '?':
    default:
      usage();
   }
  }

  // all four command-line arguments are required
  if(image_filename == 0 || output_filename == 0) {
    usage();
  }
    
  if(max_pixel_value == -1 || min_pixel_value == -1) {
    double median = image->statistics()->MedianPixel;
    min_pixel_value = median - 55.0;
    if(min_pixel_value < 0.0) min_pixel_value = 0.0;
    max_pixel_value = min_pixel_value + 4400.0;
  }

  char uniqname[12];
  sprintf(uniqname, "%05u", (unsigned int) getpid());
  char phot_filename[64];
  sprintf(phot_filename, "/tmp/fits_to_gif%s", uniqname);

  // now create a script
  char script_name[64];
  sprintf(script_name, "/tmp/script%s.cl", uniqname);

  FILE *fp = fopen(script_name, "w");
  if(!fp) {
    fprintf(stderr, "fits_to_gif: cannot create script file\n");
    exit(-2);
  }

  fprintf(fp, "dataio\n");
  fprintf(fp, "imdelete /tmp/imagez%s verify-\n", uniqname);
  fprintf(fp, "rfits %s \"\" /tmp/imagez%s short_header-\n",
	  image_filename, uniqname);
  fprintf(fp, "delete %s verify-\n", output_filename);
  fprintf(fp, "export /tmp/imagez%s %s gif outbands=\"flipy(zscale(i1,%f,%f))\"\n",
	  uniqname, output_filename, min_pixel_value, max_pixel_value);
  fprintf(fp, "imdelete /tmp/imagez%s verify-\n", uniqname);
  fprintf(fp, "logout\n");

  fclose(fp);
  
  char command_buffer[132];
  sprintf(command_buffer, "cl < %s > /tmp/script.out%s 2>&1\n",
	  script_name, uniqname);
  if(system(command_buffer)) {
    fprintf(stderr, "iraf script returned error code.\n");
  }

  char script_out[180];
  sprintf(script_out, "/tmp/script.out%s", uniqname);
  // unlink(script_out);

  // unlink(script_name);
}
