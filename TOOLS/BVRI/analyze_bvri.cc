/*  analyze_bvri.cc -- Takes photometry and assembles into photometry report
 *
 *  Copyright (C) 2016 Mark J. Munkacsy
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
#include <string.h>		// for strcat()
#include <stdio.h>
#include <unistd.h> 		// for getopt()
#include <stdlib.h>		// for atof()
#include <assert.h>
#include <Image.h>
#include <HGSC.h>
#include <libgen.h>		// dirname()
#include <strategy.h>
#include "trans_coef.h"
#include <gendefs.h>
#include <list>
#include <math.h>		// NaN
#include <dbase.h>
#include "colors.h"
#include <bvri_db.h>

//#define ENSEMBLE         // uncomment this line to enable comparison
                           // ensemble processing. If you do, you need
                           // to figure out how to perform multi-color
                           //ensembles! 

void usage(void) {
  fprintf(stderr,
	  "usage: analyze [-c] [-e] [-t] -n starname [-s flat] [-d dark] images\n");
  fprintf(stderr, "     -e     Use existing photometry in the images\n");
  fprintf(stderr, "     -t     Do not apply color transformations\n");
  fprintf(stderr, "     -c     Create virtual comp star (standard field)\n");
  exit(-2);
}

// simplify_path() will create a copy of the pathname pointed at by
// <p> and in the process will turn any consecutive pair of '//' into
// a single '/'
const char *simplify_path(const char *p) {
  char *result = (char *) malloc(strlen(p) + 2);
  const char *s = p;
  char *o = result;
  do {
    if (*s == '/' && *(s+1) == '/') s++;
    *o++ = *s;
  } while(*s++);
  return result;
}


const char *AAVSO_FilterName(const Filter &f) {
  const char *local_filter_name = f.NameOf();
  if (strcmp(local_filter_name, "Vc") == 0) return "V";
  if (strcmp(local_filter_name, "Rc") == 0) return "R";
  if (strcmp(local_filter_name, "Ic") == 0) return "I";
  if (strcmp(local_filter_name, "Bc") == 0) return "B";
  fprintf(stderr, "AAVSO_FilterName: unrecognized filter: %s\n",
	  local_filter_name);
  return "X";
}

static Filter F_V("Vc");
static Filter F_R("Rc");
static Filter F_I("Ic");
static Filter F_B("Bc");

#define NUM_FILTERS 4

int filter_to_index(Filter &f) {
  const char *local_filter_name = f.NameOf();
  if (strcmp(local_filter_name, "Vc") == 0) return 1;
  if (strcmp(local_filter_name, "Rc") == 0) return 2;
  if (strcmp(local_filter_name, "Ic") == 0) return 3;
  if (strcmp(local_filter_name, "Bc") == 0) return 0;
  assert(0); // trigger error
  return -1;
}

// These definitions HAVE to stay identical to what's found in colors.h!
#define i_V 1
#define i_R 2
#define i_I 3
#define i_B 0

Filter index_to_filter(int f_i) {
  switch(f_i) {
  case 1:
    return F_V;
  case 2:
    return F_R;
  case 3:
    return F_I;
  case 0:
    return F_B;
  default:
    assert(0); // trigger error
    /*NOTREACHED*/
    return F_V;
  }
}

PhotometryColor index_to_pc(int index) {
  switch(index) {
  case 0:
    return PHOT_B;
  case 1:
    return PHOT_V;
  case 2:
    return PHOT_R;
  case 3:
    return PHOT_I;
  default:
    assert(0); // bad index_to_pc value
    /*NOTREACHED*/
    return PHOT_V;
  }
}

//****************************************************************
//   Key structures:
// AnalysisImage: one of these for each image
// SingleMeasurement: one for each star in each image
// EachStar: Exactly one for each catalog star that shows up anywhere
//           in the collection of images.
// Measurement: Exactly one for each color of each catalog star
//****************************************************************

class SingleMeasurement; // forward declaration

// one of these for each star for each color
class Measurement {
public:
  Measurement(void);
  ~Measurement(void) {;}
  
  JULIAN jd_exposure_midpoint;
  double instrumental_mag;
  bool   is_transformed;
  double magnitude_raw;  // not transformed
  double magnitude_tr;	 // transformed
  double magnitude_err;
  double check_err_rms; // rms of check star errors
  double stddev;
  bool   stddev_valid;
  int    num_exp;
  double airmass;

  std::list<SingleMeasurement *> datapoints;
};

// There is one of these for each image being analyzed.
class AnalysisImage {
public:
  JULIAN    jd_exposure_midpoint;
  const char *image_filename;
  IStarList *image_starlist;
  ImageInfo *image_info;
  int       image_index;
  int       color_index;        // one of i_V, i_R, i_I, i_B
  double    airmass;
  std::list<SingleMeasurement *> image_stars;
  SingleMeasurement *comp_star;	// each image must have exactly one
				// comp star
};

// each star in each image gets one of these
class EachStar {
public:
  EachStar(void);
  ~EachStar(void) {;}
  
  HGSC                    *hgsc_star;
  Measurement measurements[NUM_FILTERS];
  std::list<SingleMeasurement *> data_points[NUM_FILTERS];
  char                     A_Unique_ID[16];
  bool                     is_comp;
  bool                     is_check;
  int                      is_virtual_check;

  Colors                   color;
};

EachStar::EachStar(void) {
  is_comp = is_check = false;
  is_virtual_check = 0;
  A_Unique_ID[0] = 0;
  for (int i=0; i<NUM_FILTERS; i++) {
    data_points[i].clear();
  }
}

struct SingleMeasurement {
public:
  AnalysisImage *image;
  EachStar      *star;
  double        instrumental_mag;
  double        magnitude_err;
};

Measurement::Measurement(void) {
  stddev_valid = false;
  num_exp = 0;
  airmass = -1.0;
  magnitude_tr = NAN;
  magnitude_err = NAN;
}

std::list<AnalysisImage *> all_images;
std::list<EachStar *> all_stars;
HGSC *comp_hgsc;
EachStar *comp_eachstar;
HGSC *virtual_comp;
EachStar *virtual_comp_eachstar;

double GetBestMag(Measurement *m) {
  if (isnormal(m->magnitude_tr)) return m->magnitude_tr;
  return m->magnitude_raw;
}

EachStar *FindStar(HGSC *cat_star) {
  std::list<EachStar *>::iterator it;
  for (it = all_stars.begin(); it != all_stars.end(); it++) {
    if ((*it)->hgsc_star == cat_star) {
      return (*it);
    }
  }
  return 0;
}

  

char *aavso_format(const char *name) {
  static char buffer[32];
  char *d = buffer;

  do {
    if(*name == '-') {
      *d++ = ' ';
    } else {
      *d++ = toupper(*name);
    }
  } while(*name++);

  return buffer;
}

//****************************************************************
//        Used for standards fields, where there is no single
//        comparison star.
//****************************************************************
void create_virtual_comp(void) {
  // step 1: identify the check stars that are common to all images
  // step 1a: create a composite HGSC star
  int num_raw_checkstars = 0;
  for (auto image : all_images) {
    for (auto star : image->image_stars) {
      if (star->star->hgsc_star->is_check) {
	star->star->is_check = true;
	star->star->is_virtual_check++;
	num_raw_checkstars++;
      }
    }
  }

  HGSC *virtual_cat_star = new HGSC;
  virtual_cat_star->label = strdup("Virtual");
  double measurement[NUM_FILTERS] = { }; // used for averaging of ensemble
  int measurement_counts[NUM_FILTERS] = {};

  const int num_images = all_images.size();
  int num_virtual_checks = 0;
  for (auto star : all_stars) {
    if (star->is_check) {
      if (star->is_virtual_check != num_images) {
	star->is_virtual_check = 0;
      } else {
	num_virtual_checks++;
	for (int i=0; i<NUM_FILTERS; i++) {
	  PhotometryColor pc = index_to_pc(i);
	  if (star->hgsc_star->multicolor_data.IsAvailable(pc)) {
	    measurement[i] += star->hgsc_star->multicolor_data.Get(pc);
	    measurement_counts[i]++;
	  }
	}
      }
    }
  }

  fprintf(stderr, "Virtual comp star made up of %d check stars.\n", num_virtual_checks);
  fprintf(stderr, "   (out of total of %d check stars.)\n", num_raw_checkstars);

  // compute averages across the ensemble and store into virtual_cat_star
  for (int i=0; i<NUM_FILTERS; i++) {
    if (measurement_counts[i]) {
      PhotometryColor pc = index_to_pc(i);
      double average_mag = (measurement[i]/measurement_counts[i]);
      virtual_cat_star->multicolor_data.Add(pc, average_mag);
    }
  }

  // step 2: create a virtual comp star in each image
  EachStar *virt_eachstar = new EachStar;
  virt_eachstar->hgsc_star = virtual_cat_star;
  
  for (auto image : all_images) {
    double virtual_inst_mag = 0.0;
    for (auto star : image->image_stars) {
      if (star->star->is_virtual_check) {
	virtual_inst_mag += star->instrumental_mag;
      }
    }
    image->comp_star = new SingleMeasurement;
    image->comp_star->image = image;
    image->comp_star->star = virt_eachstar;
    image->comp_star->instrumental_mag = virtual_inst_mag/num_virtual_checks;
    virt_eachstar->data_points[image->color_index].push_back(image->comp_star);
    virt_eachstar->color.AddColor(image->color_index, image->comp_star->instrumental_mag);
    Measurement &tm = virt_eachstar->measurements[image->color_index];
    tm.jd_exposure_midpoint = image->jd_exposure_midpoint;
    tm.instrumental_mag = image->comp_star->instrumental_mag;
    tm.is_transformed = false;
    tm.num_exp = 1;
    tm.airmass = image->airmass;
  }
  comp_hgsc = virtual_cat_star;
  comp_eachstar = virt_eachstar;
  virt_eachstar->color.AddComp(&virt_eachstar->color); // comp star is its
						// own comp star...
}

int
main(int argc, char **argv) {
  int ch;			// option character
  // FILE *fp_out = 0;
  char *starname = 0;
  char *darkfilename = 0;
  char *flatfilename = 0;
  int  use_existing_photometry = 0;
  bool inhibit_transforms = false;
  bool use_check_for_comp = false;
  const char *root_dir;

  // Command line options:
  // -n star_name       Name of region around which image was taken
  // -d dark.fits
  // -s flat.fits
  // -o output_filename
  // -e                 Use existing photometry already in image files
  // -t                 Inhibit transformations
  // -c                 No comp: use check stars instead

  while((ch = getopt(argc, argv, "cd:s:n:o:te")) != -1) {
    switch(ch) {
    case 'c':
      use_check_for_comp = true;
      break;

    case 'n':			// name of star
      starname = optarg;
      break;

    case 'e':
      use_existing_photometry = 1;
      break;

    case 'd':
      darkfilename = optarg;
      break;

    case 't':
      inhibit_transforms = true;
      break;

    case 's':
      flatfilename = optarg;
      break;

    case '?':
    default:
      usage();
    }
  }

  if(starname == 0) {
    usage();
  }
  
  argc -= optind;
  argv += optind;

  char HGSCfilename[132];
  sprintf(HGSCfilename, CATALOG_DIR "/%s", starname);
  FILE *HGSC_fp = fopen(HGSCfilename, "r");
  if(!HGSC_fp) {
    fprintf(stderr, "Cannot open catalog file for %s\n", starname);
    exit(-2);
  }

  // Get the strategy for this star so we can pull off any info that's
  // of use (in particular, the REMARKS).
  // Strategy::FindAllStrategies(0);
  // Strategy *strategy = Strategy::FindStrategy(starname);

#if 0
  const char *general_remarks = strategy->remarks();
  if(general_remarks) {
    fprintf(fp_out, "%s", general_remarks);
    fprintf(fp_out, "################################################\n");
  }
#endif

  HGSCList Catalog(HGSC_fp);
  // argc now contains a count of the total number of images being analyzed
  AnalysisImage ImageArray[argc];

  int image_count;
  int images_per_filter[NUM_FILTERS] {0};
  for(image_count = 0; image_count < argc; image_count++) {
    const char *this_image_name = simplify_path(argv[image_count]);
    char orig_image_buffer[128];
    strcpy(orig_image_buffer, this_image_name);
    // char *orig_image_name = basename(orig_image_buffer);
    root_dir = dirname(orig_image_buffer);

    fprintf(stderr, "Reading %s\n", this_image_name);
    all_images.push_back(&ImageArray[image_count]);
    ImageArray[image_count].image_filename = this_image_name;
    ImageArray[image_count].image_index = image_count;
    ImageArray[image_count].comp_star = 0;
    Image *orig_image = new Image(this_image_name);
    ImageInfo *info = orig_image->GetImageInfo();
    ImageArray[image_count].image_info = info;
    ImageArray[image_count].jd_exposure_midpoint = info->GetExposureMidpoint();
    if (info->AirmassValid()) {
      ImageArray[image_count].airmass = info->GetAirmass();
    } else {
      ImageArray[image_count].airmass = -1.0;
    }
    Filter this_image_filter = info->GetFilter();
    int this_filter_index = filter_to_index(this_image_filter);
    ImageArray[image_count].color_index = this_filter_index;
    images_per_filter[this_filter_index]++;
      
    // If photometry is to be done, apply dark and flat files, then
    // invoke "photometry" command
    if(!use_existing_photometry) {
      if(darkfilename || flatfilename) {
	fprintf(stderr, "Handling image processing.\n");
	IStarList orig_list(this_image_name);

	if(darkfilename) {
	  Image dark_image(darkfilename);
	  orig_image->subtract(&dark_image);
	}
	if(flatfilename) {
	  // THIS NEEDS TO BE CONDITIONAL! Test if camera is ST-9 first!!
	  //orig_image->RemoveShutterGradient(info->GetExposureDuration());
	  Image flat_image(flatfilename);
	  orig_image->scale(&flat_image);
	}
	this_image_name = "/tmp/imageq.fits";
	unlink(this_image_name); // in case already exists
	orig_image->WriteFITS(this_image_name);
	orig_list.SaveIntoFITSFile(this_image_name, 1);
      }

      char photometry_command[2248];
      char abs_image_path[2048];

      // make image path absolute (needed for IRAF in photometry)
      if (*this_image_name == '/') {
	// Already absolute
	strcpy(abs_image_path, this_image_name);
      } else {
	// Relative path
	getcwd(abs_image_path, sizeof(abs_image_path));
	strcat(abs_image_path, "/");
	strcat(abs_image_path, this_image_name);
      }
      
      sprintf(photometry_command, COMMAND_DIR "/photometry -i %s\n",
	      abs_image_path);
      system(photometry_command);
    }

    // Create a local IStarList
    IStarList *List = new IStarList(this_image_name);
    // int comp_count = 0;

    // Make a pass through all the stars in the image. Use this pass
    // to:
    // 1. Build the all_stars list
    // 2. Build a SingleMeasurement for each star in the image
    // 3. Find the comp star for the image

    for(int i=0; i < List->NumStars; i++) {
      IStarList::IStarOneStar *this_star = List->FindByIndex(i);
      if((this_star->validity_flags & PHOTOMETRY_VALID) &&
	 (this_star->validity_flags & CORRELATED)) {
	SingleMeasurement *sm = new SingleMeasurement;
	// EachStar *analysis_star = new EachStar;
	ImageArray[image_count].image_stars.push_back(sm);

	// Create an "EachStar" entry if one doesn't already
	// exist. Build the all_stars list in the process.
	HGSC *cat_entry = Catalog.FindByLabel(this_star->StarName);
	if (cat_entry == 0) continue; // odd case; not sure how it happens
	EachStar *star = FindStar(cat_entry);
	if (!star) {
	  star = new EachStar;
	  star->hgsc_star = cat_entry;
	  star->is_comp = cat_entry->is_comp;
	  star->is_check = cat_entry->is_check;
	  if (cat_entry->A_unique_ID) {
	    strcpy(star->A_Unique_ID, cat_entry->A_unique_ID);
	  }
	  all_stars.push_back(star);
	}

	if (star->is_comp) {
	  ImageArray[image_count].comp_star = sm;
	  comp_hgsc = cat_entry;
	  comp_eachstar = star;
	}

	fprintf(stderr, "Found %s %s\n",
		star->hgsc_star->label, (star->hgsc_star->is_comp ?
					 "(comp)" : ""));
	sm->image = &ImageArray[image_count];
	sm->star = star;
	sm->instrumental_mag = this_star->photometry;
	if (this_star->validity_flags & ERROR_VALID) {
	  sm->magnitude_err = this_star->magnitude_error;
	} else {
	  sm->magnitude_err = NAN;
	}
	star->data_points[sm->image->color_index].push_back(sm);
      } // end if photometry was valid for this star
    } // end loop over all stars in the IStarList
    // Check to see if the image had a usable comp star measurement
    if (ImageArray[image_count].comp_star == 0) {
      fprintf(stderr, "Image %s has no comp star.\n",
	      ImageArray[image_count].image_filename);
    }
  } // end loop over all images

  if (use_check_for_comp) {
    // This will set comp_hgsc, comp_eachstar
    create_virtual_comp();

  }

  // Now loop over all stars to average together all
  // SingleMeasurements for each catalog star. This step only has
  // meaning if multiple measurements were made for one filter. (i.e.,
  // if images weren't stacked). This adds the twist that we have to
  // convert from instrumental magnitudes to measurements relative to
  // the comp star if we have multiple images for a color.
  bool perform_averaging = false;

  // first, figure out if we need to do this averaging . . . 

  std::list<EachStar *>::iterator s_it;
#if 0 // This loop checks for multiple stars of the same color with
      // the same name. It *might* be useful if it turns out that this
      // situation causes a problem somewhere.
  for (s_it = all_stars.begin(); s_it != all_stars.end(); s_it++) {
    for (int i=0; i < NUM_FILTERS; i++) {
      if ((*s_it)->data_points[i].size() > 1) {
	fprintf(stderr, "star %s has %ld points for filter %d\n",
		(*s_it)->hgsc_star->label, (*s_it)->data_points[i].size(), i);
	perform_averaging = true;
	goto quit_scan;
      }
    }
  }
#endif
  
  for (int i=0; i<NUM_FILTERS; i++) {
    if (images_per_filter[i] > 1) {
      perform_averaging = true;
      break;
    }
  }

  if (perform_averaging) {
    fprintf(stderr, "Will perform averaging.\n");
  } else {
    fprintf(stderr, "Single measurement per color; analyzing with instrumental mags.\n");
  }

  if (comp_hgsc == 0 || comp_eachstar == 0) {
    fprintf(stderr, "No comp star found -- cannot proceed.\n");
    exit(-2);
  }

  // . . . for each star . . .
  for (s_it = all_stars.begin(); s_it != all_stars.end(); s_it++) {
  // . . . and for each filter
    for (int i=0; i < NUM_FILTERS; i++) {
      int num_points = 0;
      double sum_mag = 0.0;
      double sum_jd = 0.0;
      double sum_airmass = 0.0;
      int airmass_count = 0;
      PhotometryColor pc = index_to_pc(i);
      Measurement *m = &((*s_it)->measurements[i]);
      m->num_exp = 0;

      // convert from an instrumental mag to a differential mag
      if (comp_hgsc->multicolor_data.IsAvailable(pc)) {
	double ref_magnitude = comp_hgsc->multicolor_data.Get(pc);

	std::list<SingleMeasurement *>::iterator sm_it;
	// if "perform_averaging" is set, will need to average
	// multiple measurements
	for (sm_it = (*s_it)->data_points[i].begin();
	     sm_it != (*s_it)->data_points[i].end();
	     sm_it++) {
	  SingleMeasurement *sm = (*sm_it);
	  if (sm->image->comp_star) {
	    double magnitude;
	    if (perform_averaging) {
	      magnitude = sm->instrumental_mag +
		(ref_magnitude - sm->image->comp_star->instrumental_mag);
	    } else {
	      magnitude = sm->instrumental_mag;
	      m->magnitude_err = sm->magnitude_err;
	    }
	    sum_mag += magnitude;
	    sum_jd += sm->image->jd_exposure_midpoint.day();
	    if (sm->image->airmass >= 0.0) {
	      airmass_count++;
	      sum_airmass += sm->image->airmass;
	    }
	    num_points++;
	    m->datapoints.push_back(sm);
	  }
	}
	m->num_exp = num_points;
	m->airmass = -1.0; // corrected below if airmass is valid
	if (num_points) {
	  m->jd_exposure_midpoint = sum_jd/num_points;
	  // if we aren't performing averaging, setting
	  // magnitude_raw will be overwritten later on. Setting it
	  // here won't break anything and is absolutely necessary
	  // if we *are* performing averaging.
	  m->magnitude_raw =
	    m->instrumental_mag = sum_mag/num_points;
	  if (airmass_count) {
	    m->airmass = sum_airmass/airmass_count;
	  }
	}
      }
    } // end loop over all filters
  } // end loop over all stars

  // Now calculate and transform colors for each star. With the colors
  // converted, calculate transformed instrumental magnitudes for each
  // star, and convert from instrumental magnitudes to absolute
  // magnitudes.
  TransformationCoefficients tr; // pick up standard tr coefficients
  for (s_it = all_stars.begin(); s_it != all_stars.end(); s_it++) {
    // Let the star know about the comp star
    (*s_it)->color.AddComp(&comp_eachstar->color);

    // Pick up each filtered measurement and add to the color structure
    for (int i=0; i < NUM_FILTERS; i++) {
      Measurement *m = &((*s_it)->measurements[i]);
      if (m->num_exp > 0) {
	(*s_it)->color.AddColor(i, m->instrumental_mag);
	// Can't do transformations yet because data hasn't been
	// loaded into the comp star
      }
    }
  }

  //
  double zeros[NUM_FILTERS];
  for (int i=0; i<NUM_FILTERS; i++) {
    PhotometryColor pc = index_to_pc(i);
    if (comp_hgsc->multicolor_data.IsAvailable(pc) &&
	comp_eachstar->measurements[i].num_exp > 0) {
      zeros[i] = comp_hgsc->multicolor_data.Get(pc) -
	comp_eachstar->measurements[i].instrumental_mag;
    } else {
      zeros[i] = NAN;
    }
  }

  // *Now* we can go do transformations
  for (s_it = all_stars.begin(); s_it != all_stars.end(); s_it++) {
    if (!inhibit_transforms) {
      (*s_it)->color.Transform(&tr);
    }
    for (int i=0; i<NUM_FILTERS; i++) {
      double magnitude;
      bool is_transformed;
      (*s_it)->color.GetMag(i, &magnitude, &is_transformed);
      (*s_it)->measurements[i].magnitude_raw =
	(*s_it)->measurements[i].instrumental_mag + zeros[i];
      (*s_it)->measurements[i].is_transformed = is_transformed;
      if (is_transformed) {
	(*s_it)->measurements[i].magnitude_tr = magnitude + zeros[i];
      } else {
	(*s_it)->measurements[i].magnitude_raw = magnitude + zeros[i];
      }
    } // end loop over all filters
  } // end loop over all stars


  // *Now* we can compute check star errors (keep colors separate)
  double sum_check_err_sq[NUM_FILTERS] = {}; // initialize to 0
  int num_check[NUM_FILTERS] = {}; // initialize to 0
  
  for (s_it = all_stars.begin(); s_it != all_stars.end(); s_it++) {
    EachStar *s = (*s_it);
    // if the star isn't a check star, skip over it
    if (s->is_check == 0) continue;

    for (int i=0; i<NUM_FILTERS; i++) {    
      PhotometryColor pc = index_to_pc(i);
      if (s->hgsc_star->multicolor_data.IsAvailable(pc) == false) continue;
      double check_reference = s->hgsc_star->multicolor_data.Get(pc);

      double measured_mag = GetBestMag(&(s->measurements[i]));
      if (Colors::is_valid(measured_mag)) {
	const double err =  measured_mag - check_reference;
	sum_check_err_sq[i] += (err*err);
	num_check[i]++;
      }
    } // end loop over all filters
  } // end loop over all stars

  // Finish computing check star errors
  double check_err_rms[NUM_FILTERS];
  for (int i=0; i<NUM_FILTERS; i++) {
    if (num_check[i]) {
      check_err_rms[i] = sqrt(sum_check_err_sq[i]/num_check[i]);
    } else {
      check_err_rms[i] = -1.0;
    }
  }

  // All the data will be put into the database for the day
  char database_name[256];
  sprintf(database_name, "%s/bvri.db", root_dir);
  BVRI_DB *db = new BVRI_DB(database_name, DBASE_MODE_WRITE);

  fprintf(stderr, "DBASE starts off with %d records.\n",
	  db->NumRecords());

  // Now erase any existing elements associated with this target star
  db->DeleteStarRecords(starname);
  fprintf(stderr, "DBASE holds %d records after erase().\n",
	  db->NumRecords());
  
  // ... and loop through all data from these images
  BVRI_REC_list *rl = new BVRI_REC_list;
  for (s_it = all_stars.begin(); s_it != all_stars.end(); s_it++) {
    // Write results
    for (int c=0; c<NUM_FILTERS; c++) {
      BVRI_DB_REC *rec = new BVRI_DB_REC;

      PhotometryColor pc = index_to_pc(c);
      Measurement *this_meas = &((*s_it)->measurements[c]);
      Filter color = index_to_filter(c);
      char remarks0[2048];	// remarks field is concatenation of
				// remarks1 and remarks0
      char remarks1[2048];
      char remarks2[2048];
      const char *AAVSO_Color_Letter = AAVSO_FilterName(color);
      
      if (this_meas->num_exp < 1) continue;

      rec->DB_obs_time = this_meas->jd_exposure_midpoint;
      rec->DB_fieldname = strdup(starname);
      rec->DB_comparison_star_auid = strdup(comp_eachstar->A_Unique_ID);
      rec->DB_AAVSO_filter_letter = AAVSO_Color_Letter[0];
      rec->DB_starname = strdup((*s_it)->hgsc_star->label);
      rec->DB_is_comp = (*s_it)->is_comp;
      rec->DB_is_check = (*s_it)->is_check;
      if ((*s_it)->A_Unique_ID[0]) {
	rec->DB_AUID = strdup((*s_it)->A_Unique_ID);
      } else {
	rec->DB_AUID = 0;
      }
      rec->DB_airmass = this_meas->airmass;
      rec->DB_rawmag = this_meas->magnitude_raw;
      rec->DB_instmag = this_meas->instrumental_mag;
      rec->DB_comments = 0;
      rec->DB_status = 0;

      /*TEST TO SEE IF TRANSFORMATIONS APPLIED*/
      if (isnormal(this_meas->magnitude_tr) &&
	  this_meas->is_transformed) {
	rec->DB_transformed_mag = this_meas->magnitude_tr;
      } else {
	rec->DB_transformed_mag = NAN;
      }

      rec->DB_magerr = this_meas->magnitude_err;

      sprintf(remarks0, "%sMAGINS=%.3lf|%sERR=%.3lf|CREFMAG=%.3lf",
	      AAVSO_Color_Letter,
	      this_meas->instrumental_mag,
	      AAVSO_Color_Letter,
	      this_meas->magnitude_err,
	      comp_hgsc->multicolor_data.Get(pc));
      
      const char *color_name = 0;
      const char *transform_name;
      double color_value;
      double transform_value;
      
      double V_R, B_V, R_I, V_I;
      double B, V, R, I;
      B = GetBestMag(&(*s_it)->measurements[i_B]);
      V = GetBestMag(&(*s_it)->measurements[i_V]);
      R = GetBestMag(&(*s_it)->measurements[i_R]);
      I = GetBestMag(&(*s_it)->measurements[i_I]);

      V_R = V - R;
      B_V = B - V;
      R_I = R - I;
      V_I = V - I;
      
      rec->DB_colorvalue = NAN;
      if (c == i_V) {
	if(Colors::is_valid(V_R)) {
	  strcpy(rec->DB_colorname, "V_R");
	  color_name = "v-r";
	  transform_name = "Tv_vr";
	  color_value = V_R;
	} else if(Colors::is_valid(B_V)) {
	  strcpy(rec->DB_colorname, "B_V");
	  color_name = "b-v";
	  transform_name = "Tv_bv";
	  color_value = B_V;
	} else if(Colors::is_valid(V_I)) {
	  strcpy(rec->DB_colorname, "V_I");
	  color_name = "v-i";
	  transform_name = "Tv_vi";
	  color_value = V_I;
	}
      } else if (c == i_R) {
	if(Colors::is_valid(R_I)) {
	  strcpy(rec->DB_colorname, "R_I");
	  color_name = "r-i";
	  transform_name = "Tr_ri";
	  color_value = R_I;
	}
      } else if (c == i_I) {
	if(Colors::is_valid(R_I)) {
	  strcpy(rec->DB_colorname, "R_I");
	  color_name = "r-i";
	  transform_name = "Ti_ri";
	  color_value = R_I;
	} else if(Colors::is_valid(V_I)) {
	  strcpy(rec->DB_colorname, "V_I");
	  color_name = "v-i";
	  transform_name = "Ti_vi";
	  color_value = V_I;
	}
      } else if (c == i_B) {
	if(Colors::is_valid(B_V)) {
	  strcpy(rec->DB_colorname, "B_V");
	  color_name = "b-v";
	  transform_name = "Tb_bv";
	  color_value = B_V;
	}
      }
      if (color_name) {
	rec->DB_colorvalue = color_value;
      }
      if (this_meas->is_transformed) {
	transform_value = tr.Coefficient(transform_name);
	sprintf(remarks1, "%s=%.3lf|%s=%.3lf",
		color_name, color_value,
		transform_name, transform_value);
      } else {
	remarks1[0] = 0;
      }
      if (check_err_rms[c] >= 0.0) {
	sprintf(remarks2, "CHKERRRMS=%.3lf|NUMCHKSTARS=%d|NUMCOMPSTARS=%d",
		check_err_rms[c], num_check[c], 1);
      } else {
	sprintf(remarks2, "NUMCHKSTARS=%d|NUMCOMPSTARS=%d",
		num_check[c], 1);
      }
      if (remarks1[0]) {
	strcat(remarks0, "|");
	strcat(remarks0, remarks1);
      }
      strcat(remarks0, "|");
      strcat(remarks0, remarks2);
      rec->DB_remarks = strdup(remarks0);
      
      // rec->add_double("MAG_ERR", this_meas->/*ERR????*/);
      rl->push_back(rec);
    }
  } // end loop over all stars

  db->AddRecords(starname, rl);
  // DeepDelete(rl);

  BVRI_DB_ERRORS *rec = new BVRI_DB_ERRORS;

  for (int i=0; i<NUM_FILTERS; i++) {
    int filter_letter = AAVSO_FilterName(index_to_filter(i))[0];
    double *target;
      
    switch(filter_letter) {
    case 'B':
      target = &rec->DB_check_err_B;
      break;
	
    case 'V':
      target = &rec->DB_check_err_V;
      break;

    case 'R':
      target = &rec->DB_check_err_R;
      break;

    case 'I':
      target = &rec->DB_check_err_I;
      break;

    default:
      fprintf(stderr, "invalid filter letter??? ---> '%c'\n", filter_letter);
    }
    if (num_check[i] > 0) {
      *target = check_err_rms[i];
    } else {
      *target = 0.0;
    }
  }

  db->AddErrors(starname, rec);
	
  fprintf(stderr, "DBASE now has %d records in it.\n",
	  db->NumRecords());
  db->Close();
}
    
