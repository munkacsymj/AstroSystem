/*  stack.cc -- Stack (co-add) multiple images and create new one
 *
 *  Copyright (C) 2007 Mark J. Munkacsy
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
#include <libgen.h>		// for basename()
#include <unistd.h> 		// for getopt()
#include <stdlib.h>		// for atof()
#include <Image.h>
#include <astro_db.h>
#include <string.h>		// for strcmp()
#include <list>

// Ugly, I know, but it's an add-on capability. This is a global
// variable holding the total exposure time of the stacked image. It
// is set to the sum of the exposure times of the images being
// stacked.
double airmass_sum = 0.0;
double total_stack_time = 0.0;
JULIAN exposure_midpoint_time;
int exposure_midpoint_count = 0;
JULIAN ExposureStartTime;
double gain_sum = 0.0;
double cdelt1 = 0.0;
double cdelt2 = 0.0;
char filter_used[16] = "";
Filter Ffilter;
double DATAMAX = 65535.0;

// Here are two more add-on features, equally ugly.
int stack_north_up = 0;
double stack_rotation = 0.0;

void CarryForwardKeywords(Image **i_array,
			  int num_images,
			  Image *final_image);

inline int xround(double x) {
  if(x > 0.0) return (int) (x + 0.5);
  return (int) (x - 0.5);
}

Image *stack_image(char **i_array,
		   int num_images,
		   bool inhibit_quick,
		   bool inhibit_linearization,
		   Image *dark,
		   Image *scale,
		   int do_trim,
		   int use_existing_starlist);

int simple_image_match(IStarList *i1_list, IStarList *i2_list,
		       double expected_x, double expected_y,
		       double *del_x, double *del_y);

struct image_delta {
  double del_x, del_y;
  int count;			// number of other entries with "same"
				// x and y.
};

// image_match: returns 0 on success, 1 if failed
// puts resulting x and y offsets into *del_x and *del_y.
int image_match(IStarList *i1_list, IStarList *i2_list,
		bool inhibit_quick,
		double expected_x, double expected_y,
		double *del_x, double *del_y) {

  if (!inhibit_quick) {
    if(simple_image_match(i1_list, i2_list,
			  expected_x, expected_y,
			  del_x, del_y) == 0) return 0;
  }

  const int i1_size = i1_list->NumStars;
  const int i2_size = i2_list->NumStars;
  const int matrix_size = i1_size * i2_size;

  image_delta *mat = new image_delta[matrix_size];

#define MATRIX(h1, h2) mat[h1*i2_size + h2]

  int j1, j2;
  for(j1 = 0; j1 < i1_size; j1++) {
    for(j2 = 0; j2 < i2_size; j2++) {
      image_delta *pair = &(MATRIX(j1, j2));
      pair->del_x = i2_list->StarCenterX(j2) - i1_list->StarCenterX(j1);
      pair->del_y = i2_list->StarCenterY(j2) - i1_list->StarCenterY(j1);
      pair->count = 0;
    }
  }

#define TOLERANCE 3.0		// 3 pixels? (match to say same transform)
#define EXPECTATION_TOLERANCE 18.0 // 8 pixels? (match to say it's same star)

  for(j1 = 0; j1 < matrix_size; j1++) {
    if(fabs(mat[j1].del_x - expected_x) < EXPECTATION_TOLERANCE &&
       fabs(mat[j1].del_y - expected_y) < EXPECTATION_TOLERANCE) {
      for(j2 = 0; j2 < matrix_size; j2++) {
	if(fabs(mat[j1].del_x - mat[j2].del_x) < 1.0 &&
	   fabs(mat[j1].del_y - mat[j2].del_y) < 1.0)
	  mat[j1].count++;
      }
    }
  }

#if 0
  fprintf(stderr, "-------- expected x = %f, expected y = %f\n",
	  expected_x, expected_y);
  for(j1 = 0; j1 < matrix_size; j1++) {
    if(mat[j1].count > 1) 
      fprintf(stderr, "(%d, %d) del_x = %f, del_y = %f, count = %d\n",
	      j1/i2_size, j1 % i2_size, mat[j1].del_x, mat[j1].del_y,
	      mat[j1].count);
  }
#endif

  int biggest = 0;
  int index_of_biggest = -1;
  for(j1 = 0; j1 < matrix_size; j1++) {
    if(mat[j1].count > biggest) {
      biggest = mat[j1].count;
      index_of_biggest = j1;
    }
  }

  if(index_of_biggest == -1) return 1; // no match

  const double ref_x_delta = mat[index_of_biggest].del_x;
  const double ref_y_delta = mat[index_of_biggest].del_y;
  double sum_err_x = 0.0;
  double sum_err_y = 0.0;
  double sum_sq_x = 0.0;
  double sum_sq_y = 0.0;
  int cnt = 0;
  
  for(j1 = 0; j1 < i1_size; j1++) {
    int min_err_index = -1;
    double min_err = 1000000.0;

    for(j2 = 0; j2 < i2_size; j2++) {
      image_delta *pair = &(MATRIX(j1, j2));
      double err_x = ref_x_delta - pair->del_x;
      double err_y = ref_y_delta - pair->del_y;
      double err_sq = err_x*err_x + err_y*err_y;
      if(err_sq < min_err) {
	min_err = err_sq;
	min_err_index = j2;
      }
    }
    
    if(min_err_index >= 0 &&
       min_err <= TOLERANCE*TOLERANCE) {
      double err_x = ref_x_delta - MATRIX(j1,min_err_index).del_x;
      double err_y = ref_y_delta - MATRIX(j1,min_err_index).del_y;
      /* fprintf(stderr, "star %d matched to %d at (%.1f %.1f)\n",
	 j1, min_err_index,
	 MATRIX(j1,min_err_index).del_x,
	 MATRIX(j1,min_err_index).del_y); */

      cnt++;
      sum_err_x += err_x;
      sum_err_y += err_y;
      sum_sq_x  += (err_x * err_x);
      sum_sq_y  += (err_y * err_y);
    }
  }

  *del_x = ref_x_delta + sum_err_x/cnt;
  *del_y = ref_y_delta + sum_err_y/cnt;
  fprintf(stderr, "Offset = (%f, %f), stdev = (%f, %f), %d matches\n",
	  *del_x, *del_y, sqrt(sum_sq_x/cnt), sqrt(sum_sq_y/cnt), cnt);
  return 0;
}

int main(int argc, char **argv) {
  int ch;			// option character
  char *image_filename = 0;	// filename of the output .fits image file
  char *flatfield_filename = 0;
  char *dark_filename = 0;
  int use_existing_starlist = 0;
  bool inhibit_quick = false; // don't inhibit (i.e., permit quick)
  bool inhibit_linearization = true; // inhbit (i.e., linearize)
  int do_trim = 0;

  Image *flat_image = 0;
  Image *dark_image = 0;

  if(xround(1.3) != 1 ||
     xround(-1.3) != -1 ||
     xround(-0.1) != 0) {
    fprintf(stderr, "xround() failed performance check.\n");
    exit(2);
  }

  // Command line options:
  //
  // -t      trim image to eliminate ragged edges
  // -e      Use existing starlist instead of recomputing
  // -x      Inhibit quick check. Use if starnames aren't unique
  // -L      Inhibit linearization of the images being stacked
  // -d darkfile.fits -s flatfield_file.fits -o filename.fits   Image file (output)
  // all other arguments are taken as names of files to be included in
  // the *stack* operation
  //

  while((ch = getopt(argc, argv, "Lxted:s:o:")) != -1) {
    switch(ch) {
    case 'L':
      inhibit_linearization = true;
      break;
      
    case 't':
      do_trim = 1;
      break;

    case 'x':
      inhibit_quick = true;
      break;

    case 'e':
      use_existing_starlist = 1;
      break;
      
    case 'o':			// image filename
      image_filename = optarg;
      break;

    case 's':			// scale (flatfield) filename
      flatfield_filename = optarg;
      flat_image = new Image(flatfield_filename);
      break;

    case 'd':
      dark_filename = optarg;
      dark_image = new Image(dark_filename);
      break;

    case '?':
    default:
      fprintf(stderr,
	      "usage: %s [-t] [-d dark.fits] [-s flat.fits] -o outputimage_filename.fits \n",
	      argv[0]);
      return 2;			// error return
    }
  }
  argc -= optind;
  argv += optind;

  if(image_filename == 0) {
    fprintf(stderr,
	    "usage: stack [-d dark.fits] [-s flat.fits] -o output.fits \n");
    return 2;			// error return
  }

  ////////////////////////////////////////////////////////////////
  // create an array of images from the remaining arguments
  ////////////////////////////////////////////////////////////////
  if(argc < 1) {
    fprintf(stderr,
	    "usage: stack: at least 1 file must be included in stack\n");
    return 2;
  }

  Image *final = stack_image(argv,
			     argc,
			     inhibit_quick,
			     inhibit_linearization,
			     dark_image,
			     flat_image,
			     do_trim,
			     use_existing_starlist);

  if (final) {
    fprintf(stderr, "writing final image\n");

    ImageInfo *info = final->GetImageInfo();
    info->SetExposureDuration(total_stack_time);
    info->SetDatamax(DATAMAX);
    info->SetAirmass(airmass_sum/exposure_midpoint_count);
    if (filter_used[0] && strcmp(filter_used, "mismatch") != 0) {
      info->SetFilter(Ffilter);
    }
    info->SetEGain(gain_sum);
    if(exposure_midpoint_count) {
      JULIAN mid_time(exposure_midpoint_time.day()/exposure_midpoint_count -
		      total_stack_time/(2.0*24.0*3600.0));
      info->SetExposureStartTime(mid_time); // really, "start_time"
    }
  
    final->WriteFITSFloat(image_filename);

    const char *astro_db_dir = HasAstroDBInDirectory(image_filename);
    if (astro_db_dir) {
      AstroDB astro_db(JSON_READWRITE, astro_db_dir);
      juid_t exposure_juid = astro_db.LookupExposure(argv[0]);
      JSON_Expression *exposure_exp = astro_db.FindByJUID(exposure_juid);
      JSON_Expression *directive_exp = exposure_exp->Value("directive");
      if (directive_exp) {
	juid_t directive = directive_exp->Value_int();
      
	std::list<const char *> filenames;
	for (int i=0; i<argc; i++) {
	  filenames.push_back(argv[i]);
	}
	astro_db.AddRefreshStack(Ffilter.AppName(FILTER_APP_Filename),
				 directive,
				 info->GetObject(),
				 image_filename,
				 filenames,
				 true /*actual content*/);
      }
      astro_db.SyncAndRelease();
    }
  }
}

double GetFITSKeywordData(char *fits_filename) {
  double exposure_time_seconds = 0.0;
  ImageInfo info(fits_filename);

  if (info.ExposureDurationValid()) {
    exposure_time_seconds = info.GetExposureDuration();
  }

  if (info.ExposureStartTimeValid()) {
    ExposureStartTime = info.GetExposureStartTime();
  } else {
    ExposureStartTime = JULIAN(0.0);
  }

  if (info.KeywordPresent("FILTER")) {
    Ffilter = info.GetFilter();
    if (filter_used[0] == 0) {
      strcpy(filter_used, Ffilter.NameOf());
    } else {
      if (strcmp(filter_used, Ffilter.NameOf()) != 0) {
	strcpy(filter_used, "mismatch");
      }
    }
  }

  if (info.RotationAngleValid()) {
    stack_rotation = info.GetRotationAngle();
  } else {
    stack_rotation = 0.0;
  }

  if (info.EGainValid()) {
    gain_sum += info.GeteGain();
  } else {
    gain_sum += 1.6; // advertised ST-9 gain is 1.6 e-/ADU
  }

  if (info.AirmassValid()) {
    airmass_sum += info.GetAirmass();
  } else {
    airmass_sum += 0.0;
  }

  if (info.CDeltValid()) {
    cdelt1 = info.GetCDelt1();
    cdelt2 = info.GetCDelt2();
  } else {
    cdelt1 = cdelt2 = 0.0004222222222;
  }

  if (info.NorthIsUpValid()) {
    stack_north_up = info.NorthIsUp();
  } else {
    stack_north_up = FALSE;
  }
    
  return exposure_time_seconds;
}

struct image_data {
  double offset_x;
  double offset_y;
  int no_match;
} *ImageData;

// star_data has two sets of (x, y) locations. The first is the
// "reference" position, which is what's used to calculate the image's
// offset from the reference.  The other (x, y) is the "last"
// position, which helps us match stars from one image to the next
// when there's been a steady, linear shift from frame to frame. In
// that case, we're left with some very large offsets from the refence
// position, although the image-to-image variation is small.
struct star_data {
  double ref_x;
  double ref_y;
  double last_x;
  double last_y;

  int num_matches;		// number of images with a star here.

  star_data *next;
} *first_star = 0;

void PrintMaxPixels(Image *i, const char *message, double d_max) {
  double biggest = -9e99;

  printf("\n%s\n", message);

  for (int x=0; x<i->width; x++) {
    for (int y=0; y<i->height; y++) {
      const double v = i->pixel(x,y);
      if (v >= d_max) {
	printf("*(%d, %d): %.0lf\n",
	      x, y, v);
      } else if (v >= biggest) {
	biggest = v;
      }
    }
  }

  for (int x=0; x<i->width; x++) {
    for (int y=0; y<i->height; y++) {
      const double v = i->pixel(x,y);
      if (v >= biggest) {
	printf("(%d, %d): %.0lf\n",
	      x, y, v);
      }
    }
  }
}


// perform the actual stacking
Image *stack_image(char **i_array,
		   int num_images,
		   bool inhibit_quick,
		   bool inhibit_linearization,
		   Image *dark,
		   Image *scale,
		   int do_trim,
		   int use_existing_starlist) {
  // use the first image being stacked as the reference image
  total_stack_time = 0.0;
  Image *ref_image = new Image(i_array[0]);
  IStarList *ref_image_list;
  Image *image_list[num_images];
  image_list[0] = ref_image;
  ImageInfo *ref_info = ref_image->GetImageInfo();
  int binning = 1;
  if (ref_info and ref_info->DatamaxValid()) {
    DATAMAX = ref_info->GetDatamax();
  }
  if (ref_info and ref_info->BinningValid()) {
    binning = ref_info->GetBinning();
  }
  //PrintMaxPixels(ref_image, "ref_image", DATAMAX);

  if(dark) ref_image->subtract(dark);
  //PrintMaxPixels(ref_image, "dark-subtracted", DATAMAX);
  if (ref_info and ref_info->CameraValid()) {
    const char *camera = ref_info->GetCamera();
    if (camera[0] == 'S' and
	camera[1] == 'T' and
	camera[2] == '-' and
	camera[3] == '9') {
      const double exp_time = ref_info->GetExposureDuration();
      ref_image->RemoveShutterGradient(exp_time);
    }
    free((void *)camera);
  }

  if(scale) ref_image->scale(scale);
  //PrintMaxPixels(ref_image, "flattened", DATAMAX);

  if(use_existing_starlist) {
    ref_image_list = new IStarList(i_array[0]);
  } else {
    ref_image_list = ref_image->GetIStarList();
  }

  // First verify that all images have the same size
  int w, h;
  w = ref_image->width;
  h = ref_image->height;

  // Okay, all images match sizes
  int images_included = 0;
  ImageData = new image_data[num_images];
  Image *output = new Image(h, w);
  ImageInfo *output_info = output->CreateImageInfo();
  output_info->PullFrom(ref_info);
  
  Image *cell_counts = new Image(h, w);	// used for normalization

  // keep track of maximum offsets up, down, right, and left. Used to
  // support trimming when we are done
  int max_off_right = 0;
  int max_off_left  = 0;
  int max_off_up    = 0;
  int max_off_down  = 0;

  if (num_images > 0) {
    // comput offsets for each image relative to the first
    ImageData[0].offset_x = ImageData[0].offset_y = 0.0;
    ImageData[0].no_match = 0;
    double expected_x = 0.0;
    double expected_y = 0.0;
    for(int j=0; j<num_images; j++) {
      fprintf(stderr, "stack: reading %s\n", i_array[j]);
      Image *i = new Image(i_array[j]);
      ImageInfo *info = i->GetImageInfo();
      image_list[j] = i;

      if(i->width != w || i->height != h) {
	fprintf(stderr, "Image size mismatch: (%dx%d vs. %dx%d)\n",
		w, h, i->width, i->height);
	goto image_done;
      }

      if(dark) i->subtract(dark);
      if (!inhibit_linearization) i->linearize();
      if (info and info->CameraValid()) {
	const char *camera = info->GetCamera();
	if (camera[0] == 'S' and
	    camera[1] == 'T' and
	    camera[2] == '-' and
	    camera[3] == '9') {
	  const double exp_time = info->GetExposureDuration();
	  i->RemoveShutterGradient(exp_time);
	}
	free((void *)camera);
      }
	
      if(scale) i->scale(scale);
      //PrintMaxPixels(i, "next_image", DATAMAX);

      IStarList *this_starlist;
      if(use_existing_starlist) {
	this_starlist = new IStarList(i_array[j]);
      } else {
	this_starlist = i->GetIStarList();
      }

      double new_x, new_y;
      ImageData[j].no_match = image_match(ref_image_list, this_starlist,
					  inhibit_quick,
					  expected_x, expected_y,
					  &new_x, &new_y);
      ImageData[j].offset_x = expected_x = new_x;
      ImageData[j].offset_y = expected_y = new_y;

      if(new_x > max_off_right) max_off_right = (int) (new_x + 1.0);
      if(new_x < max_off_left)  max_off_left  = (int) (new_x - 1.0);
      if(new_y > max_off_up)    max_off_up    = (int) (new_y + 1.0);
      if(new_y < max_off_down)  max_off_down  = (int) (new_y - 1.0);

      if(ImageData[j].no_match) {
	fprintf(stderr, "Skipping ... no match found.\n");
	goto image_done;
      } else {
	images_included++;
      }

      // this is ugly. This function call returns the value of the
      // EXPOSURE keyword and also writes the values of the ROTATION and
      // the NORTH-UP keywords into the corresponding global variables.
      {
	double this_exposure_time = GetFITSKeywordData(i_array[j]);
	total_stack_time += this_exposure_time;
	if(ExposureStartTime.day() != 0.0) {
	  exposure_midpoint_time =
	    exposure_midpoint_time.add_days(ExposureStartTime.day() +
					    (this_exposure_time/(2.0*24.0*3600.0)));
	  exposure_midpoint_count++;
	  // fprintf(stderr, "exposure_midpoint_time = %lf\n", exposure_midpoint_time.day());
	}
      }
      
      {
	double weight[4];
	int x_offset[4];
	int y_offset[4];

	// set up the offset and weight arrays
	double ai, bi;

	double af = modf(-ImageData[j].offset_x, &ai);
	double bf = modf(-ImageData[j].offset_y, &bi);
	const int ai_int = xround(ai);
	const int bi_int = xround(bi);
    
	const int del_x = (af < 0.0 ? -1 : 1);
	const int del_y = (bf < 0.0 ? -1 : 1);

	af = fabs(af);
	bf = fabs(bf);

	weight[0] = af*bf;
	weight[1] = af*(1.0 - bf);
	weight[2] = (1.0 - af) * bf;
	weight[3] = (1.0 - af) * (1.0 - bf);

	x_offset[1] = x_offset[0] = ai_int + del_x;
	x_offset[3] = x_offset[2] = ai_int;
	y_offset[2] = y_offset[0] = bi_int + del_y;
	y_offset[3] = y_offset[1] = bi_int;

	int row, col;
	constexpr double HUGE = 9.9e99;
	for(col = 0; col < w; col++) {
	  for(row = 0; row < h; row++) {
	    for(int d = 0; d < 4; d++) {
	      const int xx = col+x_offset[d];
	      const int yy = row+y_offset[d];
	      if(xx >= 0 && yy >= 0 &&
		 xx < w && yy < h) {
		cell_counts->pixel(xx, yy) += weight[d];
		double v = i->pixel(col, row);
		if (v >= DATAMAX) v = HUGE;
		output->pixel(xx, yy) += weight[d] * v;
	      }
	    }
	  }
	}
      }
    image_done:
      ; //delete i;
    } // end for all images

    CarryForwardKeywords(image_list,
			 num_images,
			 output);

    if (images_included == 0) return 0;
  
    // Now normalize everything
    {
      ImageInfo *info = output->GetImageInfo();
      info->SetBinning(binning);
      ImageInfo *c_info = cell_counts->CreateImageInfo();
      c_info->SetBinning(binning);
      output->scale(cell_counts);

      for(int y=0; y<h; y++) {
	for(int x=0; x<w; x++) {
	  const double v = output->pixel(x,y);
	  if (v >= DATAMAX) output->pixel(x,y) = DATAMAX;
	}
      }
      //PrintMaxPixels(output, "output", DATAMAX);
    }

  } else {     // end if there was more than one image provided
    // The case where just a single image is used...
    int row, col;
    for(row = 0; row < h; row++) {
      for(col = 0; col < w; col++) {
	const double v = ref_image->pixel(col, row);
	output->pixel(col, row) = (v >= DATAMAX ? DATAMAX : (v < 0.0 ? 0.0 : v));
      }
    }
  }

  double sum_pixels = 0.0;
  for(int y=0; y<h; y++) {
    for(int x=0; x<w; x++) {
      const double v = output->pixel(x,y);
      sum_pixels += v;
    }
  }
  const double average = sum_pixels/(w*h);
  const double offset = 500.0 - average;
  if (offset > 0.0) {
    for(int y=0; y<h; y++) {
      for(int x=0; x<w; x++) {
	output->pixel(x,y) += offset;
      }
    }
  }
    
  delete [] ImageData;
  delete cell_counts;

  if(do_trim) {
    fprintf(stderr, "max_off_left  = %d\n", max_off_left);
    fprintf(stderr, "max_off_right = %d\n", max_off_right);
    fprintf(stderr, "max_off_up    = %d\n", max_off_up);
    fprintf(stderr, "max_off_down  = %d\n", max_off_down);
    return output->CreateSubImage(-max_off_down,
				  -max_off_left,
				  h-(max_off_up - max_off_down),
				  w-(max_off_right - max_off_left));
  } else {
    return output;
  }
}

static std::list<std::string> keywords {
  "FRAMEX",
    "FRAMEY",
    "CAMERA",
    "FOCALLEN",
    "TELESCOP",
    "SITELAT",
    "SITELON",
    "PURPOSE",
    "NORTH-UP",
    "ROTATION",
    "CDELT1",
    "CDELT2",
    "BINNING",
    "OFFSET",
    "CAMGAIN",
    "RA_NOM",
    "DEC_NOM",
    "READMODE",
    "FILTER",
    "EXPOSURE",
    "DATAMAX" };

void CarryForwardKeywords(Image **i_array,
			  int num_images,
			  Image *final_image) {
  ImageInfo *final_info = final_image->GetImageInfo();
  if (final_info == nullptr) {
    final_info = final_image->CreateImageInfo();
  }
  
  for (auto s : keywords) {
    bool all_images_share_keyword = true;
    std::string value = "XXX";
    
    for (int n=0; n<num_images; n++) {
      ImageInfo *i_info = i_array[n]->GetImageInfo();
      if (i_info->KeywordPresent(s)) {
	if (value == "XXX") {
	  value = i_info->GetValueLiteral(s);
	} else {
	  if (i_info->GetValueLiteral(s) != value) {
	    all_images_share_keyword = false;
	  }
	}
      } else {
	all_images_share_keyword = false;
      }
    }

    if (all_images_share_keyword ||
	(value != "XXX" and ((s == "RA_NOM") ||
			     (s == "DEC_NOM")))) {
      final_info->SetValue(s, value);
    }
  }
}
