#include <Image.h>
#include <math.h>
#include <unistd.h>
#include <stdio.h>

void usage(void) {
  fprintf(stderr, "usage: image_to_csv -i filename.fits\n");
  exit(2);
}

int main(int argc, char **argv) {
  int option_char;
  const char *image_filename = 0;

  while((option_char = getopt(argc, argv, "i:")) > 0) {
    switch (option_char) {
    case 'i':
      image_filename = optarg;
      break;

    case '?':
    default:
      fprintf(stderr, "Invalid argument.\n");
      usage();
      exit(2);
    }
  }

  Image image(image_filename);
  double background = image.HistogramValue(0.5);
  double max_x = -1.0;
  double max_y = -1.0;
  double brightest = 0.0;

  for (int row = 0; row < image.height; row++) {
    for (int col = 0; col < image.width; col++) {
      if (image.pixel(col, row) > brightest) {
	brightest = image.pixel(col, row);
	max_x = col;
	max_y = row;
      }
    }
  }

  const double limit = 28; // radius in pixels
  for (int loop = 0; loop < 10; loop++) {
    double offset_x = 0.0;
    double offset_y = 0.0;
    double pix_sum = 0.0;
    bool final = (loop == 9); // last time through

    for (int row = 0; row < image.height; row++) {
      for (int col = 0; col < image.width; col++) {
	const double del_x = (col + 0.5) - max_x;
	const double del_y = (row + 0.5) - max_y;
	const double del_r = sqrt(del_x*del_x + del_y*del_y);

	if (del_r < limit) {
	  const double pix = image.pixel(col, row) - background;
	  offset_x += pix*del_x;
	  offset_y += pix*del_y;
	  pix_sum += pix;

	  if (final) {
	    fprintf(stderr, "%lf,%lf\n", del_r, pix);
	  }
	}
      }
    }
    fprintf(stderr, "trial x,y @ (%lf,%lf): offset_x = %lf, offset_y = %lf\n",
	    max_x, max_y, offset_x, offset_y);
    max_x = max_x + offset_x/pix_sum;
    max_y = max_y + offset_y/pix_sum;
  }

  return 0; // exit from main()
}
      
