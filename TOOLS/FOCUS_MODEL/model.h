#ifndef _MODEL_H
#define _MODEL_H

struct Model {
  double center_x, center_y;
  double defocus_width;
  double obstruction_fraction;
  double gaussian_sigma;
  double max_radius;
};

#endif
