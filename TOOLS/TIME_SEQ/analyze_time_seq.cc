/*  analyze_time_seq.cc -- Extract photometry from a time series
 *
 *  Copyright (C) 2021 Mark J. Munkacsy
 *
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
#include <HGSC.h>

#include <math.h>
#include <unistd.h>
#include <stdio.h>
#include <iostream>
#include <filesystem>
#include <list>

struct OutputColumn {
  const char *column_label {nullptr};
  const char *star_name {nullptr};
};

void usage(void) {
  std::cerr << "usage: analyze_time_seq -n fieldname -o output.csv [-d dark.fits]\n"
	    << "         [-s flat.fits] file1.fits file2.fits ..."
	    << std::endl;
  exit(-2);
}

int main(int argc, char **argv) {
  int ch;
  const char *fieldname = nullptr;
  const char *output_filename = nullptr;
  const char *dark_filename = nullptr;
  const char *flat_filename = nullptr;
  const bool remove_shutter_gradient = true;

  // Command line options:
  // -n fieldname
  // -o output.csv
  // -d dark.fits
  // -s flat.fits

  while((ch = getopt(argc, argv, "n:o:d:s:")) != -1) {
    switch(ch) {
    case 'd':
      dark_filename = optarg;
      break;

    case 's':
      flat_filename = optarg;
      break;

    case 'o':
      output_filename = optarg;
      break;

    case 'n':
      fieldname = optarg;
      break;

    case '?':
    default:
      usage();
      /*NOTREACHED*/
    }
  }

  //argc -= optind;
  //argv -= optind;

  if (output_filename == nullptr || fieldname == nullptr) {
    usage();
    /*NOTREACHED*/
  }
	
  FILE *output_fp = fopen(output_filename, "w");
  if (!output_fp) {
    perror("Unable to create output file: ");
    exit(-2);
  }
  
  Image dark(dark_filename);
  Image flat(flat_filename);
  HGSCList hgsc(fieldname);
  if (not hgsc.NameOK()) {
    std::cerr << "Error: analyze_time_seq: Catalog for  "
	      << fieldname
	      << " not found." << std::endl;
    exit(-2);
    /*NOTREACHED*/
  }

  list<const HGSC *> all_comp_stars;
  list<const HGSC *> all_check_stars;
  list<const HGSC *> all_report_stars; // report == "variable" (usually)

  HGSCIterator it(hgsc);
  for (const HGSC *h = it.First(); h; h = it.Next()) {
    if (h->is_comp) all_comp_stars.push_back(h);
    if (h->is_check) all_check_stars.push_back(h);
    if (h->do_submit) all_report_stars.push_back(h);
  }

  std::cout << "Working with " << all_comp_stars.size() << " comp stars." << std::endl;
  std::cout << "Working with " << all_check_stars.size() << " check stars." << std::endl;
  std::cout << "Working with " << all_report_stars.size() << " variable stars." << std::endl;

  if (argc <= optind) {
    std::cerr << "Error: analyze_time_seq: no files to analyze" << std::endl;
    usage();
    /*NOTREACHED*/
  }

  std::cerr << "Starting image loop with argc = " << argc << std::endl;
  while(optind < argc) {
    std::cerr << "Image loop: " << std::endl;
    
    const char *filename = argv[optind++];
    std::filesystem::path image_path(filename);
    const char *simple_filename = strdup(image_path.filename().c_str());

    Image image(filename);
    image.subtract(&dark);
    ImageInfo *info = image.GetImageInfo();
    JULIAN midpoint = info->GetExposureMidpoint();
    const double airmass = (info->AirmassValid() ? info->GetAirmass() : 0.0);

    if (remove_shutter_gradient and
	info and
	info->ExposureDurationValid()) {
      image.RemoveShutterGradient(info->GetExposureDuration());
    }

    image.scale(&flat);
    image.find_stars();
    char command[512];
    const char *temp_image = "/tmp/tmp_image_phot.fits";
    sprintf(command, "cp %s %s;star_match -e -f -n %s -h -b -i %s;photometry -i %s -o %s",
	    filename, temp_image, fieldname, temp_image, temp_image, filename);
    int ret_val = system(command);
    if (ret_val != 0) {
      std::cerr << "star_match+photometry command failed" << std::endl;
      continue; // on to the next image
    }

    Image updated_image(filename);
    IStarList *stars = updated_image.PassiveGetIStarList();
    std::cout << "Image " << simple_filename << " has "
	      << stars->NumStars << " stars." << std::endl;

    // Now find the zero point for this image
    Filter filter = info->GetFilter();
    double sum_comp_star_inst_mags = 0.0;
    int num_comp_stars = 0;
    for (int s=0; s<stars->NumStars; s++) {
      IStarList::IStarOneStar *istar = stars->FindByIndex(s);
      if ((istar->validity_flags & PHOTOMETRY_VALID) == 0) continue;
      HGSC *cat_star = hgsc.FindByLabel(istar->StarName);
      if (cat_star and cat_star->is_comp and
	  cat_star->multicolor_data.IsAvailable(FilterToColor(filter))) {
	const double ref_mag = cat_star->multicolor_data.Get(FilterToColor(filter));
	sum_comp_star_inst_mags += (istar->photometry - ref_mag);
	num_comp_stars++;
      }
    }
    double zero_point = 0.0;
    if (num_comp_stars) {
      zero_point = sum_comp_star_inst_mags/num_comp_stars;
    } else {
      std::cerr << filename << ": no valid comp star measurement." << std::endl;
      continue;
    }

    // Generate an output row
    fprintf(output_fp, "%-16s %.4lf %c %.2lf %.4lf ",
	    simple_filename,	// image filename
	    midpoint.day(),	// JD (exposure midpoint)
	    filter.NameOf()[0],	// filter letter
	    info->GetExposureDuration(),
	    airmass);		// airmass
    for (auto s : all_comp_stars) {
      IStarList::IStarOneStar *istar = stars->FindByName(s->label);
      if (istar and (istar->validity_flags & PHOTOMETRY_VALID)) {
	fprintf(output_fp, "%9.4lf ", istar->photometry - zero_point);
      } else {
	fprintf(output_fp, "          ");
      }
    }
    for (auto s : all_check_stars) {
      IStarList::IStarOneStar *istar = stars->FindByName(s->label);
      if (istar and (istar->validity_flags & PHOTOMETRY_VALID)) {
	fprintf(output_fp, "%9.4lf ", istar->photometry - zero_point);
      } else {
	fprintf(output_fp, "          ");
      }
    }
    for (auto s : all_report_stars) {
      IStarList::IStarOneStar *istar = stars->FindByName(s->label);
      if (istar and (istar->validity_flags & PHOTOMETRY_VALID)) {
	fprintf(output_fp, "%9.4lf ", istar->photometry - zero_point);
      } else {
	fprintf(output_fp, "          ");
      }
    }
    fprintf(output_fp, "\n");
  } // end loop over all images
  fclose(output_fp);
  return 0; // end main()
} 
