/*  analyze.cc -- Takes photometry and assembles into photometry report
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
#include <string.h>		// for strcat()
#include <stdio.h>
#include <unistd.h> 		// for getopt()
#include <stdlib.h>		// for atof()
#include <ctype.h>		// toupper() in sloppystrcmp()
#include <fitsio.h>
#include <Image.h>
#include <nlls_general.h>
#include <named_stars.h>
#include <HGSC.h>
#include <libgen.h>
#include <strategy.h>
#include "correlate.h"
#include "aperture_phot.h"
#include <report_file.h>
#include <gendefs.h>

//#define ENSEMBLE         // uncomment this line to enable comparison
                           // ensemble processing. If you do, you need
                           // to figure out how to perform multi-color
                           //ensembles! 

void usage(void) {
  fprintf(stderr,
	  "usage: analyze [-e] -n starname -o starname.phot [-s flat] [-d dark] images\n");
  exit(-2);
}

const char *AAVSO_FilterName(Filter &f) {
  const char *local_filter_name = f.NameOf();
  if (strcmp(local_filter_name, "Vc") == 0) return "V";
  if (strcmp(local_filter_name, "Rc") == 0) return "R";
  if (strcmp(local_filter_name, "Ic") == 0) return "I";
  if (strcmp(local_filter_name, "Bc") == 0) return "B";
  fprintf(stderr, "AAVSO_FilterName: unrecognized filter: %s\n",
	  local_filter_name);
  return "X";
}

class AnalysisImage {
public:
  const char *image_filename;
  IStarList  *image_starlist;
  ImageInfo  *image_info;
  int        image_index;
  int        zero_point_adjusted;
  double     zero_point;	// instrumental magnitude minus
				// zero_point gives true magnitude
  double     zero_point_sigma;
};

class EachStar {
public:
  HGSC                    *hgsc_star;
  IStarList::IStarOneStar *image_star;
  AnalysisImage           *host_image;
  int                     processed;
  EachStar                *next_star;
};

struct ResultData {
  char   A_Unique_ID[16];
  char   filter_name[8];
  HGSC   *hgsc_star;
  JULIAN jd_exposure_midpoint;
  double magnitude;
  double stddev;
  int    stddev_valid;
  int    num_exp;
  int    is_comp;
  int    is_check;
  int    is_reference;

  ResultData *next_result;
};
  
EachStar *AnalysisHead = 0;
ResultData *ResultHead = 0;

void
AddReportLine(FILE *fp,
	      Strategy *strategy,
	      ResultData *comp,
	      ResultData *check);

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

int
main(int argc, char **argv) {
  int ch;			// option character
  FILE *fp_out = 0;
  char *starname = 0;
  char *darkfilename = 0;
  char *flatfilename = 0;
  int  use_existing_photometry = 0;

  // Command line options:
  // -n star_name       Name of region around which image was taken
  // -d dark.fits
  // -s flat.fits
  // -o output_filename
  // -e                 Use existing photometry already in image files

  while((ch = getopt(argc, argv, "d:s:n:o:e")) != -1) {
    switch(ch) {
    case 'n':			// name of star
      starname = optarg;
      break;

    case 'e':
      use_existing_photometry = 1;
      break;

    case 'd':
      darkfilename = optarg;
      break;

    case 's':
      flatfilename = optarg;
      break;

    case 'o':
      fp_out = fopen(optarg, "w");
      if(!fp_out) {
	fprintf(stderr, "analyze: cannot open output file %s\n", optarg);
	exit(-2);
      }
      break;

    case '?':
    default:
      usage();
    }
  }

  if(fp_out == 0 || starname == 0) {
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
  Strategy *strategy = new Strategy(starname, nullptr);

  const char *general_remarks = strategy->remarks();
  if(general_remarks) {
    fprintf(fp_out, "%s", general_remarks);
    fprintf(fp_out, "################################################\n");
  }

  HGSCList Catalog(HGSC_fp);
  // argc now contains a count of the total number of images being analyzed
  AnalysisImage ImageArray[argc];

  Filter filter_used("Invalid");
  Filter invalid_filter("Invalid");
  PhotometryColor color(PHOT_NONE); // will be set in tandem with filter_used
  int image_count;
  for(image_count = 0; image_count < argc; image_count++) {
    const char *this_image_name = argv[image_count];
    char orig_image_buffer[128];
    strcpy(orig_image_buffer, this_image_name);
    char *orig_image_name = basename(orig_image_buffer);

    fprintf(stderr, "Reading %s\n", this_image_name);
    ImageArray[image_count].image_filename = this_image_name;
    ImageArray[image_count].image_index = image_count;
    Image *orig_image = new Image(this_image_name);
    ImageArray[image_count].image_info = orig_image->GetImageInfo();
    Filter this_image_filter = ImageArray[image_count].image_info->GetFilter();
      
    // all images must use the same filter
    if (this_image_filter != invalid_filter) {
      if (filter_used != invalid_filter &&
	  filter_used != this_image_filter) {
	fprintf(fp_out, "Error: multiple filters encountered: %s and %s\n",
		filter_used.NameOf(), this_image_filter.NameOf());
      } else {
	filter_used = this_image_filter;
	color = FilterToColor(filter_used);
      }
    }

    if(!use_existing_photometry) {
      if(darkfilename || flatfilename) {
	fprintf(stderr, "Handling image processing.\n");
	IStarList orig_list(this_image_name);

	if(darkfilename) {
	  Image dark_image(darkfilename);
	  orig_image->subtractKeepPositive(&dark_image);
	}
	if(flatfilename) {
	  Image flat_image(flatfilename);
	  orig_image->scale(&flat_image);
	}
	orig_image->clip_low(0.0);
	this_image_name = "/tmp/imageq.fits";
	unlink(this_image_name); // in case already exists
	orig_image->WriteFITSFloat(this_image_name, false); // do not compress
	orig_list.SaveIntoFITSFile(this_image_name, 1); // 1=rewrite_okay
      }

      char photometry_command[132];
      sprintf(photometry_command, COMMAND_DIR "/photometry -i %s\n",
	      this_image_name);
      system(photometry_command);
    }

    IStarList *List = new IStarList(this_image_name);
    ImageArray[image_count].image_starlist = List;

    int i;
    double diff_sum = 0.0;
    double diff_sumsq = 0.0;
    int comp_count = 0;

    for(i=0; i < List->NumStars; i++) {
      IStarList::IStarOneStar *this_star = List->FindByIndex(i);
      if((this_star->validity_flags & PHOTOMETRY_VALID) &&
	 (this_star->validity_flags & CORRELATED)) {
	EachStar *analysis_star = new EachStar;
	analysis_star->next_star = AnalysisHead;
	AnalysisHead = analysis_star;

	analysis_star->hgsc_star  = Catalog.FindByLabel(this_star->StarName);
	analysis_star->image_star = this_star;
	analysis_star->host_image = &(ImageArray[image_count]);
	analysis_star->processed  = 0;

	if(analysis_star->hgsc_star == 0) {
	  fprintf(stderr,
		  "analyze: logic error: correlated star not in HGSC list\n");
	  break;
	}

	if(analysis_star->hgsc_star->is_comp) {
#ifdef ENSEMBLE
	  if(analysis_star->hgsc_star->photometry_ensemble_valid) {
	    double error = 
	      (this_star->photometry -
	       analysis_star->hgsc_star->photometry_ensemble);
	    diff_sum += error;
	    diff_sumsq += (error*error);
	    comp_count++;
	  } else if(analysis_star->hgsc_star->photometry_valid) {
	    double error = 
	      (this_star->photometry - analysis_star->hgsc_star->photometry);
	    diff_sum += error;
	    diff_sumsq += (error*error);
	    comp_count++;
	  }
#else // no ensemble support
	  if (analysis_star->hgsc_star->multicolor_data.IsAvailable(color)) {
	    double ref_magnitude =
	      analysis_star->hgsc_star->multicolor_data.Get(color);
	    double error = this_star->photometry - ref_magnitude;
	    diff_sum += error;
	    diff_sumsq += (error*error);
	    comp_count++;
	  } else {
	    fprintf(stderr, "Comp star has no photometry for color %s\n",
		    filter_used.NameOf());
	  }
#endif	    
	}
      }
    } // end loop over all stars in image
    
    if(comp_count < 1) {
      fprintf(stderr, "Image %s has no observed comp stars\n",
	      this_image_name);
      fprintf(fp_out, "# Image %s has no observed comp stars\n",
	      this_image_name);
      ImageArray[image_count].zero_point_adjusted = 0;
    } else {
      const double avg = (diff_sum/comp_count);
      ImageArray[image_count].zero_point = avg;
      if(comp_count == 1) {
	ImageArray[image_count].zero_point_sigma = 0.0;
      } else {
	ImageArray[image_count].zero_point_sigma =
	  sqrt((diff_sumsq - comp_count*avg*avg)/(comp_count-1));
      }
      ImageArray[image_count].zero_point_adjusted = 1;
      // should use basename() to get rid of boring part of pathname
      fprintf(fp_out, "# %s zero_pt %.3f err %.3f Filter %s\n",
	      orig_image_name, avg, ImageArray[image_count].zero_point_sigma,
	      filter_used.NameOf());
    }
    //unlink("/tmp/imageq.fits");
  }

  fprintf(stderr, "Analyzing using data for filter %s\n", filter_used.NameOf());

  double ref_err = 0.0;
  double ref_err_sq = 0.0;
  int    ref_err_cnt = 0;

  // for each correlated star, sum data for all observations of that star
  EachStar *ref_star;
  for(ref_star = AnalysisHead; ref_star; ref_star = ref_star->next_star) {
    if(ref_star->processed) continue;

    double sum_phot = 0.0;
    double sum_err = 0.0;
    double sum_phot_sq = 0.0;
    int    num_err = 0;
    int    num_phot = 0;
    double error_sum = 0.0;
    int    error_count = 0;
    double sum_jd = 0.0;	// sum of exposure midpoint times

    EachStar *one_star;
    for(one_star = ref_star; one_star; one_star = one_star->next_star) {
      if(one_star->hgsc_star != ref_star->hgsc_star) continue;

      // matches!!
      one_star->processed = 1;

      if(one_star->host_image->zero_point_adjusted) {
	double this_phot = one_star->image_star->photometry -
	  one_star->host_image->zero_point;
	if(one_star->hgsc_star->is_comp || one_star->hgsc_star->is_check) {
#ifdef ENSEMBLE
	  double t_ref = (one_star->hgsc_star->photometry_ensemble_valid) ?
	    one_star->hgsc_star->photometry_ensemble :
	    one_star->hgsc_star->photometry;
#else
	  if (one_star->hgsc_star->multicolor_data.IsAvailable(color)) {
	    double t_ref = 
	      one_star->hgsc_star->multicolor_data.Get(color);
	    double err = this_phot - t_ref;
	    num_err++;
	    sum_err += err;
	  }
#endif
	}
	sum_jd +=
	  one_star->host_image->image_info->GetExposureMidpoint().day();
	sum_phot += this_phot;
	sum_phot_sq += (this_phot*this_phot);
	num_phot++;

	if(one_star->image_star->validity_flags & ERROR_VALID) {
	  error_sum += (one_star->image_star->magnitude_error);
	  error_count++;
	}
	
      }
    }

    double measure = sum_phot/num_phot;
    double sigma = 0.0;
    if(num_phot > 1) {
      sigma = sqrt((sum_phot_sq - num_phot*measure*measure)/(num_phot - 1));
    }
    
    if((ref_star->hgsc_star->is_comp || ref_star->hgsc_star->is_check) &&
       ref_star->hgsc_star->multicolor_data.IsAvailable(color)) {
      double t_ref = ref_star->hgsc_star->multicolor_data.Get(color);

      ref_err_cnt++;
      double r_err = t_ref - measure;
      ref_err += r_err;
      ref_err_sq += (r_err*r_err);

      fprintf(fp_out, "%-20s %s%c %8.3f %8.3f %8.3f ",
	      ref_star->hgsc_star->label,
	      (ref_star->hgsc_star->is_comp ? "COMP" : "CHCK"),
	      (ref_star->hgsc_star->is_reference ? '*' : ' '),
	      t_ref,
	      measure,
	      r_err);
    } else {
      fprintf(fp_out, "%-20s                %8.3f          ",
	      ref_star->hgsc_star->label,
	      measure);
    }
    fprintf(fp_out, "%9.3f %3d %.3f\n",
	    sigma,
	    num_phot,
	    (error_count ? error_sum/error_count : 0.0));
    ResultData *this_result = new ResultData;
    if(!this_result) {
      fprintf(stderr, "analyze: allocate for ResultData failed.\n");
      exit(-1);
    }

    ////////////////////////////////////////////////////////////////
    //        Set up the ResultData structure
    ////////////////////////////////////////////////////////////////
    this_result->next_result = ResultHead;
    ResultHead = this_result;

    this_result->hgsc_star = ref_star->hgsc_star;
    this_result->magnitude = measure;
    if(error_count) {
      this_result->stddev = error_sum/error_count;
      if(sigma > this_result->stddev) this_result->stddev = sigma;
      this_result->stddev_valid = 1;
    } else {
      this_result->stddev_valid = 0;
    }
    this_result->num_exp = num_phot;
    this_result->is_comp = ref_star->hgsc_star->is_comp;
    this_result->is_check = ref_star->hgsc_star->is_check;
    strcpy(this_result->filter_name, AAVSO_FilterName(filter_used));
    this_result->is_reference = ref_star->hgsc_star->is_reference;
    if(ref_star->hgsc_star->A_unique_ID) {
      strcpy(this_result->A_Unique_ID, ref_star->hgsc_star->A_unique_ID);
    } else {
      this_result->A_Unique_ID[0] = 0;
    }
    this_result->jd_exposure_midpoint = sum_jd/num_phot;
    //////// end of ResultData setup //////////////////////////////
  }
  if(ref_err_cnt >= 2) {
    double measure = ref_err/ref_err_cnt;
    double sigma = sqrt((ref_err_sq - ref_err_cnt*measure*measure)/
			(ref_err_cnt - 1));
    fprintf(fp_out, "Total err = %.3f\n", sigma);
  }

  {
    ResultData *comp_star = 0;
    ResultData *comp_ref_star = 0;
    int num_ref_comp = 0;
    int num_comp = 0;
    ResultData *check_star = 0;
    ResultData *check_ref_star = 0;
    int num_ref_check = 0;
    int num_check = 0;

    ResultData *this_result;
    for(this_result = ResultHead;
	this_result;
	this_result = this_result->next_result) {
      if(this_result->is_reference) {
	if(this_result->is_comp) {
	  num_ref_comp++;
	  comp_ref_star = this_result;
	}
	if(this_result->is_check) {
	  num_ref_check++;
	  check_ref_star = this_result;
	}
      } else {
	if(this_result->is_comp) {
	  num_comp++;
	  comp_star = this_result;
	}
	if(this_result->is_check) {
	  num_check++;
	  check_star = this_result;
	}
      }
    }
    // final result will go into comp_star and check_star
    if(num_ref_comp == 1) {
      comp_star = comp_ref_star;
    } else if(num_ref_comp == 0 && num_comp == 1) {
      ; // nothing needed
    } else {
      comp_star = 0;
    }
      
    if(num_ref_check == 1) {
      check_star = check_ref_star;
    } else if(num_ref_check == 0 && num_check == 1) {
      ; // nothing needed
    } else {
      check_star = 0;
    }

    // now pick up all stars
    fprintf(fp_out, "\n\n");	// two blank lines in output file
    AddReportLine(fp_out, strategy, comp_star, check_star);
    if(strategy->ChildStrategies()) {
      int i = 0;
      for(i=0; i<strategy->ChildStrategies()->NumberStrategies(); i++) {
	Strategy *strat = strategy->ChildStrategies()->Get(i);
	AddReportLine(fp_out, strat, comp_star, check_star);
      }
    }
  }
  fclose(fp_out);
}

// sloppystrcmp() behaves somewhat like strcmp(), but it ignores the
// case of characters being compared.
static int sloppystrcmp( const char *s1,
			 const char *s2) {
  while(*s1 && *s2) {
    if(toupper(*s1) != toupper(*s2)) return 1;
    s1++;
    s2++;
  }
  return !(*s1 == 0 && *s2 == 0);
}

void
AddReportLine(FILE *fp,
	      Strategy *strategy,
	      ResultData *comp,
	      ResultData *check) {
  ReportFileLine rfl;

  ResultData *rd;
  for(rd = ResultHead; rd; rd = rd->next_result) {
    if(sloppystrcmp(rd->hgsc_star->label,
		    strategy->object()) == 0) {
      break;
    }
  }
  if(rd == 0) {
    fprintf(stderr, "Warning: analyze: no ResultData for %s\n",
	    strategy->object());
    return;
  }
  
  strcpy(rfl.report_name, rd->hgsc_star->report_ID ?
	   rd->hgsc_star->report_ID :
	   (rd->hgsc_star->A_unique_ID ? rd->hgsc_star->A_unique_ID :
	    aavso_format(rd->hgsc_star->label)));
  rfl.jd = rd->jd_exposure_midpoint.day();
  rfl.magnitude = rd->magnitude;
  rfl.error_estimate = rd->stddev;
  strcpy(rfl.filter, comp->filter_name);
  rfl.transformed = 0;
  rfl.mtype = MTYPE_ABS;
  if(comp) {
    strcpy(rfl.comp_name, comp->hgsc_star->report_ID ?
	   comp->hgsc_star->report_ID :
	   (comp->hgsc_star->A_unique_ID ? comp->hgsc_star->A_unique_ID :
	    aavso_format(comp->hgsc_star->label)));
    rfl.comp_magnitude = comp->magnitude;
  } else {
    rfl.comp_name[0] = 0;
    rfl.comp_magnitude = -99.0;
  }
  if(check) {
    strcpy(rfl.check_name, check->hgsc_star->report_ID ?
	   check->hgsc_star->report_ID :
	   (check->hgsc_star->A_unique_ID ? check->hgsc_star->A_unique_ID :
	    aavso_format(check->hgsc_star->label)));
    rfl.check_magnitude = check->magnitude;
  } else {
    rfl.check_name[0] = 0;
    rfl.check_magnitude = -99.0;
  }

  rfl.airmass = -1.0;
  rfl.group = -1;
  strcpy(rfl.chart, strategy->ObjectChart());
  if(strategy->ReportNotes()) {
    strcpy(rfl.notes, strategy->ReportNotes());
  }

  // Now that the report_line structure has been populated, it's time
  // to write it to the output file
  {
    int status;
    char *string_line = rfl.ToString(&status, '|');
    fprintf(fp, "%s\n", string_line);
    free(string_line);
  }
}

