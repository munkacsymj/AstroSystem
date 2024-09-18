#include <Image.h>
#include <stdio.h>
#include <unistd.h>		// getopt()

void usage(void) {
  fprintf(stderr, "usage: graph_composite_profile -i image.fits\n");
  exit(-2);
}

int main(int argc, char **argv) {
  int option_char;
  char *image_filename = 0;

  while((option_char = getopt(argc, argv, "i:")) > 0) {
    switch (option_char) {
    case 'i':
      image_filename = optarg;
      break;

    case '?':
    default:
      fprintf(stderr, "Invalid argument.\n");
      usage();
    }
  }

  if (!image_filename) {
    fprintf(stderr, "No image specified.\n");
    usage();
  }

  Image image(image_filename);

  const double center_x = image.width/2.0;
  const double center_y = image.height/2.0;

  FILE *fp_plot = popen("/home/mark/ASTRO/CURRENT/TOOLS/FOCUS_MODEL/graph_composite_profile.py", "w");
  if (!fp_plot) {
    fprintf(stderr, "graph_composite_profile: unable to open plotter's pipe.\n");
    exit(-2);
  }

  fprintf(fp_plot, "title %s\n", image_filename);

  for (int row = 0; row < image.height; row++) {
    for (int col = 0; col < image.width; col++) {
      double value = image.pixel(col, row);
      const double del_x = center_x - (col + 0.5);
      const double del_y = center_y - (row + 0.5);
      const double r = sqrt(del_x*del_x + del_y*del_y);

      fprintf(fp_plot, "point %lf %lf\n", r, value);
    }
  }
  fprintf(fp_plot, "show\n");
  return 0;
}


