#include "residuals.h"
#include <Image.h>
#include "model.h"
#include <math.h>

double
Residuals::RMSError(void) {
  double sum_sq = 0.0;

  for (vector<OneResidual>::iterator it = all_residuals.begin(); it != all_residuals.end(); it++) {
    sum_sq  += ((*it).err * (*it).err);
  }

  return sqrt(sum_sq / all_residuals.size());
}

void
Residuals::AddResidual(int x, int y, double err, double radius) {
  OneResidual r;
  r.x = x;
  r.y = y;
  r.r = radius;
  r.err = err;

  all_residuals.push_back(r);
}

//****************************************************************
//        Residuals::Residuals Constructor
//****************************************************************
Residuals::Residuals(Image *real_image,
		     Image *model_image,
		     Model *model) {
  // validity checks:
  // 1. image sizes must match

  if (real_image->height != model_image->height ||
      real_image->width != model_image->width) {
    fprintf(stderr, "Residuals: image size mismatch (%dx%d vs %dx%d)\n",
	    real_image->height, real_image->width,
	    model_image->height, model_image->width);
    return;
  }

  {
    Image i1(real_image->height, real_image->width);
    // clear the image
    i1.subtract(&i1);
    // copy in the real image
    i1.add(real_image);
    // subtract the model
    i1.subtract(model_image);
    i1.WriteFITSFloat("/tmp/residual.fits");
  }

  for (int row = 0; row < model_image->height; row++) {
    for (int col = 0; col < model_image->width; col++) {
      const double del_x = model->center_x - (col + 0.5);
      const double del_y = model->center_y - (row + 0.5);
      const double r = sqrt(del_x*del_x + del_y*del_y);

      if (r < model->max_radius) {
	AddResidual(col, row,
		    real_image->pixel(col, row) - model_image->pixel(col, row),
		    r);
      }
    }
  }
}


