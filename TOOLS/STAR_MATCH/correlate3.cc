/*  correlate1.cc -- Correlate stars in an image with a catalog
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
#include <fstream>
#include <iostream>
#include <assert.h>
#include <pthread.h>
#include <stdlib.h>		// for malloc(), qsort()
#include <unistd.h>		// for sbrk()
#include <stdio.h>
#include <math.h>
#include <string.h>		// strcpy()
#include <list>
#include <map>
#include <bits/stdc++.h>
#include <algorithm>
#include "correlate3.h"
#include "correlate_internal3.h"
#include "matcher3.h"

//#define SINGLE_TASK
//#define DEBUG_SINGLE_PAIR
//#define PRINT_STARS_BEING_USED

void AnalyzePair(ThreadTask *tt,
		Grid *full_grid,
		IMG_DATA *ref_img,
		IMG_DATA *alt_img,
		CAT_DATA *ref_cat,
		CAT_DATA *alt_cat);

//#define PRINTSTATS true

#ifdef PRINTSTATS
double static ARCSEC(double rad) {
  return rad*180.0*3600.0/M_PI;
}

double static max4(double a1, double a2, double a3, double a4) {
  double max0 = a1;
  if (a2 > max0) max0 = a2;
  if (a3 > max0) max0 = a3;
  if (a4 > max0) max0 = a4;
  return max0;
}

double static min4(double a1, double a2, double a3, double a4) {
  double min0 = a1;
  if (a2 < min0) min0 = a2;
  if (a3 < min0) min0 = a3;
  if (a4 < min0) min0 = a4;
  return min0;
}
#endif

double static inline max2(double a1, double a2) {
  return (a1 < a2) ? a2 : a1;
}
double static inline min2(double a1, double a2) {
  return (a1 < a2) ? a1 : a2;
}


//#define CORRELATE_DEBUG
//#define USE_INTENSITY

CAT_DATA *find_hgsc_by_name(const char *name,
			    std::vector<CAT_DATA *> list) {
  for (auto x : list) {
    if (strcmp(x->hgsc_star.label, name) == 0) return x;
  }
  return nullptr;
}

IMG_DATA *find_img_by_name(const char *name,
			   std::vector<IMG_DATA *> list) {
  for (auto x : list) {
    if (strcmp(x->star.StarName, name) == 0) return x;
  }
  return nullptr;
}
  
WCS_Simple *TwoPairToWCS(CAT_DATA *ref_cat,
			 CAT_DATA *alt_cat,
			 IMG_DATA *ref_img,
			 IMG_DATA *alt_img,
			 Context *context);
void *correlate_thread(void *all_params);

#define CATCH_KNOWN_MATCHES
#ifdef CATCH_KNOWN_MATCHES
struct pair {
  const char *s1;
  const char *s2;
} knownmatches[] = {
		    { "S026", "GSC03043-00369" },
		    { "S011", "GSC03043-00005" },
		    { "S001", "GSC03043-00211" },
		    { "S007", "GSC03043-00349" },
		    { "S017", "GSC03043-00036" },
		    { "S012", "GSC03043-00115" },

};

bool IsTargetMatch(const char *name1, const char *name2) {
  for (unsigned int i=0; i< sizeof(knownmatches)/sizeof(knownmatches[0]); i++) {
    if (strcmp(name1, knownmatches[i].s1) == 0 and
	strcmp(name2, knownmatches[i].s2) == 0) return true;
  }
  return false;
}
#endif

bool compare_img(const IMG_DATA *i1, const IMG_DATA *i2) {
  return i1->star.nlls_counts > i2->star.nlls_counts; // brightest first
}

bool compare_cat(const CAT_DATA *c1, const CAT_DATA *c2) {
  return c1->hgsc_star.magnitude < c2->hgsc_star.magnitude;
}

int compare_solution(const Solution *s1, const Solution *s2) {
  const int delta = s1->num_img_matches - s2->num_img_matches;
  if (delta == 0) return 0;
  return (delta > 0) ? 1 : -1;
}

//****************************************************************
//        correlate(): this is where it all starts...
//****************************************************************
const WCS *
correlate(Image &primary_image,
	  IStarList *list,
	  const char *HGSCfilename,
	  DEC_RA *ref_location,
	  const char *param_filename,
	  const char *residual_filename,
	  Context &context) {
  std::ofstream param_stream;
  if(param_filename) param_stream.open(param_filename);

  //********************************
  // Setup Context
  //********************************
  ImageInfo *info = primary_image.GetImageInfo();
  if (info and info->CDeltValid()) {
    context.PIXEL_SCALE_ARCSEC = info->GetCDelt1();
  } else {
    context.PIXEL_SCALE_ARCSEC = 1.52;
  }
  context.PIXEL_SCALE_RADIANS = context.PIXEL_SCALE_ARCSEC * (1.0/3600.0) * (M_PI/180.0);
  context.IMAGE_HEIGHT_PIXELS = primary_image.height;
  context.IMAGE_WIDTH_PIXELS = primary_image.width;
  context.IMAGE_HEIGHT_RAD = context.IMAGE_HEIGHT_PIXELS * context.PIXEL_SCALE_RADIANS;
  context.IMAGE_WIDTH_RAD = context.IMAGE_WIDTH_PIXELS * context.PIXEL_SCALE_RADIANS;
  context.nominal_image_center = *ref_location;
  context.sin_center_dec = sin(ref_location->dec());
  context.cos_center_dec = cos(ref_location->dec());
#if defined(PRINTSTATS) || defined(DEBUG_SINGLE_PAIR) || defined(SINGLE_TASK)
  context.NUM_TASKS = 1;
#else
  context.NUM_TASKS = 16;
#endif
  context.center_pixel_x = context.IMAGE_WIDTH_PIXELS/2;
  context.center_pixel_y = context.IMAGE_HEIGHT_PIXELS/2;
  context.camera_orientation = info->GetRotationAngle();

  fprintf(stderr, "Using rotation angle = %.2lf\n", context.camera_orientation);
  fprintf(stderr, "Using pixel scale of %lf arcsec/pixel\n", context.PIXEL_SCALE_ARCSEC);

  //Truth truth(context.image_filename);

  // READ in the HGSC stars
  FILE *hgsc_fp = fopen(HGSCfilename, "r");
  if(!hgsc_fp) {
    fprintf(stderr, "Correlate: cannot open '%s'\n", HGSCfilename);
    return nullptr;
  }
  HGSCList hgsc(hgsc_fp);
  fclose(hgsc_fp);

  //********************************
  // Create the master image_list
  // (This is already sorted by brightness, since the IStarList in the
  // image was sorted to start with.)
  //********************************
  std::vector<IMG_DATA *> init_img_list;
  for(int i=0; i<list->NumStars; i++) {
    IStarList::IStarOneStar *this_star = list->FindByIndex(i);
    if(this_star->validity_flags & COUNTS_VALID) {
      IMG_DATA *img_star = new IMG_DATA(*this_star);
      init_img_list.push_back(img_star);
    }
  }

  std::sort(init_img_list.begin(),
	    init_img_list.end(),
	    compare_img);
  int index = 0;
  for (auto s : init_img_list) {
    s->index = index++;
  }
  fprintf(stderr, "Total of %ld stars in image.\n",
	  init_img_list.size());

  //********************************
  // Create the master HGSC list
  //********************************

  std::vector<CAT_DATA *> cat_list;
  HGSCIterator it(hgsc);
  for (HGSC *h = it.First(); h; h = it.Next()) {
    CAT_DATA *cat = new CAT_DATA(*h);
    cat_list.push_back(cat);

    if (h->location.dec() > context.max_cat_dec) {
      context.max_cat_dec = h->location.dec();
    }
    if (h->location.dec() < context.min_cat_dec) {
      context.min_cat_dec = h->location.dec();
    }
    if (h->location.ra_radians() > context.max_cat_ra) {
      context.max_cat_ra = h->location.ra_radians();
    }
    if (h->location.ra_radians() < context.min_cat_ra) {
      context.min_cat_ra = h->location.ra_radians();
    }
  }

  //********************************
  // Eliminate RA wraparound within
  // the HGSC list
  //********************************
  context.wraparound = (context.max_cat_ra - context.min_cat_ra > M_PI);
  if (context.wraparound) {
    // yes, we have a wraparound that needs fixing
    context.max_cat_ra = -99.9;
    context.min_cat_ra = 99.9;
    for (HGSC *h = it.First(); h; h = it.Next()) {
      // turn high RA values into small negative values
      if (h->location.ra_radians() > M_PI) {
	h->location = DEC_RA(h->location.dec(),
			     h->location.ra_radians() - 2*M_PI);
      }
      if (h->location.ra_radians() < context.min_cat_ra) {
	context.min_cat_ra = h->location.ra_radians();
      }
      if (h->location.ra_radians() > context.max_cat_ra) {
	context.max_cat_ra = h->location.ra_radians();
      }
    }

  }

  std::sort(cat_list.begin(), cat_list.end(), compare_cat);
  index = 0;
  for (auto c : cat_list) {
    c->index = index++;
  }

  fprintf(stderr, "Catalog holds %ld stars\n",
	  cat_list.size());

  //********************************
  // Set up threads
  //********************************
  ThreadTask tasks[context.NUM_TASKS];
  pthread_t thread_ids[context.NUM_TASKS];
  for (int i=0; i<context.NUM_TASKS; i++) {
    tasks[i].task_number = i;
    tasks[i].context = &context;
    //tasks[i].truth = &truth;
    for (auto s : init_img_list) {
      IMG_DATA *img = new IMG_DATA(s);
      tasks[i].all_image_stars.push_back(img);
    }
    for (auto x : cat_list) {
      CAT_DATA *cat = new CAT_DATA(x->hgsc_star);
      cat->index = x->index;
      tasks[i].all_cat_stars.push_back(cat);
    }
  }

  {
    const int num_stars = init_img_list.size();
    for (int s=0; s<10; s++) {
      int i = s % context.NUM_TASKS;
      if (s < num_stars) {
	tasks[i].star_assignments.push_back(tasks[i].all_image_stars[s]);
      }
    }
  }

  for (int i=0; i<context.NUM_TASKS; i++) {
    int err = pthread_create(&thread_ids[i],
			     nullptr, // attributes for the thread
			     &correlate_thread,
			     &tasks[i]);
    if (err) {
      std::cerr << "Error creating thread in correlate(): %d\n" <<
	err << std::endl;
    }
  }

  Solution best_solution({nullptr, -1, -1});
  std::vector<int> histogram;
  
  // Now wait for all threads to complete
  for (int i=0; i<context.NUM_TASKS; i++) {
    int s = pthread_join(thread_ids[i], nullptr);
    if (s != 0) {
      perror("pthread_join");
    } else {
      if (BetterThan(tasks[i].best_solution,
		     best_solution)) {
	best_solution = tasks[i].best_solution;
      }
      // merge histograms
      if (tasks[i].histogram.size() > histogram.size()) {
	histogram.resize(tasks[i].histogram.size(), 0);
      }
      for (unsigned int h=0; h<tasks[i].histogram.size(); h++) {
	histogram[h] += tasks[i].histogram[h];
      }
    }
  }

  // Calculate histogram summary
  int sum_histogram = 0;
  int num_runs = 0;
  for (unsigned int x = 0; x<histogram.size(); x++) {
    sum_histogram += histogram[x]*x;
    num_runs += histogram[x];
  }
  const double histogram_avg = (double)sum_histogram/(double)num_runs;
  unsigned long sum_delta_sq = 0;
  for (unsigned int x=0; x<histogram.size(); x++) {
    sum_delta_sq += histogram[x]*(x-histogram_avg)*(x-histogram_avg);
  }
  const double stddev = sqrt((double)sum_delta_sq/(double)num_runs);
  fprintf(stderr, "Avg matches = %.3lf, Match stddev = %.3lf\n",
	  histogram_avg, stddev);

  const double num_stddev = (best_solution.num_img_matches-histogram_avg)/stddev;

  fprintf(stderr, "Best solution is at %.1lf sigma above average.\n", num_stddev);

  if (best_solution.solution_wcs == nullptr or num_stddev < 4.0) {
    fprintf(stderr, "No solution found.\n");
  } else {
    fprintf(stderr, "Best solution has %d matches.\n",
	    best_solution.num_img_matches);

    Grid *full_grid = InitializeGrid(&context, cat_list, 60.0*M_PI/(180.0*3600.0));
    int num_match = Matcher(&context,
			    full_grid,
			    *best_solution.solution_wcs,
			    cat_list,
			    init_img_list,
			    9999,
			    10.0*(M_PI/(3600.0*180.0)),
			    true);
    fprintf(stderr, "final num_match = %d\n", num_match);
    best_solution.solution_wcs->PrintRotAndScale();
  
    return best_solution.solution_wcs;
  }
  return nullptr;
}

IMG_DATA::IMG_DATA(IStarList::IStarOneStar &starlist_entry) : star(starlist_entry) {
  intensity = -2.5*log10(starlist_entry.nlls_counts);
  is_bright_star = (starlist_entry.validity_flags & SELECTED) != 0;
}

IMG_DATA::IMG_DATA(const IMG_DATA *clone) : star(clone->star) {
  index = clone->index;
  intensity = clone->intensity;
  is_bright_star = clone->is_bright_star;
}
  

bool BetterThan(Solution &s1, Solution &s2) {
  if (s1.solution_wcs == nullptr) return false;
  if (s2.solution_wcs == nullptr) return true;
  return (s1.num_img_matches > s2.num_img_matches);
}

void *correlate_thread(void *all_params) {
  std::list<IMG_DATA *> assignments;

  ThreadTask *tt = (ThreadTask *) all_params;
  Grid *full_grid = InitializeGrid(tt->context, tt->all_cat_stars, 60.0*M_PI/(180.0*3600.0));
  tt->best_solution = Solution({ nullptr, 0, 0 });

  //****************************************************************
  //        DEBUG_SINGLE_PAIR
  //****************************************************************
#ifdef DEBUG_SINGLE_PAIR
  CAT_DATA *ref_cat = find_hgsc_by_name("GSC03043-00369", tt->all_cat_stars);
  //CAT_DATA *alt_cat = find_hgsc_by_name("GSC02645-01375", cat_list);
  CAT_DATA *alt_cat = find_hgsc_by_name("GSC03043-00211", tt->all_cat_stars);
  IMG_DATA *ref_img = find_img_by_name("S026", tt->all_image_stars);
  IMG_DATA *alt_img = find_img_by_name("S001", tt->all_image_stars);
  assert(ref_cat);
  assert(alt_cat);
  assert(ref_img);
  assert(alt_img);

  WCS_Simple *wcs_test = TwoPairToWCS(ref_cat, alt_cat, ref_img, alt_img, tt->context);

  // And test...
  DEC_RA alt_loc = wcs_test->Transform(alt_img->star.nlls_x, alt_img->star.nlls_y);
  DEC_RA ref_loc = wcs_test->Transform(ref_img->star.nlls_x, ref_img->star.nlls_y);

  fprintf(stderr, "ALT Location test: (%s, %s) [transformed]\n",
	  alt_loc.string_fulldec_of(), alt_loc.string_longra_of());
  fprintf(stderr, "   ALT catalog: (%s, %s)\n",
	  alt_cat->hgsc_star.location.string_fulldec_of(),
	  alt_cat->hgsc_star.location.string_longra_of());
  fprintf(stderr, "REF Location test: (%s, %s) [transformed]\n",
	  ref_loc.string_fulldec_of(), ref_loc.string_longra_of());
  fprintf(stderr, "   REF catalog: (%s, %s)\n",
	  ref_cat->hgsc_star.location.string_fulldec_of(),
	  ref_cat->hgsc_star.location.string_longra_of());

  AnalyzePair(tt, full_grid,
	      ref_img, alt_img, ref_cat, alt_cat);
  

#else
  //****************************************************************
  //        NORMAL (non-single-pair)
  //****************************************************************
  unsigned int num_assignments = 0;
  for (auto a : tt->star_assignments) {
    for (auto temp : tt->all_image_stars) {
      if (a->index == temp->index) {
	assignments.push_back(temp);
	num_assignments++;
	break;
      }
    }
  }
  assert(num_assignments == tt->star_assignments.size());

  // The values of 40 and 40 below were chosen on 1/28/2024 to resolve a problem with m67 images
  // in Ic filter not being solvable.
  unsigned int pair_img_limit = 40;
  if (tt->all_image_stars.size() < pair_img_limit) pair_img_limit = tt->all_image_stars.size();
  unsigned int pair_cat_limit = 4000;
  if (tt->all_cat_stars.size() < pair_cat_limit) pair_cat_limit = tt->all_cat_stars.size();

  // Initial matches are done by aligning 10 brightest image stars
  // with 40 brightest catalog stars (ratio 10:40 matches the area
  // ratio of image : catalog_file)
#ifdef PRINT_STARS_BEING_USED
  fprintf(stderr, "Image stars:");
  int i=0;
  for (unsigned int s_index = 0; s_index < pair_img_limit; s_index++) {
    IMG_DATA *ref_img = tt->all_image_stars[s_index];
    i = (i+1) % 10;
    if (i == 1) {
      fprintf(stderr, "\n     ");
    }
    fprintf(stderr, "%s ", ref_img->star.StarName);
  }
  fprintf(stderr, "\n");
  i=0;
  for (unsigned int s_index = 0; s_index < pair_cat_limit; s_index++) {
    CAT_DATA *ref_cat = tt->all_cat_stars[s_index];
    i = (i+1) % 10;
    if (i == 1) {
      fprintf(stderr, "\n     ");
    }
    fprintf(stderr, "%s ", ref_cat->hgsc_star.label);
  }
  fprintf(stderr, "\n");
#endif

  for (auto ref_img : tt->star_assignments) {
    for (unsigned int ref_cat_index = 0; ref_cat_index < pair_cat_limit; ref_cat_index++) {
      CAT_DATA *ref_cat = tt->all_cat_stars[ref_cat_index];
      for (unsigned int alt_img_index = ref_img->index+1; alt_img_index < pair_img_limit; alt_img_index++) {
	IMG_DATA *alt_img = tt->all_image_stars[alt_img_index];
	for (unsigned int alt_cat_index = 0; alt_cat_index < pair_cat_limit; alt_cat_index++) {
	  CAT_DATA *alt_cat = tt->all_cat_stars[alt_cat_index];
	  if (ref_cat != alt_cat) {
	    AnalyzePair(tt, full_grid,
			ref_img, alt_img, ref_cat, alt_cat);
	  }
	}
      }
    }
  }
#ifdef PRINTSTATS
  fprintf(stderr, "Thread %d completed: %d pairs explored\n   %d went to pass 1\n",
  	  tt->task_number, tt->num_pairs, tt->num_pass1);
  fprintf(stderr, "    %d went to pass 2\n   %d went to pass 3\n   %d went to pass 4\n",
  	  tt->num_pass2, tt->num_pass3, tt->num_pass4);
#endif // PRINTSTATS
#endif // not DEBUG_SINGLE_PAIR
  delete full_grid;

  return 0;
}
    
WCS_Simple *TwoPairToWCS(CAT_DATA *ref_cat,
			 CAT_DATA *alt_cat,
			 IMG_DATA *ref_img,
			 IMG_DATA *alt_img,
			 Context *context) {
  const double delta_pixel_x = alt_img->star.nlls_x - ref_img->star.nlls_x;
  const double delta_pixel_y = alt_img->star.nlls_y - ref_img->star.nlls_y;
  const double delta_pixel_r = sqrt(delta_pixel_x*delta_pixel_x +
				    delta_pixel_y*delta_pixel_y);

  const double theta_img = atan2(delta_pixel_y, delta_pixel_x);

  const double delta_dec = alt_cat->hgsc_star.location.dec() -
    ref_cat->hgsc_star.location.dec();
  const double delta_dec_arcsec = delta_dec*3600.0*180.0/M_PI;
  double delta_ra = alt_cat->hgsc_star.location.ra_radians() -
    ref_cat->hgsc_star.location.ra_radians();
  if (delta_ra > M_PI) delta_ra -= 2.0*M_PI;
  if (delta_ra < -M_PI) delta_ra += 2.0*M_PI;
  const double delta_ra_arcsec = (3600.0*180.0/M_PI)*delta_ra * context->cos_center_dec;

  const double theta_cat = atan2(delta_dec_arcsec, delta_ra_arcsec);
  // rotation is the angle in radians, measured CCW, to shift
  // the true sky into the camera frame of reference
  const double rotation = theta_img - theta_cat;

  const double delta_arcsec = sqrt(delta_ra_arcsec*delta_ra_arcsec +
				   delta_dec_arcsec*delta_dec_arcsec);
  const double scale = delta_arcsec/delta_pixel_r;

  WCS_Simple *wcs = new WCS_Simple;
  wcs->SetImageSize(context->IMAGE_WIDTH_PIXELS,
		    context->IMAGE_HEIGHT_PIXELS);
  wcs->Set(context->nominal_image_center, scale, rotation);
  // Translate the ref_img star into dec/ra; the resulting offset
  // error will be used to adjust the WCS
  DEC_RA trial = wcs->Transform(ref_img->star.nlls_x, ref_img->star.nlls_y);
  const double offset_dec = trial.dec() - ref_cat->hgsc_star.location.dec();
  const double offset_ra = trial.ra_radians() - ref_cat->hgsc_star.location.ra_radians();

  DEC_RA new_center(context->nominal_image_center.dec() - offset_dec,
		    context->nominal_image_center.ra_radians() - offset_ra);
  wcs->Set(new_center, scale, rotation);

  return wcs;
}

void AnalyzePair(ThreadTask *tt,
		Grid *full_grid,
		IMG_DATA *ref_img,
		IMG_DATA *alt_img,
		CAT_DATA *ref_cat,
		CAT_DATA *alt_cat) {
#ifdef PRINTSTATS
  bool enable_printing = false;
#endif

  if (strcmp(alt_cat->hgsc_star.label, "GSC01081-00835") == 0 and
      strcmp(ref_cat->hgsc_star.label, "GSC01081-00698") == 0 and
      strcmp(alt_img->star.StarName, "S415") == 0 and
      strcmp(ref_img->star.StarName, "GSC01081-00804") == 0) {
    fprintf(stderr, "This is a correct pair.\n");
  }

  tt->num_pairs++;
  unsigned int final_match = 2;
  WCS_Simple *wcs = TwoPairToWCS(ref_cat, alt_cat, ref_img, alt_img, tt->context);
  if (wcs) {
    tt->num_pass1++;
    double tolerance = 10.0*M_PI/(180.0*3600.0); // 10 arcsec
    int pass1_match = Matcher(tt->context,
			      full_grid,
			      *wcs,
			      tt->all_cat_stars,
			      tt->all_image_stars,
			      10, 
			      tolerance,
			      false); // do_fixup
    delete wcs;
    final_match = pass1_match;
#ifdef PRINTSTATS
    fprintf(stderr, "pass1_match = %d\n", pass1_match);
#endif
    if (pass1_match >= 4) {
#ifdef PRINTSTATS
      enable_printing = true;
      fprintf(stderr, "Entering pass2:\n");
#endif
      tt->num_pass2++;
      WCS_Bilinear *full_wcs = CalculateWCS(tt->context,
					    tt->all_cat_stars,
					    tt->all_image_stars,
					    nullptr);
      int num_match = Matcher(tt->context,
			      full_grid,
			      *full_wcs,
			      tt->all_cat_stars,
			      tt->all_image_stars,
			      10, 
			      tolerance,
			      false); // do_fixup
      final_match = num_match;
#ifdef PRINTSTATS
      fprintf(stderr, "pass2 num_match = %d\n", num_match);
      {
	ResidualStatistics stats;
	ComputeStatistics(tt->all_image_stars, stats);
	fprintf(stderr, "     residual avg/median/stddev = %.2lf, %.2lf, %.2lf (arcsec)\n",
		ARCSEC(stats.average), ARCSEC(stats.median), ARCSEC(stats.stddev));
      }
#endif
      delete full_wcs;
      if (num_match >= 4) {
	tt->num_pass3++;
	full_wcs = CalculateWCS(tt->context,
				tt->all_cat_stars,
				tt->all_image_stars,
				nullptr);
	num_match = Matcher(tt->context,
			    full_grid,
			    *full_wcs,
			    tt->all_cat_stars,
			    tt->all_image_stars,
			    20, 
			    tolerance,
			    false); // do_fixup
	final_match = num_match;
#ifdef PRINTSTATS
	fprintf(stderr, "pass3 num_match = %d\n", num_match);
	{
	  ResidualStatistics stats;
	  ComputeStatistics(tt->all_image_stars, stats);
	  fprintf(stderr, "     residual avg/median/stddev = %.2lf, %.2lf, %.2lf (arcsec)\n",
		  ARCSEC(stats.average), ARCSEC(stats.median), ARCSEC(stats.stddev));
	}
#endif
	delete full_wcs;
	if (num_match >= 4) {
	  tt->num_pass4++;
	  full_wcs = CalculateWCS(tt->context,
				  tt->all_cat_stars,
				  tt->all_image_stars,
				  nullptr);
	  num_match = Matcher(tt->context,
			      full_grid,
			      *full_wcs,
			      tt->all_cat_stars,
			      tt->all_image_stars,
			      9999, 
			      tolerance,
			      false); // do_fixup
	  final_match = num_match;
#ifdef PRINTSTATS
	  fprintf(stderr, "pass4 num_match = %d\n", num_match);
	  {
	    ResidualStatistics stats;
	    ComputeStatistics(tt->all_image_stars, stats);
	    fprintf(stderr, "     residual avg/median/stddev = %.2lf, %.2lf, %.2lf (arcsec)\n",
		    ARCSEC(stats.average), ARCSEC(stats.median), ARCSEC(stats.stddev));
	  }
#endif
	  if (num_match >= 4) {
#ifdef PRINTSTATS
	    fprintf(stderr, "    ref/alt = %s/%s\n",
		    ref_cat->hgsc_star.label,
		    alt_cat->hgsc_star.label);
#endif
	    if (num_match > tt->best_solution.num_img_matches) {
#ifdef PRINTSTATS
	      fprintf(stderr, "new best solution in thread.\n");
#endif
	      delete tt->best_solution.solution_wcs;
	      tt->best_solution = Solution({full_wcs, num_match, 0});
	    } else {
	      delete full_wcs;
	    }
	  } else {

	    delete full_wcs;
	  }
	}
      }
    }
  }
  if (final_match >= tt->histogram.size()) {
    tt->histogram.resize(final_match+1, 0);
  }
  tt->histogram[final_match]++;
} 
  
