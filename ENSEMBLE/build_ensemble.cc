/*  build_ensemble.cc -- Program to create comparison star "ensemble"
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
#include <fitsio.h>
#include <Image.h>
#include <nlls_general.h>
#include <named_stars.h>
#include <HGSC.h>
#include <libgen.h>
#include <gsl/gsl_vector.h>
#include <gsl/gsl_matrix.h>
#include <gsl/gsl_permutation.h>
#include <gsl/gsl_linalg.h>
#include <gendefs.h>

class AnalysisImage {
public:
  char      *image_filename;
  IStarList *image_starlist;
  int       image_index;
  int       zero_point_adjusted;
  double    zero_point;		// instrumental magnitude minus
				// zero_point gives true magnitude
  double    zero_point_sigma;
};

class EachStar {
public:
  HGSC                    *hgsc_star;
  IStarList::IStarOneStar *image_star;
  AnalysisImage           *host_image;
  int                     processed;
  int                     ensemble_star;
  int                     ensemble_star_index;
  EachStar                *next_star;
};

int main(int argc, char **argv) {
  int ch;			// option character
  FILE *fp_out = 0;
  FILE *fp_ens_names = 0;
  char *starname = 0;

  // Command line options:
  // -n star_name       Name of region around which image was taken
  // -o output_filename

  while((ch = getopt(argc, argv, "d:s:n:o:")) != -1) {
    switch(ch) {
    case 'n':			// name of star
      starname = optarg;
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
      fprintf(stderr,
	      "usage: %s [-f] -n starname -i image_filename.fits [-d dark] images\n",
	      argv[0]);
      return 2;			// error return
    }
  }

  if(fp_out == 0 || starname == 0) {
    fprintf(stderr, "usage: %s -n starname -o output_file\n", argv[0]);
    return 2;			// error return
  }
  
  argc -= optind;
  argv += optind;

  char EnsembleNamesFilename[132];
  sprintf(EnsembleNamesFilename, CATALOG_DIR "/%s.ens_names",
	  starname);
  fp_ens_names = fopen(EnsembleNamesFilename, "r");
  if(!fp_ens_names) {
    fprintf(stderr, "Cannot open ensemble names file: %s\n",
	    EnsembleNamesFilename);
    exit(-2);
  }

  char HGSCfilename[132];
  sprintf(HGSCfilename, CATALOG_DIR "/%s", starname);
  FILE *HGSC_fp = fopen(HGSCfilename, "r");
  if(!HGSC_fp) {
    fprintf(stderr, "Cannot open catalog file for %s\n", starname);
    exit(-2);
  }

  HGSCList Catalog(HGSC_fp);
  // argc now contains a count of the total number of images being analyzed
  AnalysisImage ImageArray[argc];
  EachStar *AnalysisHead = 0;
  int image_count = 0;
  int total_star_count = 0;
  int image_index;
  for(image_index = 0; image_index < argc; image_index++) {
    char *this_image_name = argv[image_index];
    char orig_image_buffer[128];
    strcpy(orig_image_buffer, this_image_name);
    char *orig_image_name = basename(orig_image_buffer);

    fprintf(stderr, "Reading %s\n", this_image_name);

    ImageArray[image_count].image_filename = this_image_name;
    ImageArray[image_count].image_index = image_count;
    IStarList *List = new IStarList(this_image_name);
    ImageArray[image_count].image_starlist = List;
    total_star_count += List->NumStars;

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
	analysis_star->ensemble_star = 0;

	if(analysis_star->hgsc_star == 0) {
	  fprintf(stderr,
		  "analyze: logic error: correlated star (%s) not in HGSC list\n",
		  this_star->StarName);
	  break;
	}

	if(analysis_star->hgsc_star->photometry_valid &&
	   analysis_star->hgsc_star->is_comp) {
	  double error = 
	    (this_star->photometry - analysis_star->hgsc_star->photometry);
	  diff_sum += error;
	  diff_sumsq += (error*error);
	  comp_count++;
	}
      }
    }
    if(comp_count < 1) {
      fprintf(stderr, "Image %s has no observed comp stars\n",
	      this_image_name);
      fprintf(fp_out, "# Image %s has no observed comp stars\n",
	      this_image_name);
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
      fprintf(fp_out, "# %s zero_pt %.3f err %.3f\n",
	      orig_image_name, avg, ImageArray[image_count].zero_point_sigma);
      image_count++;
    }
    unlink("/tmp/imageq.fits");
  }

  // Now go set the "ensemble_star" flag in the EachStar structure for
  // all the stars that are mentioned in the ensemble definition file
  EachStar *Eptr[total_star_count];
  double ZeroPointReference = 0.0;
  int ZeroPointIndex = -1;
  int NumberEnsembleStars = 0;
  {
    char buffer[132];
    char one_star_name[132];
    while(fgets(buffer, sizeof(buffer), fp_ens_names)) {
      char *s = buffer;
      while(*s) {
	if(*s == '#') *s = 0;
	else s++;
      }

      // each line can have either one or two entries.  Somewhere,
      // exactly one line must have two entries. This defines the
      // overall zeropoint of the ensemble.
      double zero_reference;
      int name_count = sscanf(buffer, "%s %lf",
			      one_star_name, &zero_reference);
      if(name_count >= 1) {
	
	EachStar *a_star;
	int found = 0;
	for(a_star = AnalysisHead; a_star; a_star = a_star->next_star) {
	  if(strcmp(a_star->hgsc_star->label, one_star_name) == 0) {
	    if(found == 0) {
	      found = 1;
	      Eptr[NumberEnsembleStars] = a_star;
	      if(name_count == 2) {
		if(ZeroPointIndex != -1) {
		  fprintf(stderr,
			  "Error: ensemble names more than one reference.\n");
		  exit(-2);
		}
		ZeroPointIndex = NumberEnsembleStars;
		ZeroPointReference = zero_reference;
	      }
	    }
	    a_star->ensemble_star = 1;
	    a_star->ensemble_star_index = NumberEnsembleStars;
	  }
	}
	if(found == 0) {
	  fprintf(stderr, "build_ensemble: %s not in any image\n",
		  one_star_name);
	} else {
	  NumberEnsembleStars++;
	}
      }
    }
    fclose(fp_ens_names);
  }
  if(ZeroPointIndex == -1) {
    fprintf(stderr, "no reference found in ensemble name file\n");
    exit(-2);
  }

  // counts of interest:
  //    image_count
  //    NumberEnsembleStars

  // Each element of the Q array is either 0 or 1. Q(i,n) is 1 if star
  // "i" is found in image "n"
  // Each element of the EN array is a count of the number of images
  // each ensemble star is found in.
  // Each element of the ZN array is a count of the number of ensemble
  // stars found in that image
  int Q[NumberEnsembleStars][image_count];
  int EN[NumberEnsembleStars];
  int ZN[image_count];
  double Ey[NumberEnsembleStars];
  double Zy[image_count];

  // zero those two arrays and the matrix
  {
    int i, s;
    for(i=0; i<image_count; i++) {
      ZN[i] = 0;
      Zy[i] = 0.0;
      for(s=0; s<NumberEnsembleStars; s++) {
	Q[s][i] = 0;
      }
    }
    for(s=0; s<NumberEnsembleStars; s++) {
      EN[s] = 0;
      Ey[s] = 0.0;
    }
  }
  
  EachStar *ref_star;
  for(ref_star = AnalysisHead; ref_star; ref_star = ref_star->next_star) {
    if(ref_star->ensemble_star == 0) continue;

    // this is an ensemble star; need to know in which image it
    // appears and what its index is in the Eptr[] array.
    int star_index = ref_star->ensemble_star_index;
    int image_index = ref_star->host_image->image_index;

    Q[star_index][image_index] = 1;
    ZN[image_index]++;
    EN[star_index]++;
    double this_value = ref_star->image_star->photometry;
    Zy[image_index] += this_value;
    Ey[star_index] += this_value;
  }

  {
    int i, s;
    for(i=0; i<image_count; i++) {
      fprintf(stderr, "image %d: ZN[%d]=%d, Zy[%d]=%.3f, y(avg)=%.3f\n",
	      i, i, ZN[i], i, Zy[i], ZN[i] ? (Zy[i]/ZN[i]) : 0.0);
    }
    for(s=0; s<NumberEnsembleStars; s++) {
      fprintf(stderr, "star %d: EN[%d]=%d, Ey[%d]=%.3f, y(avg)=%.3f\n",
	      s, s, EN[s], s, Ey[s], EN[s] ? (Ey[s]/EN[s]) : 0.0);
    }
  }

  const int order = image_count + NumberEnsembleStars;
  gsl_matrix *matrix = gsl_matrix_calloc(order, order);
  gsl_vector *product = gsl_vector_calloc(order);
  gsl_permutation *permutation = gsl_permutation_alloc(order);

  if(!matrix) {
    fprintf(stderr, "ensemble: allocation of matrix failed\n");
  }
  if(!product) {
    fprintf(stderr, "ensemble: allocation of vector failed\n");
  }
  if(!permutation) {
    fprintf(stderr, "ensemble: permutation create failed\n");
  }

  if(matrix == 0 || product == 0 || permutation == 0) exit(-2);

  int n, i;
  for(i=0; i<NumberEnsembleStars; i++) {
    for(n=0; n<image_count; n++) {
      (*gsl_matrix_ptr(matrix, n, i)) = Q[i][n];
      (*gsl_matrix_ptr(matrix, image_count + i, NumberEnsembleStars +
		       n)) = Q[i][n];
    }
  }
  for(n=0; n<image_count; n++) {
    (*gsl_matrix_ptr(matrix, n, NumberEnsembleStars + n)) = ZN[n];
    (*gsl_vector_ptr(product, n)) = Zy[n];
  }
  for(i=0; i<NumberEnsembleStars; i++) {
    (*gsl_matrix_ptr(matrix, image_count + i, i)) = EN[i];
    (*gsl_vector_ptr(product, image_count + i)) = Ey[i];
  }

  int sig_num;
  if(gsl_linalg_LU_decomp(matrix, permutation, &sig_num)) {
    fprintf(stderr, "ensemble: gsl_linalg_LU_decomp() failed.\n");
    exit(-1);
  }

  if(gsl_linalg_LU_svx(matrix, permutation, product)) {
    fprintf(stderr, "ensemble: gls_linalg_LU_solve() failed.\n");
    exit(-1);
  }

  gsl_matrix_free(matrix);
  gsl_permutation_free(permutation);

  double E[NumberEnsembleStars];
  double Z[image_count];
  double mag_err[NumberEnsembleStars];
  double mag_err_sq[NumberEnsembleStars];
  int mag_err_cnt[NumberEnsembleStars];

  for(i=0; i<NumberEnsembleStars; i++) {
    E[i] = gsl_vector_get(product, i);
    mag_err[i] = mag_err_sq[i] = 0.0;
    mag_err_cnt[i] = 0;
  }

  double zero_offset = ZeroPointReference - E[ZeroPointIndex];
  for(i=0; i<NumberEnsembleStars; i++) {
    E[i] += zero_offset;
  }

  for(i=0; i<image_count; i++) {
    Z[i] = gsl_vector_get(product, i+NumberEnsembleStars);
  }

  for(ref_star = AnalysisHead; ref_star; ref_star = ref_star->next_star) {
    if(ref_star->ensemble_star == 0) continue;

    // this is an ensemble star; need to know in which image it
    // appears and what its index is in the Eptr[] array.
    int star_index = ref_star->ensemble_star_index;
    int image_index = ref_star->host_image->image_index;

    double this_value = ref_star->image_star->photometry;

    double err = this_value + zero_offset - Z[image_index] - E[star_index];
    fprintf(stderr, "star %d image %d: meas = %.3f ref = %.3f, err=%.3f\n",
	    star_index, image_index,
	    this_value+zero_offset-Z[image_index], E[star_index], err);
    mag_err[star_index] += err;
    mag_err_sq[star_index] += (err*err);
    mag_err_cnt[star_index]++;
  }

  for(i=0; i<NumberEnsembleStars; i++) {
    double sigma = 0.0;
    if(mag_err_cnt[i] > 1) {
      double measure = mag_err[i]/mag_err_cnt[i];
      sigma = sqrt((mag_err_sq[i] - mag_err[i]*measure*measure)/
		   (mag_err_cnt[i] - 1));
    }
    fprintf(stderr, "E[%d] (%s) = %.3f (std=%.3f)\n", i,
	    Eptr[i]->hgsc_star->label, E[i], sigma);
  }
    
  for(i=0; i<image_count; i++) {
    fprintf(stderr, "Z[%d] (%s) = %.3f\n", i,
	    ImageArray[i].image_filename, Z[i]);
  }

  fclose(fp_out);
}
