/*  analyze_linearity.cc -- Program to characterize CCD linearity
 *
 *  Copyright (C) 2019 Mark J. Munkacsy
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

#include <Statistics.h>
#include <Image.h>
#include <list>
#include <stdio.h>
#include <string.h>		// strcmp()
#include <stdlib.h>		// exit()
#include <unistd.h>		// getopt()
#include <assert.h>		// assert()

enum Purpose {
  P_CONTROL,
  P_LIGHT,
  P_DARK,
  P_BIAS,
  P_SETEXPOSURE
};

struct LStats {
  double median;
  double average;
  unsigned int num_saturated_pixels;
  unsigned int num_pixels;
};

struct LImageInfo {
  const char *filename;
  Purpose purpose;
  double exposure_time;
  Image *image;
  LStats whole_image_stats;
  LStats select_stats;
};

std::list<LImageInfo *> all_exposures;

void ReadAllImages(int argc, char **image_filenames) {
  while(argc--) {
    LImageInfo *i = new LImageInfo;
    const char *this_filename = *image_filenames++;
    i->filename = this_filename;
    i->image = new Image(this_filename);
    ImageInfo *ii = i->image->GetImageInfo();
    i->exposure_time = ii->GetExposureDuration();
    const char *purpose = ii->GetPurpose();
    if (strcmp(purpose, "LINCONTROL") == 0) {
      i->purpose = P_CONTROL;
    } else if (strcmp(purpose, "LINSEQ") == 0) {
      i->purpose = P_LIGHT;
    } else if (strcmp(purpose, "LINSETUP") == 0) {
      i->purpose = P_SETEXPOSURE;
    } else {
      fprintf(stderr, "ERROR: analyze_linearity: invalid PURPOSE for image %s\n",
	      this_filename);
    }
    all_exposures.push_back(i);
  }
}

void FirstPassStatistics(void) {
  std::list<LImageInfo *>::iterator it;

  for (it = all_exposures.begin(); it != all_exposures.end(); it++) {
    LImageInfo *i = (*it);
    i->image->linearize();
    Statistics *s = i->image->statistics();
    i->whole_image_stats.median = s->MedianPixel;
    i->whole_image_stats.average = s->AveragePixel;
    i->whole_image_stats.num_saturated_pixels = s->num_saturated_pixels;
    i->whole_image_stats.num_pixels = i->image->height * i->image->width;
  }
}

// Two arrays (pixel_x[] and pixel_y[] hold  the x- and y-coordinates
// of each of the pixels being used for the subfield assessment.
unsigned int *pixel_x;
unsigned int *pixel_y;
unsigned int num_subfield_pixels;

void SetSubfield(void) {
  // Pick the first LINCONTROL image, and grab all pixels on the
  // histogram between 40% and 60%.
  std::list<LImageInfo *>::iterator it;

  LImageInfo *i = 0;

  for (it = all_exposures.begin(); it != all_exposures.end(); it++) {
    i = (*it);

    if (i->purpose != P_CONTROL) continue;
  }

  assert(i);
  const double min_value = i->image->HistogramValue(0.45);
  const double max_value = i->image->HistogramValue(0.55);
  num_subfield_pixels = 0;
  const unsigned int num_pixels_permitted = i->whole_image_stats.num_pixels/10;
  pixel_x = new unsigned int [num_pixels_permitted];
  pixel_y = new unsigned int [num_pixels_permitted];
  
  for (int x = 0; x < i->image->width; x++) {
    for (int y = 0; y < i->image->height; y++) {
      const double p = i->image->pixel(x, y);
      if (p >= min_value && p <= max_value) {
	pixel_x[num_subfield_pixels] = x;
	pixel_y[num_subfield_pixels] = y;
	if (++num_subfield_pixels >= num_pixels_permitted) {
	  return;
	}
      }
    }
  }
}

static int compare_pixels(const void *a, const void *b) {
  double r = (* (const double *) a) - (* (const double *) b);
  if(r == 0) return 0;
  if(r < 0.0) return -1;
  return 1;
}

void SetSubfieldStats(void) {
  std::list<LImageInfo *>::iterator it;

  double *pixel_array = (double *) malloc(sizeof(double) * num_subfield_pixels);
  if(!pixel_array) {
    perror("Cannot allocate memory for historgramPoint in analyze_linearity.cc");
    exit(2);
  }

  for (it = all_exposures.begin(); it != all_exposures.end(); it++) {
    LImageInfo *i = (*it);

    {
      double *p = pixel_array;
      for (unsigned int n = 0; n < num_subfield_pixels; n++) {
	const unsigned int col = pixel_x[n];
	const unsigned int row = pixel_y[n];
	*p++ = i->image->pixel(col, row);
      }
    }

    const double low_limit = *((double *)HistogramPoint(pixel_array,
							num_subfield_pixels,
							sizeof(double),
							compare_pixels,
							0.1));
    const double high_limit = *((double *)HistogramPoint(pixel_array,
							 num_subfield_pixels,
							 sizeof(double),
							 compare_pixels,
							 0.9));
    i->select_stats.median = *((double *)HistogramPoint(pixel_array,
							num_subfield_pixels,
							sizeof(double),
							compare_pixels,
							0.5));
    double sum_values = 0.0;
    unsigned int num_values = 0;
    {
      for (unsigned int n = 0; n < num_subfield_pixels; n++) {
	const unsigned int col = pixel_x[n];
	const unsigned int row = pixel_y[n];
	const double p = i->image->pixel(col, row);
	if (p >= low_limit && p <= high_limit) {
	  sum_values += p;
	  num_values++;
	}
      }
    }

    i->select_stats.average = sum_values/num_values;
  } // end loop over all images
  free(pixel_array);
}

void usage(void) {
  fprintf(stderr,
	  "usage: analyze_linearity -o outfilefile \n");
  exit(2);			// error return
}

int main(int argc, char **argv) {
  int ch;			// option character
  char *outfilename = 0;

  // Command line options:
  // -o outfilename

  while((ch = getopt(argc, argv, "o:")) != -1) {
    switch(ch) {
    case 'o':			// name of logfile
      outfilename = optarg;
      break;

    case '?':
    default:
      usage();
      /*NOTREACHED*/
    }
  }

  if (outfilename == 0) {
    usage();
    /*NOTREACHED*/
  }

  argc -= optind;
  argv += optind;

  if(argc < 1) {
    fprintf(stderr,
	    "usage: analyze_linearity: at least 1 file must be included on command line.\n");
    return 2;
  }

  ReadAllImages(argc, argv);
  FirstPassStatistics();
  SetSubfield();
  SetSubfieldStats();

  // Now create output file
  FILE *fp = fopen(outfilename, "w");
  if (!fp) {
    perror("analyze_linearity: cannot create output file:");
    return 2;
  }

  fprintf(fp, "#Control files\n");
  std::list<LImageInfo *>::iterator it;
  for (it = all_exposures.begin(); it != all_exposures.end(); it++) {
    LImageInfo *i = (*it);
    if (i->purpose != P_CONTROL) continue;

    fprintf(fp, "%s,%.3lf\n",
	    i->filename, i->select_stats.average);
  }

  fprintf(fp, "#Light files\n");
  for (it = all_exposures.begin(); it != all_exposures.end(); it++) {
    LImageInfo *i = (*it);
    if (i->purpose != P_LIGHT) continue;

    fprintf(fp, "%s,%.2lf,%.3lf\n",
	    i->filename, i->exposure_time, i->select_stats.average);
  }

  fclose(fp);
}
  
    

