/*  plan_exposure.cc -- manages the selection of exposure times
 *
 *  Copyright (C) 2020 Mark J. Munkacsy

 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program (file: COPYING).  If not, see
 *   <http://www.gnu.org/licenses/>. 
 */

#include "plan_exposure.h"

#include <Image.h>
#include <HGSC.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>		// exit()
#include <dirent.h>
#include <ctype.h>		// isdigit()
#include <iostream>
#include <algorithm>

//****************************************************************
//        Reference Data
//****************************************************************

#define MAX_COLORS 8		// must be big enough to handle all
				// colors in HGSC.h

const char *master_dirname = nullptr;

struct MagnitudeReference {
  double total_fluxrate;	// e-/second integrated over whole aperture
  double ref_magnitude;		// corresponding to total_fluxrate
};

static bool reference_data_valid = false;

static constexpr double PE_ReadNoise = 20.2; // noise electrons/pixel
static double PE_DarkCurrent = 0.003; // electrons/sec/pixel at current temp
static double PE_ApertureArea = 3*3*M_PI; // 3-pixel radius
static double PE_SkyGlowCurrent[MAX_COLORS]; // e-/second/pixel
static MagnitudeReference PE_StarFlux[MAX_COLORS];
//static double PE_FWHM = 4.0;	// arcsec
static double PE_PeakRatio = 0.1; // ratio of ADU peak to total flux
static constexpr double PE_Gain = 3.6; // e-/ADU
static constexpr double PE_Saturation = 50000.0; // ADU

static bool ReferenceDataValid = false;

//****************************************************************
//        Measurement Data
//****************************************************************

struct OneMeasurement {
  std::string filename;
  Filter filter;
  PhotometryColor p_color;	// usable as an array index
  double skyglow;		// Total ADU/pixel/sec
  MagnitudeReference mag_ref;
  JULIAN when;
  double exptime;		// seconds
  double egain;			// system gain, e-/ADU
  bool okay;			// true if all data available
};

static std::list<OneMeasurement> all_measurements;

//****************************************************************
//        Dark Handling
//****************************************************************

static int IsDarkFileName(const char *s) {
  if (s[0] == 'd' and
      s[1] == 'a' and
      s[2] == 'r' and
      s[3] == 'k' and
      isdigit(s[4])) {
    const char *p = s+4;
    while(isdigit(*p)) p++;
    if (p[0] == '.' and
	p[1] == 'f' and
	p[2] == 'i' and
	p[3] == 't' and
	p[4] == 's') {
      return strtol(s+4, nullptr, 10);
    }
  }
  return -1;
}

void ErrorTooFewDarks(int n) {
  static bool err_msg_printed = false;
  if (not err_msg_printed) {
    err_msg_printed = true;
    std::cerr << "ERROR: plan_exposure initialization couldn't find enough darkfiles."
	      << std::endl;
    std::cerr << "    A minimum of two darkfiles is needed. " << n << " found." << std::endl;
  }
  reference_data_valid = false;
}

void ReadDarks(void) {
  DIR *dir_fp = opendir(master_dirname);
  struct dirent *d = nullptr;
  std::string darkdir_str(master_dirname);

  std::list<std::pair<double, double>> dark_data;
  
  while((d = readdir(dir_fp))) {
    // only consider regular files...
    if (not (d->d_type == DT_REG)) continue;
    int dark_time = IsDarkFileName(d->d_name);
    // returns -1 if not a dark filename
    if (dark_time < 0) continue;

    std::string full_dark_path(darkdir_str + '/' + d->d_name);

    Image dark(full_dark_path.c_str());
    Statistics *dark_stats = dark.statistics();
    dark_data.push_back(std::make_pair(dark_time, dark_stats->MedianPixel));
  }

#if 0 // only perform if we actually care about dark current
  // now do a simple regression
  {
    double sum_x = 0.0;
    double sum_y = 0.0;
    double sum_xy = 0.0;
    double sum_xx = 0.0;
    const int n = dark_data.size();

    if (n < 2) {
      ErrorTooFewDarks(n);
    } else {
      for (auto p : dark_data) {
	const double t = p.first;
	const double ADU = p.second;
	sum_x += t;
	sum_y += ADU;
	sum_xy += (t*ADU);
	sum_xx += (t*t);
      }

      const double slope = (n*sum_xy - sum_x*sum_y)/(n*sum_xx - sum_x*sum_x);
      PE_DarkCurrent = slope*PE_Gain;
      reference_data_valid = true;
    }
  }
#else
  PE_DarkCurrent = 0.003;
  reference_data_valid = true;
#endif
}
    
//****************************************************************
//        Initialization
// Must be called exactly once at the beginning. Darks must
// exist. Probably needs to have "optical configuration" added as an
// argument.
//****************************************************************
void InitializeExposurePlanner(const char *homedir) {
  master_dirname = homedir;
  ReadDarks();
  if (reference_data_valid) {
    std::cerr << "Dark current = "
	      << PE_DarkCurrent
	      << " e-/sec/pixel"
	      << std::endl;
  }
}

//****************************************************************
//        AddImageToExposurePlanner()
// Any image of the sky is useful. Will be used for skyglow, HWFM PSF,
// and flux/magnitude ratio. THIS SHOULD BE A RAW IMAGE, NOT
// DARK-SUBTRACTED. 
//****************************************************************
void MeasureSkyGlow(Image &image, OneMeasurement &om, const char *image_filename);
void MeasureStars(Image &image, OneMeasurement &om, const char *image_filename);

void AddImageToExposurePlanner(Image &image, const char *image_filename) {
  OneMeasurement om;
  om.okay = false;
  om.filename = std::string(image_filename);

  ImageInfo *info = image.GetImageInfo();
  om.filter = info->GetFilter();
  om.p_color = FilterToColor(om.filter);
  if (not info->ExposureDurationValid()) return; // reject the point
  om.exptime = info->GetExposureDuration();
  if (not info->EGainValid()) return; // reject the point
  om.egain = info->GeteGain();
  om.okay = true;
 
  // First grab skyglow
  MeasureSkyGlow(image, om, image_filename);
  // Then pull out the star data
  if (om.okay) {
    ReferenceDataValid = false;
    MeasureStars(image, om, image_filename);
  }
  if (om.okay) {
    all_measurements.push_back(om);
  }
}

//****************************************************************
//        MeasureSkyGlow()
//****************************************************************
void MeasureSkyGlow(Image &image, OneMeasurement &om, const char *image_filename) {
  std::string darkname(std::string(master_dirname) +
		       "/dark" + to_string((int)om.exptime) + ".fits");
  
  struct stat statbuf;
  if (stat(darkname.c_str(), &statbuf)) {
    om.okay = false;
  } else {
    Image dark(darkname.c_str());
    Image light(image_filename);
    light.subtract(&dark);
  
    const double median = light.statistics()->MedianPixel;
    om.skyglow = median/om.exptime;
  }
}

//****************************************************************
//        MeasureStars()
//****************************************************************
void MeasureStars(Image &image, OneMeasurement &om, const char *image_filename) {
  om.okay = false;
  ImageInfo *info = image.GetImageInfo();
  if (not info->ObjectValid()) return;
  if (not info->EGainValid()) return;
  const double egain = info->GeteGain();
  const char *object_name = info->GetObject();
  HGSCList catalog(object_name);
  if (not catalog.NameOK()) return;

  std::string darkname(std::string(master_dirname) +
		       "/dark" + to_string((int)om.exptime) + ".fits ");
  
  std::string command1(std::string("calibrate ") +
		       " -d " + darkname +
		       " -i " + image_filename +
		       " -o /tmp/photometry.fits");
		       
  std::cerr << "Running: " << command1 << std::endl;
  int ret = system(command1.c_str());
  if (ret) return;

  std::string command2("find_stars -f -i /tmp/photometry.fits");
  std::cerr << "Running: " << command2 << std::endl;
  ret = system(command2.c_str());
  if (ret) return;

  std::string command3(std::string("star_match -h -f -e ") +
		       " -n " + object_name +
		       " -i /tmp/photometry.fits");
  std::cerr << "Running: " << command3 << std::endl;
  ret = system(command3.c_str());
  if (ret) return;

  std::string command4(std::string("photometry -u ") +
		       " -i /tmp/photometry.fits");
  std::cerr << "Running: " << command4 << std::endl;
  ret = system(command4.c_str());
  if (ret) return;

  Image processed("/tmp/photometry.fits");
  IStarList *stars = processed.GetIStarList();
  double mag_sum = 0.0;
  int num_averaged = 0;
  for (int i=0; i<stars->NumStars; i++) {
    const auto star = stars->FindByIndex(i);
    const double flux = star->nlls_counts*egain;
    //const double photometry = star->photometry;
    const bool valid = (star->validity_flags & PHOTOMETRY_VALID);
    const bool correlated = (star->validity_flags & CORRELATED);

    if (not (valid and correlated)) continue;

    HGSC *cat = catalog.FindByLabel(star->StarName);
    if (cat == nullptr) continue;

    if (not cat->multicolor_data.IsAvailable(om.p_color)) continue;

    const double catalog_mag = cat->multicolor_data.Get(om.p_color);

    // now do something to perform regression
    mag_sum += (catalog_mag - (-2.5*log10(flux/om.exptime)));
    num_averaged++;
  } // end loop over all stars

  if (num_averaged > 0) {
    om.mag_ref.ref_magnitude = mag_sum/num_averaged;
    om.mag_ref.total_fluxrate = 1.0;
    std::cerr << "Averaged " << num_averaged << " stars; mag reference = "
	      << om.mag_ref.ref_magnitude << std::endl;
    om.okay = true;
  }
}

//****************************************************************
//        ExposurePlannerPrintMeasurements
//    Debugging support.
//****************************************************************

void ExposurePlannerPrintMeasurements(void) {
  for (auto om : all_measurements) {
    std::cout << "Filter: " << om.p_color
	      << " Skyglow = " << om.skyglow
	      << " Mag_ref = " << om.mag_ref.ref_magnitude
	      << " ExpTime = " << om.exptime
	      << "  " << om.filename
	      << std::endl;
  }
}

//****************************************************************
//        UpdateReferenceData()
//    Make all the reference data consistent with all observations.
//****************************************************************
void UpdateReferenceData(void) {
  if (ReferenceDataValid) return;

  // Values that need to be refreshed:
  //    1. PE_SkyGlowCurrent
  //    2. PE_StarFlux
  //    3. FWHM

  // SkyGlowCurrent is a simple average for each color
  {
    double sums[MAX_COLORS] = {0.0};
    double counts[MAX_COLORS] = {0};

    for (auto om : all_measurements) {
      counts[om.p_color]++;
      sums[om.p_color] += om.skyglow * om.egain;
    }
    for (int i=0; i<MAX_COLORS; i++) {
      if (sums[i] and counts[i]) {
	PE_SkyGlowCurrent[i] = sums[i] / counts[i];
      } else {
	PE_SkyGlowCurrent[i] = 0.0;
      }
    }
  }

  // StarFlux is a simple average for each color
  {
    double flux_sums[MAX_COLORS] = {0.0};
    double mag_sums[MAX_COLORS] = {0.0};
    double counts[MAX_COLORS] = {0};

    for (auto om : all_measurements) {
      counts[om.p_color]++;
      flux_sums[om.p_color] += om.mag_ref.total_fluxrate;
      mag_sums[om.p_color] += om.mag_ref.ref_magnitude;
    }
    for (int i=0; i<MAX_COLORS; i++) {
      if (counts[i]) {
	PE_StarFlux[i].total_fluxrate = flux_sums[i] / counts[i];
	PE_StarFlux[i].ref_magnitude = mag_sums[i] / counts[i];
      } else {
	PE_StarFlux[i].total_fluxrate = 0.0;
	PE_StarFlux[i].ref_magnitude = 0.0;
      }
    }
  }
  ReferenceDataValid = true;
}

//****************************************************************
//        GetExposurePlan()
//****************************************************************

// These MUST remain sorted by exposure time!

struct PaletteChoice {
  double time;
  int camera_gain;
  int offset;
  int readout_mode;
  double system_gain; // e-/ADU in binned pixel
  double readnoise;   // readnoise per binned pixel
  double data_max;    // binned pixel (ADU) that would saturate
};

#if 1 // QHY268M
static std::list<PaletteChoice> exposure_time_palette
  { { 60.0, 0, 5, 1, 1.0, 3.5*3, 500000.0 },
    { 30.0, 0, 5, 1, 1.0, 3.5*3, 500000.0 },
    { 10.0, 0, 5, 1, 1.0, 3.5*3, 500000.0 },
    { 5.0, 0, 5, 1, 1.0, 3.5*3, 500000.0 },
    //    { 3.0, 0, 5, 3, 1.628, 5.7*4, 500000.0 },
    //    { 1.0, 0, 5, 3, 1.628, 5.7*4, 500000.0 },
  };
#endif

#if 0 // SBIG ST-9
static std::list<PaletteChoice> exposure_time_palette
  { { 60.0, 0, 5, 1, 3.2, 13, 60000.0 },
    { 30.0, 0, 5, 1, 3.2, 13, 60000.0 },
    { 10.0, 0, 5, 1, 3.2, 13, 60000.0 },
    { 5.0, 0, 5, 3, 3.2, 13, 60000.0 },
    //    { 3.0, 0, 5, 3, 1.628, 5.7*4, 500000.0 },
    //    { 1.0, 0, 5, 3, 1.628, 5.7*4, 500000.0 },
  };
#endif

#if 0
    {120.0, false},
      {60.0, false},
	{30.0, true}, // true ==> preferred exposure time
	  {10.0, false},
	    {6.0, false},
	       {3.0, false},
		};
#endif

struct TimeCandidate {
  double exptime;
  bool preferred;
  double total_flux;		// e- for currently-considered star
  bool saturates;
  int num_exposures;
  double SNR;
  double total_dwell_time;	// exposure time plus download time
  int camera_gain;
  int offset;
  int readout_mode;
  double system_gain;
};

const ExposurePlanList &GetExposurePlan(const ColorMagnitudeList &ml) {
  static ExposurePlanList epl;
  epl.exposure_plan_valid = false;
  epl.exposure_plan_list.clear();

  fprintf(stderr, "GetExposurePlan(): starting.\n");
  UpdateReferenceData();
  if (!reference_data_valid) return epl;

  epl.exposure_plan_valid = true;
  // Each color is handled independently
  for (auto pair : ml) {
    const PhotometryColor &filter = pair.first;
    const MagnitudeList &mags = pair.second;

    // if no data is available in this color, then there will be no
    // candidates pushed into epl. Caller must be prepared for this!
    if (mags.size() == 0 or
	PE_SkyGlowCurrent[filter] == 0.0 or
	PE_StarFlux[filter].total_fluxrate == 0.0) continue;

    std::cerr << "Generating plan for color "
	      << ColorToName(filter)
	      << " with magnitudes ";
    for (auto m : mags) {
      std::cerr << m << ' ';
    }
    std::cerr << std::endl;

    const double brightest = *std::min_element(mags.begin(), mags.end());
    const double dimmest = *std::max_element(mags.begin(), mags.end());

    std::vector<TimeCandidate> candidates;
    for (auto t : exposure_time_palette) {
      TimeCandidate c;
      c.exptime = t.time;
      c.camera_gain = t.camera_gain;
      c.offset = t.offset;
      c.readout_mode = t.readout_mode;
      c.system_gain = t.system_gain;
      
      const double delta_mag = (PE_StarFlux[filter].ref_magnitude-brightest);
      const double flux_rate = pow(10.0, delta_mag/2.5); // e-/sec
      c.total_flux = flux_rate * c.exptime; // e-
      if (c.total_flux*PE_PeakRatio/c.system_gain > t.data_max) {
	c.saturates = true;
      } else {
	c.saturates = false;
	// calculate noise
	const double readnoise = t.readnoise * sqrt(PE_ApertureArea);
	const double darkcurrent = PE_DarkCurrent * c.exptime * PE_ApertureArea;
	const double darknoise = sqrt(darkcurrent);
	const double skyglow = PE_SkyGlowCurrent[filter] * c.exptime * PE_ApertureArea;
	const double skyglownoise = sqrt(skyglow);
	c.total_flux = c.exptime * 
	  pow(10.0, (PE_StarFlux[filter].ref_magnitude-dimmest)/2.5);
	const double targetnoise = sqrt(c.total_flux);
	const double oneshotSNR = c.total_flux/sqrt(readnoise*readnoise +
						    darknoise*darknoise +
						    skyglownoise * skyglownoise +
						    targetnoise * targetnoise);
	constexpr double TargetSNR = 100.0;
	const double SNRratio = TargetSNR/oneshotSNR;
	const double exp_num_float = SNRratio*SNRratio;
	c.num_exposures = (int) (0.5 + ceil(exp_num_float));
	std::cerr << "t=" << c.exptime << std::endl;
	std::cerr << "    readnoise = " << readnoise << std::endl;
	std::cerr << "    darknoise = " << darknoise << std::endl;
	std::cerr << "    glownoise = " << skyglownoise << std::endl;
	std::cerr << "    targnoise = " << targetnoise << std::endl;
	std::cerr << "    total flux = " << c.total_flux << std::endl;
      }
      candidates.push_back(c);
    }
    std::cerr << "Choices for " << ColorToName(filter) << ":" << std::endl;
    for (auto c : candidates) {
      std::cerr << "    " << c.exptime << " secs: ";
      if (c.saturates) {
	std::cerr << " saturates." << std::endl;
      } else {
	std::cerr << " " << c.num_exposures << " exposures." << std::endl;
      }
    }

    constexpr int MIN_EXPOSURES = 3;
    //constexpr double DOWNLOAD_TIME = 14.0; // seconds for ST-9
    constexpr double DOWNLOAD_TIME = 3.3; // seconds for QHY268M
    constexpr double MAX_DWELL_TIME = 580.0; // seconds = 4*120sec +
					     // DOWNLOAD
    constexpr double HAPPY_THRESHOLD = 134.0;

    double best_dwell_time = 99.9e99;
    int best_candidate = -1;
    
    for (unsigned int i=0; i<candidates.size(); i++) {
      TimeCandidate &c = candidates[i];
      if (c.saturates) continue; // this one isn't a candidate

      const int num_exposures = std::max(c.num_exposures, MIN_EXPOSURES);
      c.total_dwell_time = num_exposures*(c.exptime + DOWNLOAD_TIME);

      if (c.total_dwell_time < best_dwell_time) {
	best_dwell_time = c.total_dwell_time;
	best_candidate = i;
	if (c.total_dwell_time <= HAPPY_THRESHOLD) break;
      }
    } // end loop over all candidates
    if (best_candidate >= 0) {
      TimeCandidate &c = candidates[best_candidate];
      const int num_exposures = std::min(std::max(c.num_exposures, MIN_EXPOSURES),
					 (int) (0.5 + MAX_DWELL_TIME/(c.exptime+DOWNLOAD_TIME)));
      FilterExposurePlan fep;
      fep.eTime = c.exptime;
      fep.eQuantity = num_exposures;
      fep.eCameraGain = c.camera_gain;
      fep.eCameraMode = c.readout_mode;
      fep.eCameraOffset = c.offset;
      epl.exposure_plan_list.insert(std::pair<PhotometryColor,FilterExposurePlan>(filter, fep));
    }
  }

  std::cerr << "Final recommendations:" << std::endl;
  for (auto c : epl.exposure_plan_list) {
    const PhotometryColor &pc = c.first;
    const FilterExposurePlan &fep = c.second;

    std::cerr << "Filter " << ColorToName(pc) << ": "
	      << fep.eQuantity << " exposures at "
	      << fep.eTime << " seconds" << std::endl;
  }

  return epl;
}
