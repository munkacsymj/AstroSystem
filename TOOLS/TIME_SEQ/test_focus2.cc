#include <Image.h>
#include "gaussian_fit.h"
#include <IStarList.h>
#include <alt_az.h>
#include <math.h>
#include <stdio.h>

extern double GaussianR0;

double gaussian(Image *image, int *status);

int main(int argc, char **argv) {
  const char *imagename = "/home/IMAGES/11-15-2021/image124.fits";
  const char *darkname = "/home/IMAGES/11-15-2021/dark60.fits";
  const char *flatname = "/home/IMAGES/11-15-2021/flat_Rc.fits";

  Image image(imagename);
  Image dark(darkname);
  Image flat(flatname);

  GaussianR0 = 1.0;

  ImageInfo *info = image.GetImageInfo();
  IStarList *star_list = image.GetIStarList();
  CompositeImage *composite = BuildComposite(&image, star_list);
  int status = 0;
  double gaussian_value = gaussian(composite, &status);

  ALT_AZ loc = info->GetAzEl();
  const double zenith_angle = M_PI/2.0 - loc.altitude_of();
  const double blur_factor = pow(cos(zenith_angle), 0.6);
  gaussian_value *= blur_factor;

  fprintf(stderr, "Final gaussian_value = %lf\n", gaussian_value);
  composite->WriteFITSFloatUncompressed("/tmp/composite.fits");

  return 0;
}

double gaussian(Image *image, int *status) {
  double dark_reference_pixel = image->statistics()->DarkestPixel;

  const double center_x = image->width/2.0;
  const double center_y = image->height/2.0;

  Gaussian g;
  g.reset();			// initialize the gaussian
  GRunData run_data;
  run_data.reset();

  fprintf(stderr, "dark_reference_pixel = %.2lf\n", dark_reference_pixel);
  FILE *fp_g = fopen("/tmp/gaussian.csv", "w");

  for (int row = 0; row < image->height; row++) {
    for (int col = 0; col < image->width; col++) {
      double value = image->pixel(col, row);
      const double del_x = center_x - (col + 0.5);
      const double del_y = center_y - (row + 0.5);
      const double r = sqrt(del_x*del_x + del_y*del_y);

      double adj_value = value - dark_reference_pixel;

      fprintf(fp_g, "%d,%d,%.3lf,%.1lf\n",
	      col, row, r, adj_value);

      run_data.add(del_x, del_y, adj_value);
    }
  }
  fclose(fp_g);

  double return_value = 0.0;
  
  if (nlls_gaussian(&g, &run_data)) {
    fprintf(stderr, "gaussian: no convergence.\n");
    *status = 1;
    return_value = 0.0;
  } else {
    *status = 0;
    return_value = g.state_var[1]/10.0;
    fprintf(stderr, "gaussian: %.3lf\n", return_value);
  }

  return return_value;
}

