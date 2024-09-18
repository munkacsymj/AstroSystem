#include "dec_ra.h"
#include <math.h>
#include <stdio.h>


int main(int argc, char **argv) {
  DEC_RA location(-0.001942881123049432, (8.0/12.0)*M_PI);

  const double x = 3.405;
  fprintf(stdout, "%09.6lf\n", x);

  fprintf(stdout, "%s, %s\n", location.string_dec_of(),
	  location.string_ra_of());
  return 0;
}
