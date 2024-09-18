#ifndef _ESTIMATE_PARAMS_H
#define _ESTIMATE_PARAMS_H

class Image;

struct FocusParams {
 public:
  double center_x, center_y;
  double background;
  double background_sigma;
  double half_power_point;
  double moment_width;
  double moment_2_width;
  double width_75;
  double total_flux;
  bool success;
};

void estimate_params(Image *i, FocusParams &p);

#endif
