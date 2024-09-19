#include <Image.h>

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

void usage(void) {
  fprintf(stderr, "usage: gradient -1 file1.fits -2 file2.fits -o output.csv\n");
  exit(-2);
}

const char *image_directory = "/home/IMAGES/11-19-2021/";

struct Trial {
  int image_num;
  double exp_time;
  double slope;
};

#define BLUE

#ifdef BLUE
constexpr static int ref_image = 92;
Trial all_trials[] = {{5, 0.1, 0.0},
		      {11, 0.2, 0.0},
		      {17, 0.3, 0.0},
		      {23, 0.4, 0.0},
		      {29, 0.5, 0.0},
		      {35, 0.6, 0.0},
		      {41, 0.7, 0.0},
		      //{47, 0.8, 0.0},
		      //{53, 0.9, 0.0},
		      //{59, 1.0, 0.0},
		      //{65, 1.3, 0.0},
		      //{71, 1.5, 0.0},
		      //{77, 2.0, 0.0},
		      //{82, 3.0, 0.0},
		      //{87, 4.0, 0.0},
		      //{92, 5.0, 0.0},
};
#endif // BLUE
#ifdef GREEN
constexpr static int ref_image = 72;
Trial all_trials[] = {{6, 0.1, 0.0},
		      {12, 0.2, 0.0},
		      {18, 0.3, 0.0},
		      {24, 0.4, 0.0},
		      {30, 0.5, 0.0},
		      {36, 0.6, 0.0},
		      {42, 0.7, 0.0},
		      {48, 0.8, 0.0},
		      {54, 0.9, 0.0},
		      {60, 1.0, 0.0},
		      {66, 1.3, 0.0},
		      {72, 1.5, 0.0},
};
#endif // GREEN
#ifdef RED
constexpr static int ref_image = 73;
Trial all_trials[] = {{7, 0.1, 0.0},
		      {13, 0.2, 0.0},
		      {19, 0.3, 0.0},
		      {25, 0.4, 0.0},
		      {31, 0.5, 0.0},
		      {37, 0.6, 0.0},
		      {43, 0.7, 0.0},
		      {49, 0.8, 0.0},
		      {55, 0.9, 0.0},
		      {61, 1.0, 0.0},
		      {67, 1.3, 0.0},
		      {73, 1.5, 0.0},
};
#endif // RED
#ifdef IR
constexpr static int ref_image = 144;
Trial all_trials[] = {{78, 2.0, 0.0},
		      {83, 3.0, 0.0},
		      {88, 4.0, 0.0},
		      {93, 5.0, 0.0},
		      {108, 6.0, 0.0},
		      {112, 7.0, 0.0},
		      {116, 8.0, 0.0},
		      {120, 9.0, 0.0},
		      {124, 10.0, 0.0},
		      {128, 15.0, 0.0},
		      {132, 20.0, 0.0},
		      {136, 25.0, 0.0},
		      {140, 30.0, 0.0},
		      {144, 60.0, 0.0},
};
#endif // RED


void ProcessTrial(const char *image1_name, const char *image2_name, Trial &trial);

int main(int argc, char **argv) {
  constexpr int NUM_TRIALS = sizeof(all_trials)/sizeof(all_trials[0]);
  char ref_filename[80];
  sprintf(ref_filename, "%simage%03dc.fits", image_directory, ref_image);
  for (int i=0; i<NUM_TRIALS; i++) {
    char other_filename[80];
    sprintf(other_filename, "%simage%03dc.fits", image_directory, all_trials[i].image_num);
    ProcessTrial(ref_filename, other_filename, all_trials[i]);
  }

  FILE *fp = fopen("/tmp/gradient_summary.csv", "w");
  for (int i=0; i<NUM_TRIALS; i++) {
    fprintf(fp, "%.1lf,%le\n", all_trials[i].exp_time, all_trials[i].slope);
  }
  fclose(fp);

  return 0;
}

void ProcessTrial(const char *image1_name, const char *image2_name, Trial &trial) {
  Image image1(image1_name);
  Image image2(image2_name);

  const double median1 = image1.statistics()->MedianPixel;
  const double median2 = image2.statistics()->MedianPixel;
  image1.scale(1.0/median1);
  image2.scale(1.0/median2);

  image1.scale(&image2);
  const char *output_table = "/tmp/gradient.csv";
  FILE *output = fopen(output_table, "w");
  if (!output) {
    perror("Unable to open output table for writing: ");
    exit(-2);
  }

  double sum_x = 0.0;
  double sum_y = 0.0;
  double sum_xx = 0.0;
  double sum_yy = 0.0;
  double sum_xy = 0.0;
  
  for (int y=0; y < image1.height; y++) {
    double pix_sum = 0.0;
    for (int x=0; x < image1.width; x++) {
      pix_sum += image1.pixel(x,y);
    }
    const double avg = pix_sum/image1.width;
    // I know this is confusing, but the following lines use a
    // reversed sense of (x,y). Sorry. I did it so I could use the
    // algorithm I found in wikipedia for Simple Linear Regression
    sum_x += y;
    sum_xx += (y*y);
    sum_y += avg;
    sum_yy += (avg*avg);
    sum_xy += (avg*y);
    
    fprintf(output, "%d,%.4lf\n", y, avg);
  }
  fclose(output);

  const double slope = (image1.height*sum_xy - sum_x*sum_y)/(image1.height*sum_xx - sum_x*sum_x);
  fprintf(stderr, "Slope = %le\n", slope);
  trial.slope = slope;
}  
