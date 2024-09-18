const int FS_X0    = 0;		// x0 (star center, X)
const int FS_Y0    = 1;		// y0 (star center, Y)
const int FS_C     = 2;		// C = total flux
const int FS_B     = 3;		// B = background
const int FS_R     = 4;		// R = blur (FWHM)
const int FS_Beta  = 5;		// Beta = gaussian tail

class focus_state {
public:
  double state_var[8];
  double mel;

  focus_state(void);		// provides default init state

  double &R(void)                { return state_var[FS_R]; }
  double &Beta(void)             { return state_var[FS_Beta]; }
};

// returns -1 if would not converge
// returns 0 if converged okay
int nlls(Image *primary_image, focus_state *fs);
int nlls1(Image *primary_image, focus_state *fs);
Image *nlls_create_image(focus_state *fs, int width, int height);
