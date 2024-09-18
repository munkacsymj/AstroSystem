#include "model.h"
#include "gaussian_blur.h"
#include "build_ref_image.h"
#include "estimate_params.h"
#include "Image.h"

int main(int argc, char **argv) {
  Model m;

  for (double focus = 0.5; focus < 6.0; focus += 0.2) {

    m.center_x = 20;
    m.center_y = 20;
    m.defocus_width = focus;
    m.obstruction_fraction = 0.40;
    m.gaussian_sigma = 1.1;

    Image *answer = RefImage(40, 40, &m, 10000.0);

    FocusParams fp;

    fp.center_x = m.center_x;
    fp.center_y = m.center_y;
    estimate_params(answer, fp);

    printf("Focus = %lf, gauss = %lf, Moment1 = %lf, Moment2 = %lf, ratio = %lf\n",
	   m.defocus_width, m.gaussian_sigma, fp.moment_width, fp.moment_2_width,
	   fp.moment_2_width/fp.moment_width);
  }
}
