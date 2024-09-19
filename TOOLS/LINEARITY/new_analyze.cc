/*  new_analyze.cc -- Program to characterize CCD linearity
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
#include <experimental/filesystem>
#include <ctype.h>
#include <stdio.h>
#include <string.h>		// strcmp()
#include <stdlib.h>		// exit()
#include <unistd.h>		// getopt()
#include <assert.h>		// assert()
#include <fitsio.h>

struct PixelData {
  short count;
  double sum;
  double average;
  bool include;
};

typedef PixelData PixelImage[512][512];

struct PixelLoc {
  PixelLoc(short xx, short yy) : x(xx), y(yy) {;}
  unsigned short x;
  unsigned short y;
};

typedef std::list<PixelLoc> PixelList;

struct ImageData {
  const char *pathname;		// full pathname
  const char *filename;		// just filename
  double pixel_average;
  double image_gain; // ADU per second of exposure
  double slope;
  bool included;
};

class SmoothedFlux; // forward declaration

class Context {
public:
  Context(const char *biasImage) : bias_image(biasImage) { ; }
  ~Context(void) { ; }
  
  Image bias_image;
  PixelImage pi;
  unsigned int pixel_count_included; // pixels in pi with include==true
  const char *home_directory;
  SmoothedFlux *flux_smoother;
  std::list<ImageData *> ListOfImageFiles; // full pathname
  PixelList sample_points;
  FILE *out_fp; // output data
};

//****************************************************************
//        SmoothedFlux
//****************************************************************

typedef struct {
  char filename[32];
  double smoothed_flux;
} OneEntry;

class SmoothedFlux {
public:
  SmoothedFlux(const char *flux_csv_filename);
  ~SmoothedFlux(void);

  // smoothed flux will be a number roughly centered on 1.0; it is a
  // multiplicative scaling factor
  double GetSmoothedFlux(const char *pathname);
private:
  std::list<OneEntry *> all_points;
};

SmoothedFlux::~SmoothedFlux(void) {
  for (auto e : all_points) {
    delete e;
  }
}

double
SmoothedFlux::GetSmoothedFlux(const char *pathname) {
  // after the last '/' in pathname is the name of the file itself
  const char *f = pathname;
  const char *last_component = pathname;
  while(*f) {
    while(*f && *f != '/') f++;
    if (*f == '/') {
      last_component = ++f;
    }
  }

  for (auto e : all_points) {
    if(strcmp(last_component, e->filename) == 0) {
      return e->smoothed_flux;
    }
  }
  fprintf(stderr, "ERROR: Smoothed Flux: filename %s not found.\n",
	  pathname);
  return 0.0;
}

SmoothedFlux::SmoothedFlux(const char *flux_csv_filename) {
  FILE *fp = fopen(flux_csv_filename, "r");
  // CSV file, two columns, first column is filename, second is
  // smoothed flux value (not normalized)
  if(!fp) {
    perror("Cannot open smoothed flux file:");
    return;
  }

  char line[80];
  int skipped_lines = 0;
  double sum_values = 0.0;
  int num_values = 0;
  
  while(fgets(line, sizeof(line), fp)) {
    char *s;
    int num_comma = 0;
    char *last_word_start = line;
    char *words[8];
    
    if (line[0] == '\n') continue; // skip blank lines
    s = line;
    while (*s) {
      if (*s == ',') {
	*s = 0;
	words[num_comma++] = last_word_start;
	last_word_start = s+1;
      }
      if (num_comma > 5) {
	fprintf(stderr, "SmoothedFlux: too many commas in .csv file.\n");
	break;
      }
      s++;
    }
    words[num_comma] = last_word_start;
    
    if (num_comma != 1) {
      fprintf(stderr, "SmoothedFlux: .csv file must have exactly two columns.\n");
      return;
    }

    if (!isdigit(*words[1])) {
      skipped_lines++;
    } else {
      OneEntry *e = new OneEntry;
      if (strlen(words[0]) > sizeof(e->filename)) {
	fprintf(stderr, "ERROR: SmoothedFlux: image filename too long: %s\n",
		words[0]);
      } else {
	strcpy(e->filename, words[0]);
	sscanf(words[1], "%lf", &e->smoothed_flux);
	sum_values += e->smoothed_flux;
	num_values++;
	all_points.push_back(e);
	fprintf(stderr, "remembering %s: %.1lf\n",
		e->filename, e->smoothed_flux);
      }
    }
  }
  const double avg_value = sum_values/num_values;
  for (auto e : all_points) {
    e->smoothed_flux /= avg_value;
  }

  fprintf(stderr, "%d line(s) skipped (header).\n", skipped_lines);
}

//****************************************************************
//        CalibrationImage
//        Processes one single image.
//****************************************************************
void CalibrationImage(ImageData *id,
		      Context &context) {
  Image image(id->pathname);
  image.subtract(&context.bias_image);
  //image.linearize();
  const double hist_low = image.HistogramValue(0.1);
  const double hist_high = image.HistogramValue(0.9);
  const double exposure_time = image.GetImageInfo()->GetExpt3() * context.flux_smoother->GetSmoothedFlux(id->filename);
  const double high_limit = (hist_high < 63000.0 ? hist_high : 63000.0);
  
  double image_sum = 0.0;
  int pixel_count = 0;
  
  // first, compute average for this image.
  for (int y = 0; y < 512; y++) {
    for (int x = 0; x < 512; x++) {
      double value = image.pixel(x, y);
      if (value > hist_low && value < high_limit) {
	// Yes: include it
	image_sum += value;
	pixel_count++;
      }
    }
  }

  const double image_average = image_sum/pixel_count;
  id->pixel_average = image_average;
  // to reduce quantization error, only include images that have
  // average usable pixel values >= 10,000 ADU.
  if (image_average < 10000.0 ||
      image_average > 60000.0 ||
      exposure_time == 0.0 ||
      !id->included) return;

  pixel_count = 0;
  // now compute relative delta light flux for each usable pixel;
  const double gain_factor = id->image_gain/image_average;
  const double gain_term = 1.0 - id->image_gain;
  
  for (int y = 0; y < 512; y++) {
    for (int x = 0; x < 512; x++) {
      const double value = image.pixel(x, y);
      if (value > hist_low && value < high_limit) {
	const double a_value = value * gain_factor + gain_term;
	pixel_count++;
	context.pi[x][y].count++;
	context.pi[x][y].sum += a_value;
      }
    }
  }
  fprintf(stderr, "completed %s: %d pixels included.\n",
	  id->filename, pixel_count);
}

//****************************************************************
//        CalculateAverages
//****************************************************************

void CalculateAverages(Context &context) {
  for (int y = 0; y < 512; y++) {
    for (int x = 0; x < 512; x++) {
      context.pi[x][y].count = 0;
      context.pi[x][y].sum = 0.0;
    }
  }
  
  for (auto f : context.ListOfImageFiles) {
    CalibrationImage(f, context);
  }

  fprintf(stderr, "Threshold count = %d.\n",
	  (int)context.ListOfImageFiles.size()/4);

  int most_popular_pixel_count = 0;
  
  // The "average" that's put into each pixel is a "flux fraction" and
  // it *is* corrected with smoothedFlux. It is multiplicative to the
  // ADU for that pixel.
  int pixels_included = 0;
  for (int y = 0; y < 512; y++) {
    for (int x = 0; x < 512; x++) {
      if(context.pi[x][y].count > most_popular_pixel_count) {
	most_popular_pixel_count = context.pi[x][y].count;
      }
      if(context.pi[x][y].count > (int)context.ListOfImageFiles.size()/4) {
	pixels_included++;
	context.pi[x][y].average = context.pi[x][y].sum/context.pi[x][y].count;
	context.pi[x][y].include = true;
      } else {
	context.pi[x][y].include = false;
      }
    }
  }
  fprintf(stderr, "Total of %d pixels being included in linearity analysis.\n",
	  pixels_included);
  context.pixel_count_included = pixels_included;
  fprintf(stderr, "Most popular pixel was included in %d images.\n",
	  most_popular_pixel_count);

  fprintf(stderr, "Sample Points:\n");
  for (auto p : context.sample_points) {
    fprintf(stderr, "(%d,%d) [%d] %lf %d\n",
	    p.x, p.y, context.pi[p.x][p.y].include,
	    context.pi[p.x][p.y].average,
	    context.pi[p.x][p.y].count);
  }
}

//****************************************************************
//        FitsImageFilePattern (imagennn.fits)
//****************************************************************
bool FitsImageFilePattern(const char *path) {
  if (path[0] != 'i' ||
      path[1] != 'm' ||
      path[2] != 'a' ||
      path[3] != 'g' ||
      path[4] != 'e' ||
      (!isdigit(path[5])) ||
      (!isdigit(path[6])) ||
      (!isdigit(path[7]))) return false;

  const char *s = path+7;
  while(isdigit(*s)) s++;
  return (*s == '.' &&
	  *(s+1) == 'f' &&
	  *(s+2) == 'i' &&
	  *(s+3) == 't' &&
	  *(s+4) == 's' &&
	  *(s+5) == 0);

}

//****************************************************************
//        FindRelevantImages
//****************************************************************
void FindRelevantImages(Context &c) {
  for(auto &p : std::experimental::filesystem::directory_iterator(c.home_directory)) {
    if (FitsImageFilePattern(p.path().filename().c_str())) {
      fitsfile *fptr;
      int status = 0;
      if ( fits_open_file(&fptr, p.path().c_str(), READONLY, &status)) {
	fprintf(stderr, "Error in fits_open_file(%s): %d\n",
		p.path().filename().c_str(), status);
	return;
      }
      char purpose[80];
      if(fits_read_key(fptr, TSTRING, "PURPOSE", purpose, NULL, &status)) {
	purpose[0] = 0;
      }
      
      (void) fits_close_file(fptr, &status);

      if (strcmp(purpose, "LINSEQ") == 0) {
	ImageData *id = new ImageData;
	id->image_gain = 1.0; // default gain
	id->pathname = strdup(p.path().c_str());
	id->filename = strdup(p.path().filename().c_str());
	id->included = true;
			      
	c.ListOfImageFiles.push_back(id);
	//fprintf(stderr, "Using file %s\n", p.path().c_str());
      } else {
	fprintf(stderr, "Rejecting file %s because purpose == '%s'\n",
		p.path().c_str(), purpose);
      }
    } else {
      fprintf(stderr, "Rejecting candidate file %s (not a raw image file)\n", p.path().c_str());
    }
  }
}

//****************************************************************
//        MeasureLinearity
//****************************************************************
void MeasureLinearity(Context &context) {
  // Generate one point per image, so loop through all images

  double slope_sum = 0.0;
  int slope_count = 0;
  
  for (auto id : context.ListOfImageFiles) {
    if (id->pixel_average > 60000.0) {
      id->included = false;
      continue;
    } else {
      id->included = true;
    }
    
    Image image(id->pathname);
    image.subtract(&context.bias_image);
    //image.linearize();
    // Calculate flux-corrected exposure time
    const double exposure_time = image.GetImageInfo()->GetExpt3() *
      context.flux_smoother->GetSmoothedFlux(id->pathname);

    if (exposure_time == 0.0) {
      id->included = false;
      continue;
    }

    double sum_x = 0.0;
    double sum_y = 0.0;
    double sum_xx = 0.0;
    double sum_yy = 0.0;
    double sum_xy = 0.0;
    int count = 0;
    
    for (int y = 0; y < 512; y++) {
      for (int x = 0; x < 512; x++) {
	if (context.pi[x][y].include && image.pixel(x,y)<62000.0) {
	  // slope of this will be in units of (ADU/sec)/(unit flux)
	  const double xv = (context.pi[x][y].average - 1.0);
	  const double yv = (image.pixel(x,y) - id->pixel_average)/exposure_time;

	  count++;
	  sum_x += xv;
	  sum_y += yv;
	  sum_xx += (xv*xv);
	  sum_yy += (yv*yv);
	  sum_xy += (xv*yv);
	}
      }
    }
    const double slope = (count*sum_xy - sum_x*sum_y)/(count*sum_xx - sum_x*sum_x);
    //const double slope = sum_xy/sum_xx;
    id->slope = slope;
    slope_sum += slope;
    slope_count++;
    
    fprintf(context.out_fp, "%s, %.1lf, %.2lf, %lf",
	    id->pathname, id->pixel_average, exposure_time, slope);

    for (auto p : context.sample_points)  {
      fprintf(context.out_fp, ", %lf", image.pixel(p.x, p.y));
    }
    fprintf(context.out_fp, "\n");
  }

  const double average_slope = slope_sum/slope_count;
  fprintf(stderr, "Average image slope = %lf\n", average_slope);
  double max_gain = 0.0;
  double min_gain  = 9999999.9;
  
  for (auto id : context.ListOfImageFiles) {
    if (id->included) {
      id->image_gain = id->slope/average_slope;
      if (id->image_gain < min_gain) min_gain = id->image_gain;
      if (id->image_gain > max_gain) max_gain = id->image_gain;
    }
  }
  fprintf(stderr, "Max image gain = %lf\n", max_gain);
  fprintf(stderr, "Min image gain = %lf\n", min_gain);
  
  fprintf(context.out_fp, "______________________________________\n");
}

//****************************************************************
//        PlotContrastCurve
//****************************************************************
void PlotContrastCurve(const char *output_file,
		       const char *image_file,
		       Context &context) {
  const unsigned int sample_rate = context.pixel_count_included/30000.0;
  FILE *fp = fopen(output_file, "w"); // .csv file
  if (!fp) {
    perror("Cannot open file for PlotContrastCurve:");
    return;
  }

  Image image(image_file);
  image.subtract(&context.bias_image);
  //image.linearize();
  const double exposure_time = image.GetImageInfo()->GetExpt3() *
    context.flux_smoother->GetSmoothedFlux(image_file);

  double pixel_sum = 0.0;
  for (int y = 0; y < 512; y++) {
    for (int x = 0; x < 512; x++) {
      pixel_sum += image.pixel(x,y);
    }
  }
  const double pixel_average = pixel_sum/(512.0*512.0);
  unsigned int pixel_counter = 0;

  for (int y = 0; y < 512; y++) {
    for (int x = 0; x < 512; x++) {
      if (pixel_counter++ == sample_rate) {
	pixel_counter = 0;
	if (context.pi[x][y].include && image.pixel(x,y)<62000.0) {
	
	  const double xv = context.pi[x][y].average;
	  const double yv = (image.pixel(x,y) - pixel_average)/exposure_time;

	  fprintf(fp, "%lf,%lf\n", xv, yv);
	}
      }
    }
  }
  fclose(fp);
}
  
//****************************************************************
//        MAIN
//****************************************************************
int main(int argc, char **argv) {
  fprintf(stderr, "new_analyze: initializing... reading smoothed flux file.\n");
  
  Context *context = new Context("/home/IMAGES/10-4-2019/bias.fits");
  SmoothedFlux smoothed_flux("/home/mark/ASTRO/CURRENT/TOOLS/LINEARITY/smoothed_flux_10-4-2019.csv");
  fprintf(stderr, "Smoothed flux for %s: %lf\n",
	  "/home/IMAGES/10-4-2019/image269.fits",
	  smoothed_flux.GetSmoothedFlux("/home/IMAGES/10-4-2019/image269.fits"));
  context->home_directory = "/home/IMAGES/10-4-2019";
  context->flux_smoother = &smoothed_flux;
  FindRelevantImages(*context);

  // first, clear context for sums for averages
  for (int y = 0; y < 512; y++) {
    for (int x = 0; x < 512; x++) {
      context->pi[x][y].count = 0;
      context->pi[x][y].sum = 0.0;
    }
  }

  context->sample_points.push_back(PixelLoc(45, 208));
  context->sample_points.push_back(PixelLoc(263, 500));
  context->sample_points.push_back(PixelLoc(90, 108));
  context->sample_points.push_back(PixelLoc(98, 308));
  context->sample_points.push_back(PixelLoc(145, 228));
  context->sample_points.push_back(PixelLoc(245, 268));
  context->sample_points.push_back(PixelLoc(254, 408));
  context->sample_points.push_back(PixelLoc(167, 425));
  context->sample_points.push_back(PixelLoc(345, 191));
  context->sample_points.push_back(PixelLoc(445, 358));
  
  context->out_fp = fopen("/tmp/linearity.csv", "w");
  
  CalculateAverages(*context);
  MeasureLinearity(*context);
  //PlotContrastCurve("/tmp/contrast.csv",
  //		    "/home/IMAGES/10-4-2019/image397.fits",
  //		    *context);

  fprintf(stderr, "\n\n----- STARTING CYCLE 2 -----\n");
  CalculateAverages(*context);
  MeasureLinearity(*context);
  fprintf(stderr, "\n\n----- STARTING CYCLE 3 -----\n");
  CalculateAverages(*context);
  MeasureLinearity(*context);
  fclose(context->out_fp);
  return 0;
}
  
