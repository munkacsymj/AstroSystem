/*  matcher.cc -- Match stars in an image with a catalog
 *
 *  Copyright (C) 2022 Mark J. Munkacsy
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

#include "matcher3.h"
#include <dec_ra.h>
#include <math.h>
#include <algorithm>
#include <gsl/gsl_matrix.h>
#include <gsl/gsl_blas.h>
#include <gsl/gsl_linalg.h>


//****************************************************************
//        The LOGGER
//****************************************************************

struct Candidate {
  const CAT_DATA *cat;
  double residual_arcsec;
};

//#define LOGGER
//#define PRINTSTARLISTS

#ifdef LOGGER
static FILE *log_fp = nullptr;
struct LoggerRow {
  const IMG_DATA *this_img;
  const CAT_DATA *match;
  double match_err;
  std::list<Candidate> candidates;
};

std::list<LoggerRow *> LogCurrentPair;

static void ClearCurrentPair(void) {
  for (auto p : LogCurrentPair) {
    p->candidates.clear();
    delete p;
  }
  LogCurrentPair.clear();
}

static void LOGMatcherStart(void) {
  if (log_fp == nullptr) {
    log_fp = fopen("/tmp/matcher.csv", "w");
  }
}

static void LOGMatcherNewIMG(IMG_DATA *img) {
  if (log_fp == nullptr) LOGMatcherStart();
  LoggerRow *lr = new LoggerRow;
  lr->this_img = img;
  lr->match = nullptr;
  lr->match_err = -1.0;
  LogCurrentPair.push_back(lr);
}

static void LOGMatcherCandidate(CAT_DATA *cat,
				double residual2) {
  LogCurrentPair.back()->candidates.push_back(Candidate ({cat, sqrt(residual2)*3600.0*180.0/M_PI}));
}

static void LOGMatcherIMGDone(CAT_DATA *match) {
  LoggerRow *row = LogCurrentPair.back();
  std::list<Candidate> &candidates = row->candidates;
  for (auto x : candidates) {
    if (x.cat == match) {
      row->match_err = x.residual_arcsec;
      break;
    }
  }
  row->match = match;
}

static void PrintPair(void) {
  for (auto row : LogCurrentPair) {
    const IMG_DATA *current_img = row->this_img;
    fprintf(log_fp, "%d, %.2lf, %.2lf, %s, ",
	    current_img->index,
	    current_img->star.nlls_x,
	    current_img->star.nlls_y,
	    (row->match ? row->match->hgsc_star.label : "<nil>"));
    if (row->match) {
      fprintf(log_fp, "%.1lf, ", row->match_err);
    } else {
      fprintf(log_fp, ", ");
    }
    fprintf(log_fp, "\"");
    for (auto x : row->candidates) {
      fprintf(log_fp, "%s: %.1lf\n",
	      x.cat->hgsc_star.label, x.residual_arcsec);
    }
    fprintf(log_fp, "\", %s\n", current_img->star.StarName);
  }
  fprintf(log_fp, "\n\n\n");
  
  fflush(log_fp);
}

static void PairDone(bool print_pair) {
  if (print_pair) {
    PrintPair();
  }
  ClearCurrentPair();
}

#endif
  
//****************************************************************
//        The GRID
//****************************************************************

struct ResidualPair {
  double residual;
  IMG_DATA *img;
};

#if 0
double static ARCSEC(double rad) {
  return rad*180.0*3600.0/M_PI;
}
#endif

bool comp_residuals(ResidualPair &p1, ResidualPair&p2) {
  return p1.residual < p2.residual;
}

void ComputeStatistics(std::vector<IMG_DATA *> &stars,
		       ResidualStatistics &stats) {
  std::vector<ResidualPair> residuals;
  double residual_sum = 0.0;
  for (auto s : stars) {
    if (s->matches.size() > 0) {
      const double residual = sqrt(s->residual2);
      residuals.push_back(ResidualPair ({residual, s}));
      residual_sum += residual;
    }
  }
  if (residuals.size() < 1) {
    stats.average = 0.0;
    stats.median = 0.0;
    stats.stddev = 0.0;
    return;
  }
  stats.average = residual_sum/residuals.size();
  std::sort(residuals.begin(), residuals.end(), comp_residuals);
  if (residuals.size() >= 2) {
    stats.median = residuals[residuals.size()/2].residual;
  } else {
    // only one residual
    stats.median = residuals[0].residual;
  }
  double dev_sq = 0.0;
  for (auto r : residuals) {
    const double offset = r.residual - stats.average;
    dev_sq += (offset*offset);
  }
  stats.stddev = sqrt(dev_sq/residuals.size());

#if 0
  fprintf(stderr, "Largest residuals:\n");
  for (int i=(int)residuals.size()-5; i<(int)residuals.size(); i++) {
    if (i < 0) continue;
    fprintf(stderr, "    %.2lf: %s/%s\n",
	    ARCSEC(residuals[i].residual), residuals[i].img->star.StarName,
	    residuals[i].img->matches.front()->hgsc_star.label);
  }
#endif
}
    
Grid::Grid(Context &context,
	   std::vector<CAT_DATA *> all_cat,
	   double max_tolerance) { // in arcradians
  // max_tolerance should be equal to 1.5 times the side of a cell

  // Find max/min of HGSCList. All values in radians. No cos(dec) adjustment.
  double max_dec = -99.9;
  double min_dec = 99.9;
  double max_ra = -99.9;
  double min_ra = 99.9;

  for (auto star : all_cat) {
    const DEC_RA &loc = star->hgsc_star.location;
    if (loc.dec() > max_dec) max_dec = loc.dec();
    if (loc.dec() < min_dec) min_dec = loc.dec();
    if (loc.ra_radians() > max_ra) max_ra = loc.ra_radians();
    if (loc.ra_radians() < min_ra) min_ra = loc.ra_radians();
  }
  // Fix wraparound
  wraparound = context.wraparound;
  //fprintf(stderr, "First pass. min_ra = %lf, max_ra = %lf, wraparound = %d\n",
  //	  min_ra, max_ra, wraparound);
  if (wraparound) {
    // must re-scan RA
    max_ra = -99.9;
    min_ra = 99.9;
    for (auto star : all_cat) {
      const double ra = Normalize(star->hgsc_star.location.ra_radians());
      if (ra > max_ra) max_ra = ra;
      if (ra < min_ra) min_ra = ra;
    }
  }
      
  dec_ref = min_dec;
  ra_ref = min_ra;
  dec_incr = max_tolerance;
  cos_dec = cos((max_dec+min_dec)/2.0);
  ra_incr = dec_incr/cos_dec;

  num_dec_cells = 1+(max_dec-min_dec)/dec_incr;
  num_ra_cells = 1+(max_ra-min_ra)/ra_incr;
  num_cells_total = num_dec_cells * num_ra_cells;

  //fprintf(stderr, "min_ra = %lf, max_ra = %lf\n",
  //	  min_ra, max_ra);
  //fprintf(stderr, "wraparound = %d\n", wraparound);
  //fprintf(stderr, "num_dec_cells = %d, num_ra_cells = %d\n",
  //	  num_dec_cells, num_ra_cells);

  grid = new Cell* [num_cells_total];
  for (int i=0; i<num_cells_total; i++) {
    grid[i] = new Cell;
  }

  // Populate the grid
  for (auto star : all_cat) {
    int x = LocToGridNum(star->hgsc_star.location);
    if (x >= 0) {
      grid[x]->push_back(star);
    } else {
      fprintf(stderr, "ERROR: Cat star %s falls off grid.\n",
	      star->hgsc_star.label);
    }
  }
  int empty_cells = 0;
  unsigned int max_cell_size = 0;
  for (int i=0; i<num_cells_total; i++) {
    if (grid[i]->size() == 0) empty_cells++;
    if (grid[i]->size() > max_cell_size) max_cell_size = grid[i]->size();
  }
  //fprintf(stderr, "Grid has %d cells. %d are empty. Biggest has %u stars.\n",
  //	  num_cells_total, empty_cells, max_cell_size);
  
}

Grid::~Grid(void) {
  delete [] grid;
}

  // returns -1 if off-grid
int
Grid::LocToGridNum(const DEC_RA &loc) const {
  const DEC_RA loc_n = Normalize(loc);
  const int dec_i = (loc_n.dec() - dec_ref)/dec_incr;
  const int ra_i = (loc_n.ra_radians() - ra_ref)/ra_incr;
  const int x = matrix_index(dec_i, ra_i);

  if (x < 0 or x >= num_cells_total) {
    fprintf(stderr, "Bad LocToGridNum: dec_i = %d, ra_i = %d\n",
	    dec_i, ra_i);
    return -1;
  }
  return x;
}

double Grid::Distance2(const DEC_RA &t1, const DEC_RA &t2) const {
  double del_dec = t1.dec() - t2.dec();
  double r1 = Normalize(t1.ra_radians());
  double r2 = Normalize(t2.ra_radians());
  double del_ra = r1 - r2;
  return del_dec*del_dec+del_ra*del_ra*cos_dec*cos_dec;
}

CAT_DATA *
Grid::FindNearest(const DEC_RA &loc,
		  double tolerance,
		  double &residual,
		  int max_index) const {
  const DEC_RA loc_n = Normalize(loc);
  CAT_DATA *closest_star = nullptr;
  double closest_distance2 = 99.9; // square of closest distance

  const int dec_i = (loc_n.dec() - dec_ref)/dec_incr;
  const int ra_i = (loc_n.ra_radians() - ra_ref)/ra_incr;

  for (int d=dec_i-1; d < dec_i+2; d++) {
    if (d < 0 or d >= num_dec_cells) continue;
    for (int r=ra_i-1; r < ra_i+2; r++) {
      if (r < 0 or r >= num_ra_cells) continue;
      
      const int x = matrix_index(d, r);

      for(auto star : *grid[x]) {
	if (star->index > max_index) continue;
	DEC_RA star_loc = Normalize(star->hgsc_star.location);
	double d2 = Distance2(loc_n, star_loc);
#ifdef LOGGER
	LOGMatcherCandidate(star, d2);
#endif
	if (d2 < closest_distance2) {
	  closest_distance2 = d2;
	  closest_star = star;
	}
      }
    }
  }
  if (tolerance*tolerance >= closest_distance2) {
    residual = closest_distance2;
    return closest_star;
  }
  return nullptr;
}

Grid *InitializeGrid(Context *context, std::vector<CAT_DATA *> &cat_list, double coarse_tolerance) {
  return new Grid(*context, cat_list, coarse_tolerance);
}

//****************************************************************
//        Matcher
//****************************************************************

// Returns the number of successful matches
int
Matcher(Context *context,
	Grid *grid,		// must be consistent with cat_list
	const WCS &wcs,
	std::vector<CAT_DATA *> &cat_list,
	std::vector<IMG_DATA *> &image_list,
	int num_img_to_use,
	double tolerance,	// arcseconds
	bool do_fixup) {
  // The most time-critical invocations have a long cat_list and a
  // short image_list, so it's okay to take a little while playing with
  // the cat_list. We will use Dec/RA coordinates for the matching.

  if (num_img_to_use > (long) image_list.size()) {
    num_img_to_use = (int) image_list.size();
  }

  const DEC_RA center_loc = grid->Normalize(wcs.Center());
  const double vert_span = 1.02*(context->IMAGE_HEIGHT_RAD/2.0);
  const double horiz_span = 1.02*(context->IMAGE_WIDTH_RAD/2.0);

  int num_cat_to_use = 0;
  int num_good_cat = 0;
  const int target_num_to_use = num_img_to_use*5/4;
  for (unsigned int i=0; i<cat_list.size(); i++) {
    const CAT_DATA *cat = cat_list[i];
    const double delta_dec = fabs(cat->hgsc_star.location.dec() - center_loc.dec());
    const double delta_ra = fabs(cat->hgsc_star.location.ra_radians() - center_loc.ra_radians());
    if ( delta_dec < vert_span and
	 delta_ra < horiz_span) {
      num_good_cat++;
      if (num_good_cat > target_num_to_use) {
	num_cat_to_use = i;
	break;
      }
    }
  }
  if (num_cat_to_use == 0) {
    num_cat_to_use = (int) cat_list.size();
  }

#ifdef PRINTSTARLISTS
  fprintf(stderr, "Using %d image stars and %d catalog stars.\n",
  	  num_img_to_use, num_cat_to_use);
#endif
#ifdef LOGGER
  LOGMatcherStart();
#endif
#ifdef PRINTSTARLISTS
  fprintf(stderr, "image_list follows:\n");
#endif
  for (unsigned int i=0; i<image_list.size(); i++) {
    IMG_DATA *x = image_list[i];
    if ((int) i < num_img_to_use) {
      x->trial_loc = grid->Normalize(wcs.Transform(x->star.nlls_x, x->star.nlls_y));
    }
    x->matches.clear();
#ifdef PRINTSTARLISTS
    fprintf(stderr, "    %s at (%.1lf, %.1lf) --> (%s, %s)\n",
	    x->star.StarName,
	    x->star.nlls_x, x->star.nlls_y,
	    x->trial_loc.string_fulldec_of(),
	    x->trial_loc.string_longra_of());
#endif
  }

#ifdef PRINTSTARLISTS
  fprintf(stderr, "\n\ncat_list follows:\n");
  static bool ever_printed = false;
#endif
  for (auto x : cat_list) {
    x->matches.clear();
#ifdef PRINTSTARLISTS
    if (ever_printed == false) {
      fprintf(stderr, "    %s at (%s, %s)\n",
	      x->hgsc_star.label,
	      x->hgsc_star.location.string_fulldec_of(),
	      x->hgsc_star.location.string_longra_of());
    }
#endif
  }
  //ever_printed = true;

  int num_matches = 0;
  for(int i=0; i<num_img_to_use; i++) {
    IMG_DATA *istar = image_list[i];
 #ifdef LOGGER
    LOGMatcherNewIMG(istar);
#endif
    double residual_sq;
    CAT_DATA *match = grid->FindNearest(istar->trial_loc, tolerance, residual_sq, num_cat_to_use);
    if (match) {
      istar->matches.push_back(match);
      match->matches.push_back(istar);
      match->residual2 = residual_sq;
      istar->residual2 = residual_sq;
      num_matches++;
    }
#ifdef LOGGER
    LOGMatcherIMGDone(match);
#endif
  }

#if 0
  if (num_matches > 0) {
    fprintf(stderr, "Matcher: found %d matches.\n", num_matches);
    fprintf(stderr, "Matcher: num_good_cat = %d\n", num_good_cat);
  }
#endif

  // Now deal with catalog stars that were matched twice.
  double closest;
  IMG_DATA *best_fixup;
  int num_fixups = 0;
  for (auto x : cat_list) {
    if (x->matches.size() > 1) {
      closest = 99.9;
      num_fixups++;
      best_fixup = nullptr;
      for (auto m : x->matches) {
	double r = grid->Distance2(x->hgsc_star.location, m->trial_loc);
	if (r < closest) {
	  closest = r;
	  best_fixup = m;
	}
      }
      // un-match the image stars other than the closest
      for (auto m : x->matches) {
	if (m != best_fixup) {
	  m->matches.clear();
	  num_matches--;
	} else {
	  m->residual2 = closest;
	}
      }
      x->matches.clear();
      x->matches.push_back(best_fixup);
    }
  }

  if (do_fixup) {
    for (IMG_DATA *i : image_list) {
      if (i->matches.size()) {
	// Yes, match. Copy some HGSC data into the IStar structure
	HGSC *hgsc = & i->matches.front()->hgsc_star;
	if (strlen(hgsc->label) >= STARNAME_LENGTH) {
	  fprintf(stderr, "ERROR: starname is too long: %s\n",
		  hgsc->label);
	} else {
	  strcpy(i->star.StarName, hgsc->label);
	}
	i->star.dec_ra = hgsc->location;
	i->star.magnitude = hgsc->magnitude;
	i->star.validity_flags |= (DEC_RA_VALID | MAG_VALID | CORRELATED);
	i->star.info_flags = 0;
	if (hgsc->is_comp) {
	  i->star.info_flags |= STAR_IS_COMP;
	}
	if (hgsc->is_check) {
	  i->star.info_flags |= STAR_IS_CHECK;
	}
	if (hgsc->do_submit) {
	  i->star.info_flags |= STAR_IS_SUBMIT;
	}
      } else {
	// non-match.
	i->star.dec_ra = wcs.Transform(i->star.nlls_x, i->star.nlls_y);
	i->star.validity_flags |= DEC_RA_VALID;
      }
    }
  }

#ifdef LOGGER
  //PairDone(num_matches > 6);
  PairDone(true);
#endif
  return num_matches;
}
  
// CalculateWCS: Calculate a new WCS using the matches contained in
// the two star lists. This is a relatively expensive computation, so
// use sparingly. This is thread-safe as long as the caller has a
// dedicated cat_list and image_list for this thread.

class HTransform {
public:
  HTransform(double a, double b, double c, double d) :
    Ha(a), Hb(b), Hc(c), Hd(d) {;}
  ~HTransform(void) {;}

  double transform(double x, double y) {
    return Ha + x*Hb + y*Hc + x*y*Hd;
  }
  
private:
  double Ha, Hb, Hc, Hd;
};

WCS_Bilinear *
CalculateWCS(Context *context,
	     std::vector<CAT_DATA *> &cat_list,
	     std::vector<IMG_DATA *> &image_list,
	     const char *residual_filename) {

#if 0
  fprintf(stderr, "Print Matches..... (cat first)\n");
  for (auto cat : cat_list) {
    if (cat->matches.size()) {
      IMG_DATA *match = cat->matches.front();
      fprintf(stderr, "%p -> %p (%d)\n",
	      cat, match, (int)cat->matches.size());
    }
  }
  fprintf(stderr, "     (now image)\n");
  for (auto img : image_list) {
    if (img->matches.size()) {
      CAT_DATA *match = img->matches.front();
      fprintf(stderr, "%p -> %p (%d)\n",
	      img, match, (int)img->matches.size());
    }
  }
#endif

  //fprintf(stderr, "CalculateWCS() --------------------->\n");

  gsl_matrix *sum_xy_dec = gsl_matrix_calloc(4, 1);
  gsl_matrix *sum_xy_ra  = gsl_matrix_calloc(4, 1);
  gsl_matrix *sum_xx = gsl_matrix_calloc(4, 4);
  gsl_matrix *W = gsl_matrix_alloc(4, 1);
  int n = 0;

  for (auto cat : cat_list) {
    HGSC *this_cat_star = &cat->hgsc_star;

    if (this_cat_star->do_not_trust_position ||
	(cat->matches.size() == 0)) continue;

    IStarList::IStarOneStar *this_image_star = &cat->matches.front()->star;

    double y_dec = this_cat_star->location.dec();
    double y_ra  = this_cat_star->location.ra_radians();
    
    gsl_matrix_set(W, 0, 0, 1.0);
    gsl_matrix_set(W, 1, 0, this_image_star->nlls_x);
    gsl_matrix_set(W, 2, 0, this_image_star->nlls_y);
    gsl_matrix_set(W, 3, 0, this_image_star->nlls_y*this_image_star->nlls_x);

    gsl_blas_dgemm(CblasNoTrans, CblasTrans, 1.0,
		   W, W, 1.0, sum_xx);

    // Next steps are kind of wierd because gsl_matrix_scale()
    // modifies the matrix being scaled. We need to scale by two
    // different factors (y_dec and y_ra), so we scale first by y_dec
    // and then scale by y_ra/y_dec, which will "undo" the first
    // scaling and scale by the second.
    
    gsl_matrix_scale(W, y_dec);
    gsl_matrix_add(sum_xy_dec, W);
    gsl_matrix_scale(W, y_ra/y_dec);
    gsl_matrix_add(sum_xy_ra, W);

    n++;
  }
  //fprintf(stderr, "Calculating WCS with %d pairs\n", n);

  // now that all data has been collected, it's time to solve for the
  // fitting parameters
  gsl_permutation *p = gsl_permutation_alloc(4);
  gsl_matrix *inverse = gsl_matrix_alloc(4, 4);
  int s;
  gsl_linalg_LU_decomp(sum_xx, p, &s);
  gsl_linalg_LU_invert(sum_xx, p, inverse);

  gsl_matrix *result = gsl_matrix_calloc(4, 1);
  gsl_blas_dgemm(CblasNoTrans, CblasNoTrans, 1.0,
		 inverse, sum_xy_dec, 0.0, result);

  // extract our four fitting parameters
  const double fit_a_dec = gsl_matrix_get(result, 0, 0);
  const double fit_b_dec = gsl_matrix_get(result, 1, 0);
  const double fit_c_dec = gsl_matrix_get(result, 2, 0);
  const double fit_d_dec = gsl_matrix_get(result, 3, 0);

  // and now repeat for RA
  gsl_blas_dgemm(CblasNoTrans, CblasNoTrans, 1.0,
		 inverse, sum_xy_ra, 0.0, result);

  // extract our four fitting parameters
  const double fit_a_ra = gsl_matrix_get(result, 0, 0);
  const double fit_b_ra = gsl_matrix_get(result, 1, 0);
  const double fit_c_ra = gsl_matrix_get(result, 2, 0);
  const double fit_d_ra = gsl_matrix_get(result, 3, 0);

  // Now release the matrices we no longer need
  gsl_matrix_free(result);
  gsl_matrix_free(inverse);
  gsl_matrix_free(sum_xy_dec);
  gsl_matrix_free(sum_xy_ra);
  gsl_matrix_free(sum_xx);
  gsl_matrix_free(W);

#if 0
  fprintf(stderr, "Corner declinations:\n");
  fprintf(stderr, "At (0, 0): %lf\n", fit_a_dec);
  fprintf(stderr, "At (%d, 0): %lf\n", img_width, fit_a_dec + img_width*fit_b_dec);
  fprintf(stderr, "At (0, %d): %lf\n", img_height, fit_a_dec + img_height*fit_c_dec);
  fprintf(stderr, "At (%d, %d): %lf\n", img_width, img_height,
	  fit_a_dec + img_width*fit_b_dec + img_height*fit_c_dec + img_width*img_height*fit_d_dec);

  fprintf(stderr, "Corner RAs:\n");
  fprintf(stderr, "At (0, 0): %lf\n", fit_a_ra);
  fprintf(stderr, "At (%d, 0): %lf\n", img_width, fit_a_ra + img_width*fit_b_ra);
  fprintf(stderr, "At (0, %d): %lf\n", img_height, fit_a_ra + img_height*fit_c_ra);
  fprintf(stderr, "At (%d, %d): %lf\n", img_width, img_height,
	  fit_a_ra + img_width*fit_b_ra + img_height*fit_c_ra + img_width*img_height*fit_d_ra);

  fprintf(stderr, "Center (dec, RA) = %.16lf, %.16lf\n",
	  fit_a_dec + (img_width/2.0)*fit_b_dec + (img_height/2.0)*fit_c_dec +
	  (img_width*img_height/4.0)*fit_d_dec,
	  fit_a_ra + (img_width/2.0)*fit_b_ra + (img_height/2.0)*fit_c_ra +
	  (img_width*img_height/4.0)*fit_d_ra);
#endif

  HTransform t_ra(fit_a_ra, fit_b_ra, fit_c_ra, fit_d_ra);
  HTransform t_dec(fit_a_dec, fit_b_dec, fit_c_dec, fit_d_dec);

  const int img_height = context->IMAGE_HEIGHT_PIXELS;
  const int img_width = context->IMAGE_WIDTH_PIXELS;
  WCS_Bilinear *wcs = new WCS_Bilinear(img_height, img_width);
  wcs->SetULPoint(DEC_RA(t_dec.transform(0.0, img_height), t_ra.transform(0.0, img_height)));
  wcs->SetURPoint(DEC_RA(t_dec.transform(img_width, img_height),
			 t_ra.transform(img_width, img_height)));
  wcs->SetLLPoint(DEC_RA(t_dec.transform(0.0, 0.0), t_ra.transform(0.0, 0.0)));
  wcs->SetLRPoint(DEC_RA(t_dec.transform(img_width, 0.0), t_ra.transform(img_width, 0.0)));

  return wcs;
}




