#include <Image.h>
#include <obs_record.h>
#include <mag_from_image.h>
#include "plan_exposure.h"

int main(int argc, char **argv) {

  double mag = magnitude_from_image("/home/IMAGES/4-23-2021/image618.fits",
				    "/home/IMAGES/4-23-2021/dark10.fits",
				    "GSC02688-03149",
				    "ux-cyg");

  ObsRecord all_obs;
  const double b = all_obs.PredictBrightness("GSC02688-03149",
					     'B',
					     mag);
  fprintf(stderr, "predicted blue mag = %lf\n", b);

  std::string dir("/home/IMAGES/4-7-2021");
  InitializeExposurePlanner(dir.c_str());

  for (int i=151; i<263; i++) {
    if (i >= 178 and i < 201) continue;
    char image_name[64];
    sprintf(image_name, "%s/image%03d.fits", dir.c_str(), i);
    Image image(image_name);
    AddImageToExposurePlanner(image, image_name);
  }

  ExposurePlannerPrintMeasurements();

#define U_AUR // RR_MON

#ifdef RR_MON
  MagnitudeList B_mags;
  B_mags.push_back(12.208);
  B_mags.push_back(15.0);
  MagnitudeList V_mags;
  V_mags.push_back(11.961);
  V_mags.push_back(13.8);
  MagnitudeList R_mags;
  R_mags.push_back(11.834);
  R_mags.push_back(10.8);
  MagnitudeList I_mags;
  I_mags.push_back(11.687);
  I_mags.push_back(7.8);
#endif
  
#ifdef U_AUR
  MagnitudeList B_mags;
  B_mags.push_back(11.706);
  B_mags.push_back(12.4);
  MagnitudeList V_mags;
  V_mags.push_back(11.498);
  V_mags.push_back(10.8);
  MagnitudeList R_mags;
  R_mags.push_back(11.386);
  R_mags.push_back(8.4);
  MagnitudeList I_mags;
  I_mags.push_back(11.222);
  I_mags.push_back(5.7);
#endif
  
  ColorMagnitudeList ml;
  ml.insert({ {PHOT_V, V_mags} ,
	      {PHOT_B, B_mags},
	      {PHOT_R, R_mags},
	      {PHOT_I, I_mags}} );
  const ExposurePlanList &epl = GetExposurePlan(ml);
}
