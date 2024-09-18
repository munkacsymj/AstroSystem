#include <stdio.h>
#include <string.h>
#include <unistd.h> 		// for getopt()
#include <stdlib.h>		// for atof(), atoi()
#include <Image.h>		// for class Image

// Command-line options:
//
// -d dark_frame.fits		// filename of dark frame
// -i image.fits                // filename of image
// -o output_filename
//

int main(int argc, char **argv) {
  int ch;			// option character
  Image *dark_image = 0;
  Image *primary_image = 0;
  char image_filename[256];
  FILE *output_file = 0;

  while((ch = getopt(argc, argv, "d:i:o:")) != -1) {
    switch(ch) {
    case 'd':			// darkfile name
      dark_image = new Image(optarg); // create image from dark file
      break;

    case 'i':			// image file name
      primary_image = new Image(optarg);
      strcpy(image_filename, optarg); // save the filename
      break;

    case 'o':
      output_file = fopen(optarg, "w");
      if(!output_file) {
	perror("Cannot open output file");
      }
      break;

    case '?':
    default:
	fprintf(stderr,
		"usage: %s -d dark.fits -i image.fits\n", argv[0]);
	return 2;		// error return
    }
  }

  if(primary_image == 0 || output_file == 0) {
    fprintf(stderr,
	    "usage: %s -d dark.fits -i image.fits -o output_file\n",
	    argv[0]);
    return 2;			// error return
  }

  if(dark_image) primary_image->subtract(dark_image);

  primary_image->PrintBiggestStar(output_file);
}
