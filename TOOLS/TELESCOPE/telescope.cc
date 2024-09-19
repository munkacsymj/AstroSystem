/*  telescope.cc -- manage a database of telescope configuration data
 *
 *  Copyright (C) 2021 Mark J. Munkacsy

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

#include <fstream>
#include <iostream>
#include <stdio.h>
#include <math.h>		// NAN
#include <unistd.h>
#include <string.h>		// strcmp()

#include <system_config.h>

void usage(void) {
  std::cerr << "Usage: telescope [-l] <list choices>\n"
	    << "                 [-t telescope]\n"
	    << "                 [-r f_ratio]\n"
	    << "                 [-c camera]\n"
	    << "                 [-f focuser]\n"
	    << "                 [-p pixel_scale (unbinned)]\n"
	    << "                 [-s focus_slope]\n"
	    << "                 [-x corrector]\n"
	    << "                 [-e efl_in_mm]\n";
  exit(0);
}

bool selection_valid(const char *keyword, std::list<std::string> choices) {
  for (auto x : choices) {
    if (strcmp(x.c_str(), keyword) == 0) {
      return true;
    }
  }
  return false;
}

void ListOptionSet(const std::string keyword, std::list<std::string> choices) {
  std::cout << keyword << ':' << std::endl;
  for (auto x : choices) {
    std::cout << "        " << x << std::endl;
  }
}

int main(int argc, char **argv) {
  int ch;			// option character

  const char *set_telescope = nullptr;
  const char *set_efl_string = nullptr;
  const char *set_slope_string = nullptr;
  const char *set_camera = nullptr;
  const char *set_focuser = nullptr;
  const char *set_corrector = nullptr;
  const char *set_pixel_scale = nullptr;
  const char *set_fratio_string = nullptr;

  bool list_options = false;
  bool list_config = true;

  // Command line options:
  // -t telescope (set value)
  // -e focal_length (set numeric value in mm)
  // -r f_ratio (set value)
  // -c camera (set value)
  // -s focus_slope (set value)
  // -p pixel_scale (set value, arcsec/pixel, unbinned)
  // -x corrector (set value)
  // -f focuser (set value)
  // -l (list choices)

  while((ch = getopt(argc, argv, "lr:t:s:p:e:c:x:f:")) != -1) {
    switch(ch) {
    case 't':
      set_telescope = optarg;
      list_config = false;
      break;

    case 'r':
      set_fratio_string = optarg;
      list_config = false;
      break;

    case 'p':
      set_pixel_scale = optarg;
      list_config = false;
      break;

    case 's':
      set_slope_string = optarg;
      list_config = false;
      break;

    case 'e':
      set_efl_string = optarg;
      list_config = false;
      break;

    case 'c':
      set_camera = optarg;
      list_config = false;
      break;

    case 'x':
      set_corrector = optarg;
      list_config = false;
      break;

    case 'f':
      set_focuser = optarg;
      list_config = false;
      break;

    case 'l':
      list_options = true;
      list_config = false;
      break;

    case '?':
    default:
      usage();
   }
  }

  SystemConfig config;

  if (list_config) {
    double efl = config.EffectiveFocalLength();
    double focus_slope = config.FocusSlope();
    std::string telescope = config.Telescope();
    std::string camera = config.Camera();
    std::string focuser = config.Focuser();
    std::string corrector = config.Corrector();
    double pixel_scale = config.PixelScale();

    std::cout << "EFL: " << efl << "mm" << std::endl;
    std::cout << "Focus Slope: " << focus_slope << std::endl;
    std::cout << "Pixel Scale (unbinned): " << pixel_scale << std::endl;
    std::cout << "f/number: " << config.FocalRatio() << std::endl;
    std::cout << "Telescope: " << telescope << std::endl;
    std::cout << "Camera: " << camera << std::endl;
    std::cout << "Focuser: " << focuser << std::endl;
    std::cout << "Corrector: " << corrector << std::endl;
    exit(0);
  }

  if (list_options) {
    ListOptionSet("TELESCOPE (-t)", config.TelescopeChoices());
    ListOptionSet("CAMERA (-c)", config.CameraChoices());
    ListOptionSet("FOCUSER (-f)", config.FocuserChoices());
    ListOptionSet("CORRECTOR (-x)", config.CorrectorChoices());
    exit(0);
  }

  if (set_telescope) {
    if (selection_valid(set_telescope, config.TelescopeChoices())) {
      config.SetTelescope(set_telescope);
    } else {
      std::cerr << "telescope: invalid value for -t option: "
		<< set_telescope << std::endl;
    }
  }

  if (set_camera) {
    if (selection_valid(set_camera, config.CameraChoices())) {
      config.SetCamera(set_camera);
    } else {
      std::cerr << "telescope: invalid value for -c option: "
		<< set_camera << std::endl;
    }
  }

  if (set_focuser) {
    if (selection_valid(set_focuser, config.FocuserChoices())) {
      config.SetFocuser(set_focuser);
    } else {
      std::cerr << "telescope: invalid value for -f option: "
		<< set_focuser << std::endl;
    }
  }

  if (set_corrector) {
    if (selection_valid(set_corrector, config.CorrectorChoices())) {
      config.SetCorrector(set_corrector);
    } else {
      std::cerr << "telescope: invalid value for -x option: "
		<< set_corrector << std::endl;
    }
  }

  if (set_pixel_scale) {
    double p_scale = strtod(set_pixel_scale, nullptr);
    if (p_scale <= 0.0 or p_scale > 99.9) {
      std::cerr << "telescope: invalid pixel scale -p: "
		<< set_pixel_scale << std::endl;
    } else {
      config.SetPixelScale(p_scale);
    }
  }

  if (set_fratio_string) {
    double f_ratio = strtod(set_fratio_string, nullptr);
    if (f_ratio < 3.0 or f_ratio > 20.0) {
      std::cerr << "telescope: invalid f/ratio -r: "
		<< set_fratio_string << std::endl;
    } else {
      config.SetFocalRatio(f_ratio);
    }
  }

  if (set_efl_string) {
    double efl = strtod(set_efl_string, nullptr);
    if (efl <= 0.0) {
      std::cerr << "telescope: invalid effective focal length -e: "
		<< set_efl_string << std::endl;
    } else {
      config.SetEffectiveFocalLength(efl);
    }
  }

  if (set_slope_string) {
    double focus_slope = strtod(set_slope_string, nullptr);
    if (focus_slope <= 0.0) {
      std::cerr << "telescope: invalid focus slope -s: "
		<< set_slope_string << std::endl;
    } else {
      config.SetFocusSlope(focus_slope);
    }
  }
  
  config.Update();
  std::cout << "telescope: configuration updated." << std::endl;
  std::cout << "telescope: updating config file on jellybean." << std::endl;
  {
    char command[256];
    sprintf(command, "scp %s jellybean:%s",
	    CONFIG_FILE, CONFIG_FILE);
    int ret = system(command);
    if (ret == 0) {
      std::cout << "    ...update successful." << std::endl;
    } else {
      std::cout << "    ...update failed." << std::endl;
    }
    std::cout << "telescope: updating config file on jellybean2." << std::endl;
    sprintf(command, "scp %s jellybean2:%s",
	    CONFIG_FILE, CONFIG_FILE);
    ret = system(command);
    if (ret == 0) {
      std::cout << "    ...update successful." << std::endl;
    } else {
      std::cout << "    ...update failed." << std::endl;
    }
  }
  return 0;
}
  

