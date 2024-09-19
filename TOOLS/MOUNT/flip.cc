#include <stdio.h>
#include "scope_api.h"

void PrintScopeData(FILE *fp) {
  fprintf(fp, "scope on west side of pier = %d\n",
	  scope_on_west_side_of_pier());
  fprintf(fp, "dec axis is flipped = %d\n",
	  dec_axis_is_flipped());
  fprintf(fp, "hour angle = %.1lf (deg)\n",
	  (180/M_PI)*GetScopeHA());
  fprintf(fp, "min remaining to limit = %ld (min)\n",
	  MinsRemainingToLimit());
  DumpCurrentLimits();
}

int main(int argc, char **argv) {
  connect_to_scope();
  fprintf(stderr, "Preparing to issue Flip command.\n");
  PrintScopeData(stderr);
  bool flipped = PerformMeridianFlip();
  if (flipped) {
    fprintf(stderr, "Flip completed success.\n");
  } else {
    fprintf(stderr, "Mount returned error.\n");
  }
  PrintScopeData(stderr);
  return 0;
}

