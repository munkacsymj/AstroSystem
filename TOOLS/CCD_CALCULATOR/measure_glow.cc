/*  measure_glow.cc -- 
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

void usage(void) {
  cerr << "usage: measure_glow -d /home/IMAGES/xxx\n";
  exit(-2);
  /*NOTREACHED*/
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
  int exp_time = (int) (0.5 + info->GetExposureDuration());
  string darkname(dirname);
  darkname += "/dark" + to_string(exp_time) + ".fits";
  Image dark(&(darkname[0]));
  image.subtract(&dark);
  // cout << "dark subtraction completed.\n";
  char filter_letter = filter.NameOf()[0];

  JULIAN exp_midpoint = image.GetImageInfo()->GetExposureMidpoint();

  string output_filename("/tmp/glow_");
  output_filename += filter_letter;
  output_filename += ".csv";
  //cout << "output_filename = " << output_filename << endl;
  ofstream out_file;
  out_file.open(output_filename, ios::out | ios::app);
  if (not out_file.is_open()) {
    cerr << "Unable to write to " << output_filename << endl;
  }

  Statistics *stats = image.statistics();

  out_file << exp_time << ' ' << stats->MedianPixel
	   << " " << exp_midpoint.to_string() << endl;

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
      //cout << "Checking " << entry.path().filename().string()
      //   << " which has extension of " << entry.path().extension().string()
      //   << endl;
      
      if (entry.is_regular_file() and
	  entry.path().extension().string() == string(".fits")) {
	cout << "Processing file " << entry.path().filename().string() << endl;
	ProcessImage(&entry.path().filename().string()[0], directory);
      }
    }
  }
  return 0;
}
