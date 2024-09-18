#include <stdio.h>
#include <string.h>
#include <unistd.h> 		// for getopt()
#include <stdlib.h>		// for atof(), atoi()
#include <math.h>		// need M_PI
#include <screen_image.h>
#include <Image.h>		// for class Image
#include "nlls_simple.h"

// Command-line options:
//
// -d dark_frame.fits		// filename of dark frame
// -i image.fits                // filename of image
// -a angle			// angle (degrees) of hartman mask
//

long image_focus(char *filename) {
  ImageInfo info(filename);

  if (info.FocusValid()) {
    return info.GetFocus();
  }

  return -1.0;
}

int main(int argc, char **argv) {
  int ch;			// option character
  Image *dark_image = 0;
  Image *primary_image = 0;
  char image_filename[256];

  while((ch = getopt(argc, argv, "sr:d:i:a:")) != -1) {
    switch(ch) {
    case 'd':			// darkfile name
      dark_image = new Image(optarg); // create image from dark file
      break;

    case 'i':			// image file name
      primary_image = new Image(optarg);
      strcpy(image_filename, optarg); // save the filename
      break;

    case '?':
    default:
	fprintf(stderr,
		"usage: %s -d dark.fits -i image.fits -a hartman_angle\n",
		argv[0]);
	return 2;		// error return
    }
  }

  if(primary_image == 0) {
    fprintf(stderr,
	    "usage: %s -d dark.fits -i image.fits -a hartman_angle\n",
	    argv[0]);
    return 2;			// error return
  }

  if(dark_image) primary_image->subtract(dark_image);

  const long focus_setting = image_focus(image_filename);
    
  focus_state fs;
  fs.state_var[FS_R] = 1.0;
  fs.state_var[FS_Beta] = 5.0;

  if(nlls1(primary_image, &fs) < 0) {
    printf("%s no convergence\n", image_filename);
  } else {
    printf("%s %ld %f %f %f\n",
	   image_filename,
	   focus_setting,
	   fs.state_var[FS_R],
	   fs.state_var[FS_Beta],
	   fs.mel);
  }
}

