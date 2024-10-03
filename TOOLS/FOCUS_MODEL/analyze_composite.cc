#include <Image.h>
#include <stdio.h>
#include <unistd.h>		// getopt()
#include <list>
#include <assert.h>
#include "gaussian_fit.h"

const double bin_width = 1.2; // pixels
struct RangeBin {
  double radius;		// center of the range bin
  std::list<double> values;
  double smoothed_value;
  double histogram_value;
};

#define NUM_INTENSITY_BINS 25
struct IntensityBin {
  double intensity;		// center of the intensity bin
  std::list<double> r_values;
  double avg_r;
  double r_std_dev;
};

void usage(void) {
  fprintf(stderr, "usage: analyze_composite -i image.fits\n");
  exit(-2);
}

double list_average(std::list<double> *v) {
  double sum = 0.0;
  std::list<double>::iterator it;
  for (it = v->begin(); it != v->end(); it++) {
    sum += (*it);
  }
  return sum/v->size();
}

double stddev(std::list<double> *v) {
  const double average = list_average(v);
  double sum_sq_diff = 0.0;
  std::list<double>::iterator it;
  for (it = v->begin(); it != v->end(); it++) {
    const double f = (*it) - average;
    sum_sq_diff += (f*f);
  }
  return sqrt(sum_sq_diff/(v->size() - 1));
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
  double dark_reference_pixel = image.HistogramValue(0.05);

  const double center_x = image.width/2.0;
  const double center_y = image.height/2.0;

  const int max_r = 1 + (int) sqrt((image.width * image.width)/4.0 +
				   (image.height * image.height)/4.0);
  RangeBin *all_bins = new RangeBin[max_r + 1];
  for (int i = 0; i <= max_r; i++) {
    all_bins[i].radius = bin_width/2.0 + i*bin_width;
  }

  Gaussian g;
  g.reset();			// initialize the gaussian
  GRunData run_data;
  run_data.reset();

  for (int row = 0; row < image.height; row++) {
    for (int col = 0; col < image.width; col++) {
      double value = image.pixel(col, row);
      const double del_x = center_x - (col + 0.5);
      const double del_y = center_y - (row + 0.5);
      const double r = sqrt(del_x*del_x + del_y*del_y);

      double adj_value = value - dark_reference_pixel;

      run_data.add(r, adj_value);

      //constexpr int skipper = 6;
      //if (row % skipper == 0 and col % skipper == 0) {
      //fprintf(stderr, "%.2lf,%.1lf\n", r, adj_value);
      //}
    }
  }

  if (nlls_gaussian(&g, &run_data)) {
    fprintf(stderr, "gaussian: no convergence.\n");
  } else {
    fprintf(stdout, "gaussian: %.3lf\n", g.state_var[1]/10.0);
  }
  return 0;
}
  
