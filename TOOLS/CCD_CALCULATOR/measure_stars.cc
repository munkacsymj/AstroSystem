/*  measure_stars.cc -- 
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

#include <Image.h>
#include <Statistics.h>
#include <HGSC.h>
#include <stdlib.h>		// exit()
#include <unistd.h>		// getopt()
#include <cstring>
#include <string>
#include <iostream>
#include <fstream>
using namespace std;
#include <filesystem>
namespace fs = std::filesystem;

double image_median;

void usage(void) {
  cerr << "usage: measure_stars -d /home/IMAGES/xxx\n";
  exit(-2);
  /*NOTREACHED*/
}

double PeakADU(Image &image, double center_x, double center_y) {
  int start_x = center_x - 1.0;
  int start_y = center_y - 1.0;

  if (start_x < 0 or start_y < 0) return -1.0;

  double brightest = -1.0;

  for (int x = start_x; x < start_x+2; x++) {
    for (int y = start_y; y < start_y+2; y++) {
      const double p = image.pixel(x,y);
      if (p > brightest) brightest = p;
    }
  }

  return brightest;
}

double IntegratedFlux(Image &image, double center_x, double center_y) {
  constexpr double aperture_radius = 5.0;
  const int start_x = center_x - aperture_radius - 1.0;
  const int end_x = center_x + aperture_radius + 1.0;
  const int start_y = center_y - aperture_radius - 1.0;
  const int end_y = center_y + aperture_radius + 1.0;

  if (start_x < 0 or start_y < 0 or
      end_x >= image.width or end_y >= image.height) return -1.0;

  double sum_flux = 0.0;
  constexpr double r_sq = aperture_radius * aperture_radius;
  for (int x=start_x; x <= end_x; x++) {
    for (int y=start_y; y <= end_y; y++) {
      const double del_x = center_x - x;
      const double del_y = center_y - y;
      const double r = (del_x * del_x + del_y * del_y);
      if (r > r_sq) continue;

      const double p = image.pixel(x,y) - image_median;
      sum_flux += p;
    }
  }
  return sum_flux;
}

void ProcessImage(const char *filename, const char *dirname) {
  string image_file(filename);
  if (!fs::exists(fs::path(filename))) {
    image_file = (string(dirname) + "/" + filename);
  }
  
  Image image(&(image_file[0]));
  ImageInfo *info = image.GetImageInfo();
  if (strcmp(info->GetPurpose(), "PHOTOMETRY") != 0) {
    return;
  }
  
  Filter filter = info->GetFilter();
  PhotometryColor color = FilterToColor(filter);
  int exp_time = (int) (0.5 + info->GetExposureDuration());
  string darkname(dirname);
  darkname += "/dark" + to_string(exp_time) + ".fits";
  Image dark(&(darkname[0]));
  image.subtract(&dark);
  cout << "dark subtraction completed.\n";

  Statistics *stat = image.statistics();
  image_median = stat->MedianPixel;

  const char *object = info->GetObject();
  HGSCList hgsc(object);

  IStarList *starlist = image.PassiveGetIStarList();
  cout << "image has " << starlist->NumStars << " stars.\n";

  char filter_letter = filter.NameOf()[0];
  cout << "Image uses filter " << filter.NameOf() << endl;
  cout << "Filter letter = " << filter_letter << endl;
  string output_filename("/tmp/stars_");
  output_filename += filter_letter;
  output_filename += ".csv";
  cout << "output_filename = " << output_filename << endl;
  ofstream out_file;
  out_file.open(output_filename, ios::out | ios::app);
  if (not out_file.is_open()) {
    cerr << "Unable to write to " << output_filename << endl;
  }

  for (int i=0; i<starlist->NumStars; i++) {
    IStarList::IStarOneStar *star = starlist->FindByIndex(i);
    HGSC *catalog = hgsc.FindByLabel(star->StarName);
    if (catalog and catalog->multicolor_data.IsAvailable(color)) {
      out_file << star->nlls_x << ' ' << star->nlls_y << ' '
	       << PeakADU(image, star->nlls_x, star->nlls_y) - image_median << ' '
	       << IntegratedFlux(image, star->nlls_x, star->nlls_y) << ' '
	       << catalog->multicolor_data.Get(color) << ' '
	       << exp_time
	       << endl;
    }
  }
  out_file.close();
  

}

int main(int argc, char **argv) {
  int ch;			// option character
  const char *directory = 0;
  const char *filename = 0;

  // Command line options:
  // -i imagefile.fits
  // -d image_directory

  while((ch = getopt(argc, argv, "i:d:")) != -1) {
    switch(ch) {
    case 'd':
      directory = optarg;
      break;

    case 'i':
      filename = optarg;
      break;

    case '?':
    default:
      usage();
   }
  }

  if (directory == 0) usage();

  if (filename) {
    ProcessImage(filename, directory);
  } else {
    const fs::path homedir(directory);

    for (const auto& entry : fs::directory_iterator(homedir)) {
      cout << "Checking " << entry.path().filename().string()
	   << " which has extension of " << entry.path().extension().string()
	   << endl;
      
      if (entry.is_regular_file() and
	  entry.path().extension().string() == string(".fits")) {
	cout << "Processing file " << entry.path().filename().string() << endl;
	ProcessImage(&entry.path().filename().string()[0], directory);
      }
    }
  }
  return 0;
}
