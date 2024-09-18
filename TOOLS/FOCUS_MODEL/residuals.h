#ifndef _RESIDUALS_H
#define _RESIDUALS_H

#include <Image.h>
#include <vector>

using std::vector;

class Model;			/* forward declaration */

class Residuals {
 public:
  Residuals(Image *real_image,
	    Image *model_image,
	    Model *model);
  ~Residuals(void) { all_residuals.clear(); }

  double RMSError(void);	/* returns the rms total of all residuals */
  void AddResidual(int x, int y, double err, double radius);
  int NumPoints(void) const { return all_residuals.size(); }
  int ResidualX(int point) const { return all_residuals[point].x; }
  int ResidualY(int point) const { return all_residuals[point].y; }
  double ResidualR(int point) const { return all_residuals[point].r; }
  double ResidualErr(int point) const { return all_residuals[point].err; }

 private:
  struct OneResidual {
    int x;
    int y;
    double r;
    double err;
  };

  vector<OneResidual> all_residuals;
};

#endif
