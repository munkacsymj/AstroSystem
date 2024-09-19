/*  synthetic_flat.cc -- Build a flat from normal science images
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
#include <stdio.h>
#include <sys/types.h>		// for pid_t
#include <unistd.h> 		// for getopt()
#include <stdlib.h>		// for atof()
#include <sys/stat.h>		// for mkdir()
#include <sys/types.h>		// for mkdir()
#include <bits/stdc++.h>	// for sort()
#include <errno.h>		// errno
#include <Image.h>
#include <IStarList.h>
#include <vector>

void usage(void) {
      fprintf(stderr,
	      "usage: synthetic_flat [-a | -d dark.fits -o newflat.fits] file1.fits file2.fits ...\n");
      fprintf(stderr, "    (-a will auto-choose dark file ane output filenames)\n");
      exit(-2);
}

static int double_lessthan(const void *a, const void *b) {
  double r = (* (const double *) a) - (* (const double *) b);
  if(r == 0) return 0;
  if(r < 0.0) return -1;
  return 1;
}

static bool starts_with(const char *fullstring, const char *pattern) {
  while(*fullstring and *pattern) {
    if (*fullstring++ != *pattern++) return false;
    if (*pattern == 0) return true;
  }
  return false;
}

static std::vector<std::string> keywords {
  "FRAMEX",
    "FRAMEY",
    "BINNING",
    "OFFSET",
    "CAMGAIN",
    "READMODE",
    "FILTER",
    "EXPOSURE",
    "DATAMAX" };

int main(int argc, char **argv) {
  int ch;			// option character
  char *output_filename = nullptr;	// filename of the .fits flat file
  char *dark_filename = nullptr;
  bool auto_name = false; // -a option

  // Command line options:
  // -d darkfile.fits
  // -o output_flatfile.fits
  // -a ... auto-naming

  while((ch = getopt(argc, argv, "ao:d:")) != -1) {
    switch(ch) {
    case 'a':
      auto_name = true;
      break;

    case 'd':
      dark_filename = optarg;
      break;

    case 'o':
      output_filename = optarg;
      break;

    case '?':
    default:
      usage();
   }
  }

  argc -= optind;
  argv += optind;

  if (argc < 1) {
    fprintf(stderr, "usage: synthetic_flat: at least 1 image file must be included.\n");
    return 2;
  }

  char base_dir[128] = "";

  if (auto_name) {
    if (argv[0][0] == '/') {
      // absolute pathname
      if (not starts_with(argv[0], "/home/IMAGES/")) {
	fprintf(stderr, "synthetic_flat: invalid base directory: %s\n", argv[0]);
	exit(-2);
      }
      const char *p = argv[0] + 13;
      while(*p and *p != '/') p++;
      char *d = base_dir;
      for(const char *x = argv[0]; x<p; x++, d++) {
	*d = *x;
      }
      *d = 0;
    } else {
      // relative pathname
      if (getcwd(base_dir, sizeof(base_dir)) == nullptr) {
	fprintf(stderr, "getcwd() failed:");
	perror("getcwd():");
      }
      strcat(base_dir, "/");
      // does the image filename contain a '/'?
      const char *p = argv[0];
      int slash = -1;
      while(*p) {
	if (*p == '/') slash = (p-argv[0]);
	p++;
      }
      if (slash != -1) {
	p = argv[0];
	char *d = base_dir + strlen(base_dir);
	while(p != (argv[0]+slash)) *d++ = *p++;
	*d = 0;
      }
    }
  }

  // all command-line arguments are required
  if((not auto_name) and (dark_filename == nullptr or
			  output_filename == nullptr)) {
    usage();
  }

  Image first_image(argv[0]); // invoke to get width/height

  struct OneImage {
    std::string filename;
    char filtername[4];
    int exp_time;
  };

  struct OneFlat {
    char filtername[4];
    const char *output_name;
    std::vector<std::string> header_keywords;
    std::vector<std::string> keyword_values;
  };

  std::list<OneImage> all_images;
  std::list<OneFlat> all_flats;

  if (auto_name) {
    // Pre-Scan
    fprintf(stderr, "Performing pre-scan.\n");
    for (int n=0; n<argc; n++) {
      const char *image_name = argv[n];
      Image image(image_name);
      ImageInfo *info = image.GetImageInfo();
      OneImage i;
      i.filename = std::string(image_name);
      if (info and info->PurposeValid()) {
	if (strcmp("PHOTOMETRY", info->GetPurpose()) != 0) {
	  fprintf(stderr, "skipping this image\n");
	  continue;
	}
      }
      if (info and info->FilterValid()) {
	strcpy(i.filtername, (info->GetFilter()).NameOf());
	bool found = false;
	for(auto f : all_flats) {
	  if(strcmp(f.filtername, i.filtername) == 0) {
	    found = true;
	    break;
	  }
	}
	if (not found) {
	  OneFlat f;
	  strcpy(f.filtername, i.filtername);
	  char output_file[256];
	  sprintf(output_file, "%s/flat_%s.fits",
		  base_dir, f.filtername);
	  f.output_name = strdup(output_file);
	  all_flats.push_back(f);
	}
      } else {
	i.filtername[0] = 0;
      }
      if (info and info->ExposureDurationValid()) {
	i.exp_time = int(0.5 + info->GetExposureDuration());
      } else {
	i.exp_time = -1;
      }
      all_images.push_back(i);
      fprintf(stderr, "Found %s [%s], %d secs\n",
	      i.filename.c_str(), i.filtername, i.exp_time);
    }
  } else {
    OneFlat f;
    f.output_name = output_filename;
    strcpy(f.filtername, "NA");
    all_flats.push_back(f);

    for (int n=0; n<argc; n++) {
      OneImage i;
      i.filename = std::string(argv[n]);
      strcpy(i.filtername, "NA");
      all_images.push_back(i);
    }
  }

  for (auto output : all_flats) {
    fprintf(stderr, "Working on output file %s\n",
	    output.output_name);
    Image *background_sums = new Image(first_image.height, first_image.width);
    Image *star_mask = new Image(first_image.height, first_image.width);
    Image *weight_sums = new Image(first_image.height, first_image.width);
    Image *pixel_counts = new Image(first_image.height, first_image.width);

    double star_radius = 7.0; // pixels
    if (first_image.GetImageInfo() and first_image.GetImageInfo()->CDeltValid()) {
      star_radius = 2.0 * 4.5 /*arcsec*/ / first_image.GetImageInfo()->GetCDelt1();
    }
    fprintf(stderr, "synthetic_flat: using star_radius of %.1lf (pixels)\n", star_radius);
    const int star_limit = (int) (star_radius + 1.0);
    bool first_file = true;

    for (auto img : all_images) {
      if (strcmp(img.filtername, output.filtername) != 0) continue;
      const char *image_name = img.filename.c_str();
      Image image(image_name);
      ImageInfo *info = image.GetImageInfo();
      if (info == nullptr) {
	fprintf(stderr, "ERROR: image %s has no ImageInfo\n",
		image_name);
	continue;
      }

      bool all_keywords_match = true;
      for (unsigned int i=0; i<keywords.size(); i++) {
	std::string k = keywords[i];
	if (info->KeywordPresent(k)) {
	  if (first_file) {
	    output.header_keywords.push_back(k);
	    output.keyword_values.push_back(info->GetValueLiteral(k));
	  } else {
	    if (info->GetValueLiteral(k) != output.keyword_values[i]) {
	      all_keywords_match = false;
	    }
	  }
	} else {
	  all_keywords_match = false;
	}
      }
      first_file = false;

      if (not all_keywords_match) {
	fprintf(stderr, "ERROR: keyword mismatch in image %s\n",
		image_name);
	continue;
      }

      if (auto_name) {
	static char dark_filestring[256];
	sprintf(dark_filestring, "%s/dark%d.fits",
		base_dir, img.exp_time);
	dark_filename = dark_filestring;
      }
      char buffer[512];
      const char *temp_image = "/tmp/image00x.fits";

      sprintf(buffer, "calibrate -o %s -i %s -d %s;find_stars -i %s -f",
	      temp_image, image_name, dark_filename, temp_image);
      int ret_val = system(buffer);
      if (ret_val) {
	fprintf(stderr, "calibrate/find_stars failed. synthetic_flat quitting.\n");
	exit(-2);
      }
    
      Image cleanimage(temp_image);
      IStarList *stars = cleanimage.PassiveGetIStarList();
      // clear the star_mask
      for (int y=0; y<star_mask->height; y++) {
	for (int x=0; x<star_mask->width; x++) {
	  star_mask->pixel(x,y) = 1.0;
	}
      }

      const double star_radius_squared = star_radius*star_radius;
      // create star_mask
      for (int sn = 0; sn < stars->NumStars; sn++) {
	IStarList::IStarOneStar *star = stars->FindByIndex(sn);
	const int center_x = (int) (star->nlls_x + 0.5);
	const int center_y = (int) (star->nlls_y + 0.5);

	for (int y= -star_limit; y<star_limit; y++) {
	  const double del_y = star->nlls_y - (center_y+y);
	  for (int x=-star_limit; x < star_limit; x++) {
	    const double del_x = star->nlls_x - (center_x+x);
	    const double r_squared = (del_x*del_x + del_y*del_y);
	    const int pixel_x = center_x+x;
	    const int pixel_y = center_y+y;
	  
	    if (r_squared <= star_radius_squared and
		pixel_x >= 0 and pixel_y >= 0 and
		pixel_x < star_mask->width and pixel_y < star_mask->height) {
	      star_mask->pixel(center_x+x, center_y+y) = -1.0; // means do not use
	    }
	  }
	}
      }

      // Calculate background median

      double *background_pixel_list = new double[star_mask->height * star_mask->width];
      int num_background_pixels = 0;
      for (int y = 0; y < star_mask->height; y++) {
	for (int x = 0; x < star_mask->width; x++) {
	  if (star_mask->pixel(x,y) > 0.0) {
	    background_pixel_list[num_background_pixels++] = cleanimage.pixel(x,y);
	  }
	}
      }
      double background_median;
      background_median = * (double *) Median(background_pixel_list,
					      num_background_pixels,
					      sizeof(double),
					      &double_lessthan);
      fprintf(stderr, "background_median uses %d points (%.1lf %%)\n",
	      num_background_pixels, 100.0*num_background_pixels/(star_mask->height*
								  star_mask->width));
      double sum = 0.0;
      for(int n=0; n<num_background_pixels; n++) {
	sum += background_pixel_list[n];
      }
      fprintf(stderr, "background average = %.1lf\n", sum/num_background_pixels);
      delete [] background_pixel_list;
      
      // Add background to the master background accumulation wherever mask is +1
      for (int y = 0; y < star_mask->height; y++) {
	for (int x = 0; x < star_mask->width; x++) {
	  if (star_mask->pixel(x,y) > 0.0) {
	    pixel_counts->pixel(x,y)++;
	    background_sums->pixel(x,y) += cleanimage.pixel(x,y);
	    weight_sums->pixel(x,y) += background_median;
	  }
	}
      }
      fprintf(stderr, "synthetic_flat: image %s has background median = %.1lf\n",
	      image_name, background_median);
    } // end loop over all images

    // calculate some basic statistics for the result
    int num_pixels_skipped = 0;
    int num_pixels_max = 0;
    Image final_flat(first_image.height, first_image.width);
  
    for (int y = 0; y < pixel_counts->height; y++) {
      for (int x = 0; x < pixel_counts->width; x++) {
	if (pixel_counts->pixel(x,y) == 0.0) {
	  num_pixels_skipped++;
	  final_flat.pixel(x,y) = 1.0;
	} else {
	  if (pixel_counts->pixel(x,y) == argc) num_pixels_max++;
	  final_flat.pixel(x,y) = background_sums->pixel(x,y)/weight_sums->pixel(x,y);
	}
      }
    }

    fprintf(stderr, "\n\nNum pixels without data = %d\n", num_pixels_skipped);
    fprintf(stderr, "Num pixels with perfect coverage = %d\n", num_pixels_max);
    fprintf(stderr, "Final flat written to %s\n", output.output_name);

    delete background_sums;
    delete star_mask;
    delete weight_sums;
    delete pixel_counts;

    ImageInfo *final_info = final_flat.GetImageInfo();
    if (final_info == nullptr) {
      final_info = final_flat.CreateImageInfo();
    }

    for (unsigned int i=0; i<output.header_keywords.size(); i++) {
      final_info->SetValue(output.header_keywords[i],
			   output.keyword_values[i]);
    }
    final_flat.WriteFITSFloatUncompressed(output.output_name);
  }
}
