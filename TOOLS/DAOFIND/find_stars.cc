/*  find_stars.cc -- Use IRAF's daofind to locate stars in an image
 *
 *  Copyright (C) 2007, 2021 Mark J. Munkacsy
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
#include <gendefs.h>
#include "params.h"		// local
#include "apbfdfind.h"
#include "fwhm.h"

#define FWHM2SIGMA 0.42467 
void IdentifyRowsToExclude(Image &i, int *rows2excl);

void PrintNumValid(DAOStarlist &sl) {
  int count = 0;
  for (auto s : sl) {
    if (s->valid) count++;
  }
  fprintf(stderr, "valid = %d\n", count);
}

static bool comp_stars(const DAOStar *s1, const DAOStar *s2) {
  return s1->peak_value > s2->peak_value;
}

void usage(void) {
      fprintf(stderr,
	      "usage: find_stars -i image.fits -d dark.fits -s flat.fits\n");
      exit(-2);
}

void preserve_file(const char *filename) {
  // The file is at risk of being overwritten because it uses a
  // re-used name. preserve_file() will rename it, giving it a unique
  // name.
  int pid = getpid();
  char new_filename[850];
  char command[900];
  char preserve_dir[800];

  // We put the file to be preserved into a PRESERVE directory inside
  // the day's IMAGE directory (e.g., /home/IMAGES/12-3-2016/PRESERVE)
  char *homedir = DateToDirname(); // from Image.h
  sprintf(preserve_dir, "%s/PRESERVE", homedir);
  int result = mkdir(preserve_dir, 0777);
  // mkdir will fail if the PRESERVE directory already exists. This is
  // actually okay.
  if (result != 0 && errno != EEXIST) {
    perror("Error creating daily PRESERVE directory:");
    return;
  }
  
  sprintf(new_filename, "%s/preserve.%d", preserve_dir, pid);
  sprintf(command, "cp %s %s", filename, new_filename);
  if(system(command)) {
    fprintf(stderr, "preserve_file: system() failed trying '%s'\n",
	    command);
  } else {
    fprintf(stderr, "Temporary file being preserved as %s\n", new_filename);
  }
}

void print_star_pixels(Image *image, const char *name, double x_center, double y_center) {
  constexpr int box_radius = 10;
  for (int y= y_center-box_radius; y < y_center+box_radius; y++) {
    const double del_y = y-y_center;
    for (int x = x_center-box_radius; x < x_center+box_radius; x++) {
      const double del_x = x-x_center;
      const double r = sqrt(del_x*del_x + del_y*del_y);
      printf("%d,%d,%lf,%lf\n", x, y, r, image->pixel(x,y));
    }
  }
}

int main(int argc, char **argv) {
  int ch;			// option character
  char *image_filename = 0;	// filename of the .fits image file
  char *flat_filename = 0;
  char *dark_filename = 0;
  double threshold = 15.0;
  bool force_recalc = false;
  RunParams rp;

  // Command line options:
  // -i imagefile.fits
  // -d darkfile.fits
  // -s flatfile.fits
  // -q nn.n number of sigma (threshold) [default = 2xstd dev]
  // -f [force recalculation of stars, even if already present]

  while((ch = getopt(argc, argv, "fq:d:s:i:")) != -1) {
    switch(ch) {
    case 'f':			// force recalculation
      force_recalc = true;
      break;
      
    case 'i':			// image filename
      image_filename = optarg;
      break;

    case 'q':
      threshold = atof(optarg);
      break;

    case 'd':
      dark_filename = optarg;
      break;

    case 's':
      flat_filename = optarg;
      break;

    case '?':
    default:
      usage();
   }
  }

  // all four command-line arguments are required
  if(image_filename == 0) {
    usage();
  }

  // Handle a dark image, if specified
  Image image(image_filename);
  if(dark_filename) {
    Image dark(dark_filename);
    image.subtract(&dark);
    force_recalc = true;
  }

  // Handle a flat field image, if specified
  if(flat_filename) {
    Image flat(flat_filename);
    image.scale(&flat);
    force_recalc = true;
  }

  IStarList *orig_i = image.PassiveGetIStarList();
  if (orig_i == 0 || orig_i->NumStars == 0 || force_recalc) {

    {
      if (image.height > 512 or image.width > 512) {
	Image *alt_image = image.CreateSubImage(image.height/2 - 256,
						image.width/2 - 256,
						512, 512);
	rp.median = alt_image->statistics()->MedianPixel;
      } else {
	rp.median = image.statistics()->MedianPixel;
      }
    }
    
    // now calculate the std dev of the background.
    // variance = (sum(x^2))/N - average^2
    
    double sum_sq = 0.0;
    double sum = 0.0;
    int pixel_count = 0;
    const double low_lim = image.HistogramValue(0.2);
    const double high_lim = image.HistogramValue(0.8);

    for (int row=0; row<image.height; row++) {
      for (int col=0; col<image.width; col++) {
	const double v = image.pixel(col, row);
	if (v >= low_lim && v <= high_lim) {
	  pixel_count++;
	  sum += v;
	  sum_sq += (v*v);
	}
      }
    }
    const double average = sum/pixel_count;
    const double background_variance = sum_sq/pixel_count - (average*average);
    const double std_dev = sqrt(background_variance);

    fprintf(stderr, "image standard deviation = %.1f\n", std_dev);

    if (image.GetImageInfo() and image.GetImageInfo()->CDeltValid()) {
      rp.fwhm_psf = 4.5 /*arcsec*/ / image.GetImageInfo()->GetCDelt1();
    } else {
      rp.fwhm_psf = 3.5; // pixels
    }
    fprintf(stderr, "find_stars: using FWHM of %.2lf (pixels)\n", rp.fwhm_psf);
    rp.data_min = 1.0;
    rp.threshold = std_dev*threshold;
    rp.ratio = 1.0;		// circular star PSF
    rp.theta = 0.0;		// N/A, since stars are circular
    rp.nsigma = 1.5;
    rp.readnoise = 13.0;
    rp.sharplo = 0.3;
    rp.sharphi = 1.0;
    rp.roundlo = -2.5;
    rp.roundhi = 2.5;
    // PULL EGAIN from keywords
    // rp.gain_e_per_ADU = egain;

    int rows_to_exclude[image.height] = {};
    IdentifyRowsToExclude(image, rows_to_exclude);

    DAOStarlist found_stars;
    int cycle_number = 0;

    do {
      cycle_number++;
      found_stars.clear();
      ap_bfdfind(image, rp, found_stars);
      rp.convolution->WriteFITSFloat("/tmp/convolution.fits");
      ap_detect(*rp.convolution, *rp.gauss, rp, found_stars, rows_to_exclude);
      ap_sharp_round(found_stars, image, rp);
      ap_xy_round(found_stars, image, rp);
      ap_test(found_stars, image, rp);

#if 0
      std::cout << "starlist follows:\n";
      for (auto star : found_stars) {
	std::cout << "(" << star->x << "," << star->y << ") "
		  << "sharp=" << star->sharp
		  << " round1=" << star->round1
		  << " round2=" << star->round2
		  << " valid=" << star->valid << "\n";
      }
#endif
      
      if(cycle_number == 1) {
	for (auto star : found_stars) {
	  star->peak_value = image.pixel((int)(star->x + 0.5), (int)(star->y+0.5));
	}
	fprintf(stderr, "first pass found_stars   ");
	PrintNumValid(found_stars);
	std::sort(found_stars.begin(), found_stars.end(), comp_stars);
	//found_stars.sort();
	DAOStarlist shortlist;
	int count = 100;
	for (auto s : found_stars) {
	  if (s->valid) {
	    shortlist.push_back(s);
	    if (--count == 0) break;
	  }
	}

	//fprintf(stderr, "shortlist initial    ");
	//PrintNumValid(shortlist);
      
	FWHMParam fwhm_param;
	fwhm_param.FWHMx = rp.fwhm_psf;
	fwhm_param.FWHMy = rp.fwhm_psf;
	fwhm_param.rp = &rp;
  
	measure_fwhm(shortlist, image, fwhm_param);
	if (fwhm_param.valid &&
	    fwhm_param.FWHMx > 2.0 &&
	    fwhm_param.FWHMy > 2.0) {
	  rp.fwhm_psf = fwhm_param.FWHMx;
	  rp.ratio = fwhm_param.FWHMy/fwhm_param.FWHMx;
	} else {
	  break; // don't have an updated FWHM, so can't improve
	}
      }
    } while(cycle_number < 2);

    {
      IStarList newlist;
      int star_id = 0;

      for (auto star : found_stars) {
	if (star->valid) {
	  IStarList::IStarOneStar *new_star = new IStarList::IStarOneStar;

	  sprintf(new_star->StarName, "S%03d", star_id++);
	  new_star->photometry = 0.0;
	  new_star->nlls_x = star->x;
	  new_star->nlls_y = star->y;

	  //      new_star->validity_flags = (NLLS_FOR_XY | PHOTOMETRY_VALID);
	  new_star->validity_flags = (NLLS_FOR_XY);
	  new_star->info_flags = 0;
	  newlist.IStarAdd(new_star);
	  //printf("---- (%.2lf, %.2lf : %d, %d)\n", star->x, star->y, star->nx, star->ny);
	  //print_star_pixels(&image, new_star->StarName, new_star->nlls_x, new_star->nlls_y);
	}
      }

      fprintf(stderr, "find_stars: found %d stars using daofind\n",
	      newlist.NumStars);

      newlist.SaveIntoFITSFile(image_filename, 1);
    }

  }

}

void IdentifyRowsToExclude(Image &i, int *rows2excl) {
  double row_avg[i.height] = {0.0};
  double overall_sum = 0.0;
  double row_sum_sq = 0.0;

  for (int r=0; r<i.height; r++) {
    double sum = 0.0;
    for (int c=0; c<i.width; c++) {
      sum += i.pixel(c, r);
    }
    const double this_row_avg = sum/i.width;
    row_avg[r] = this_row_avg;
    overall_sum += this_row_avg;
    row_sum_sq += (this_row_avg*this_row_avg);
  }

  const double overall_avg = overall_sum/i.height;
  const double overall_stddev = sqrt(row_sum_sq/i.height - overall_avg*overall_avg);

  fprintf(stderr, "image avg = %.1lf, row_stddev = %lf\n", overall_avg, overall_stddev);
  for (int r=0; r<i.height; r++) {
    //fprintf(stderr, "   r=%d, avg=%.1lf\n", r, row_avg[r]);
    double abnormal = fabs(row_avg[r] - overall_avg)/overall_stddev;
    const bool exclude = (abnormal > 4.0 and row_avg[r] < overall_avg);
    rows2excl[r] = exclude;
#if 0
    if (exclude) {
      //fprintf(stderr, "   excluding row = %d, avg (%.1lf) off by %.1lf stddevs\n",
      //	      r, row_avg[r], abnormal);
    } else if(r > 1050) {
      //fprintf(stderr, "   row = %d, avg = %.1lf, abnormal = %.2lf\n", r, row_avg[r], abnormal);
      ;
    }
#endif
  }
}
