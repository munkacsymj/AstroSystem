#include "colors.h"
#include "trans_coef.h"
#include <stdio.h>

int main(int argc, char **argv) {
  TransformationCoefficients tc("./test_coef.txt");

  Colors c_var;

  c_var.AddColor(i_B, -6.223);
  c_var.AddColor(i_V, -7.855);

  Colors c_comp;
  c_comp.AddColor(i_B, -6.202);
  c_comp.AddColor(i_V, -7.109);

  c_var.AddComp(&c_comp);
  c_var.Transform(&tc);

  double tr_blue;
  double tr_green;
  bool   blue_transformed;
  bool   green_transformed;

  c_var.GetColor(i_B, &tr_blue, &blue_transformed);
  c_var.GetColor(i_V, &tr_green, &green_transformed);

  fprintf(stderr, "transformed instrumental mags: %.3lf (b), %.3lf (v)\n",
	  tr_blue, tr_green);

  const double zero_B = 11.779 - (-6.202);
  const double zero_V = 11.166 - (-7.109);

  fprintf(stderr, "final transformed mags: %.3lf (b), %.3lf (v)\n",
	  tr_blue + zero_B, tr_green + zero_V);
}
