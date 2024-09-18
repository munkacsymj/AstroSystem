#include <julian.h>

const static int MAGNITUDE_AVAILABLE = 1;
const static int NO_MAGNITUDE = 2;

class MAGNITUDE {
  int status;
  double value;
};

enum FILTER { NONE, JOHNSON_V, JOHNSON_R, JOHNSON_I, JOHNSON_B, JOHNSON_U };

class ArchiveObservation {
  ArchiveStar *star;
  ArchiveFile *file;
  JULIAN      when;
  MAGNITUDE   Mv;
  FILTER      filter;
  int         num_obs;
};

