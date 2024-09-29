/*  focus.cc -- Program to perform auto-focus
 *
 *  Copyright (C) 2007, 2017 Mark J. Munkacsy
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
#include "/home/mark/gsl-2.4/gsl/gsl_vector.h"
#include "/home/mark/gsl-2.4/gsl/gsl_matrix.h"
#include "/home/mark/gsl-2.4/gsl/gsl_rng.h"
#include "/home/mark/gsl-2.4/gsl/gsl_blas.h"
#include "/home/mark/gsl-2.4/gsl/gsl_multifit.h"
#include <stdio.h>
#include <Image.h>
#include <scope_api.h>
#include <camera_api.h>
#include <unistd.h>		// sleep()
#include <hyperbola.h>
#include "focus.h"
#include <pthread.h>
#include <list>
#include <string.h>		// strdup()
#include <assert.h>
#include <iostream>
#include <gaussian_fit.h>
#include <system_config.h>
#include "proc_messages.h"

using namespace std;

double DoFineFocus(double coarse_focus);
double FineMeasure(const char *filename);
void SolveParabola(double *a, double *b, double *c);
double aperture_sum(double centerx, double centery, Image &image);

#define PARAM_FILE_PATH "/tmp/focus_param.txt"

static int box_bottom;
static int box_top;
static int box_left;
static int box_right;

static double low_threshold { 2.0 }; // below this is "near-focus"
static double high_threshold { 8.0 }; // above this is "wild" (do not trust)
static int target_box_size { 144 }; // pixels (must be divisible by 3)

static int max_blur {10};

static int camera_is_busy = 0;

static RunData run_data;

extern int inhibit_plotting;
extern FocuserName focuser_to_use;
extern FILE *logfile;

FILE *fp_plot = 0;

SystemConfig *config {nullptr};
double image_scale {1.52}; // ST-9 default with 100" EFL SCT
double hyperbola_C {64.0};

//****************************************************************
//        Establish some limits
//****************************************************************
 int MIN_TRAVEL = 50; // never command focuser below here
 int MAX_TRAVEL = 2450; // never command focuser beyond here
 int MIN_SOLUTION = 100; // don't allow hyperbola soln below
 int MAX_SOLUTION = 2400; // don't allow hyperbola soln above

double MAX_STARSIZE {8.0}; // Again, a ST-9 default

//****************************************************************
//        Forward declarations
//****************************************************************
struct ResultSummary {
  int number_bad;
  int useful_on_high_side;
  int useful_on_low_side;
  int useful_near_focus;
};

struct ExposureRequest;
void create_requests(int num_requests, int low_limit, int high_limit);
void assess_results(ResultSummary *results, double focus_estimate);
bool add_image(CompositeImage *composite_image, Image *i);
void ScheduleExposure(ExposureRequest *r);
void *exposer_thread(void *exposure_time); 
void FetchAndProcessExposures(double *current_estimate); 
//****************************************************************

static bool user_aborted = false;

bool user_abort_requested(void) {
  int message_id;
  if (ReceiveMessage("focus", &message_id)) {
    user_aborted = true;
    return true;
  }
  return false;
}

long set_focus(long encoder_to_set) {
  long current_encoder = CumFocusPosition(focuser_to_use);
  long delta_encoder = encoder_to_set - current_encoder;
  bool direction_change = (preferred_direction == DIRECTION_POSITIVE ?
			   delta_encoder < 0 : delta_encoder >= 0);
  int direction_backwards = (preferred_direction == DIRECTION_POSITIVE ? -1 : 1);

  if (direction_change) {
    static constexpr int BACKLASH_VALUE = 600;
    scope_focus(BACKLASH_VALUE*direction_backwards+delta_encoder,
		FOCUSER_MOVE_RELATIVE,
		focuser_to_use);
    current_encoder = scope_focus(-BACKLASH_VALUE*direction_backwards,
				  FOCUSER_MOVE_RELATIVE,
				  focuser_to_use);
  } else {
    current_encoder = scope_focus(encoder_to_set,
				  FOCUSER_MOVE_ABSOLUTE,
				  focuser_to_use);
  }
  return current_encoder;
}

void adjust_box(Image *image) {
  static int first_call = 1;

  //if (first_call) image->find_stars();

  if(image->GetIStarList()->NumStars == 0) {
    fprintf(stderr, "ERROR: no stars found. Giving up.\n");
    exit(-2);
  }

  // Initialize center_x,y to the location of the largest star found
  // by GetIStarList() 
  int largest_star_index = image->LargestStar();
  double center_x =
    image->GetIStarList()->StarCenterX(largest_star_index);
  double center_y =
    image->GetIStarList()->StarCenterY(largest_star_index);
  //const double total_count = image->GetIStarList()->IStarPixelSum(largest_star_index);
  const double background = image->statistics()->StdDev;
  //const double SNR = (total_count/background);
  const double total_count = aperture_sum(center_x, center_y, *image);
  const double SNR =
    (total_count-image->statistics()->AveragePixel)/background;

  fprintf(stderr, "star center at (%f, %f) with SNR = %.1lf\n", center_x, center_y, SNR);
  fprintf(stderr, "   total_count = %.1lf, background = %.1lf\n", total_count, background);

  // Note that "SNR" isn't really SNR, because the
  // total pixel count is the sum over multiple pixels, while
  // background is a per-pixel value.
  if (SNR < 2.0) {
    fprintf(stderr, "Quitting because SNR is too low.\n");
    exit(-2);
  }

  if(first_call) {
    SystemConfig config;
    if (config.IsST9()) {
      center_y = image->height - center_y;
    }
    first_call = 0;
  } else {
    center_x += box_left;
    center_y = box_top - center_y;
  }

  // must be divisible by 3!
  //const int boxsize_h = (((int) (108.0/image_scale))/3) * 3;
  //const int boxsize_v = boxsize_h;
  const int boxsize_h = target_box_size;
  const int boxsize_v = target_box_size;

  if (center_x < boxsize_h/2) center_x = boxsize_h/2;
  if (center_y < boxsize_v/2) center_y = boxsize_v/2;
  if (center_x > (image->width - boxsize_h/2)) center_x = (image->width - boxsize_h/2);
  if (center_y > (image->height - boxsize_v/2)) center_y = (image->height - boxsize_v/2);
  
  box_bottom = ((int) center_y) -boxsize_v/2;
  box_top = box_bottom + boxsize_v-1;
  box_left = ((int) center_x) - boxsize_h/2;
  box_left = 3*(box_left/3);
  box_right = box_left + boxsize_h-1;

  /* fprintf(stderr,
	  "imaging box at bottom = %d, top = %d, left = %d, right = %d\n",
	  box_bottom, box_top, box_left, box_right); */
}

//****************************************************************
//        Thread coordination
//****************************************************************

// Handling of composites.
// "Normal" ExposureRequests have is_composite set to false. They
// result in a OneMeasurement that has is_composite set to false, the
// composite image is <nil>, and num_exposures will be 1.
// If that measurement, though, fails to converge, a composite
// OneMeasurement will be created, holding a composite image that is
// initialized to the image that wouldn't converge. Four new
// ExposureRequests will be immediately generated and queued, with
// is_composite set for each of the four.
//
// When each of those 4 completes, we will see the is_composite flag
// set, and we won't even try to measure blur. Instead, we will add
// the image to the composite. Once we have 5 images in the composite,
// the composite will be submitted for analysis.

struct OneMeasurement {
  int focus_encoder;
  int num_exposures;		// if composite
  bool is_composite;
  bool is_fine_focus;
  char *image_filename;		// if not composite
  double err;			// relative to curve-fit model
  CompositeImage *composite;	// if composite
  double measured_focus;        // if < 0.0, not measurable
};

struct ExposureRequest {
  int quantity;			// a quantity of -1 means all done
  int focus_encoder;
  bool is_composite;
  char *image_filename;		// filled in when image obtained
  OneMeasurement *corresponding_composite; // only if is_composite
  OneMeasurement *result;
};

// Requests are put into the request_list sorted by focus position, so
// that the focusing process is monotonically increasing (avoids
// errors due to sloppy backlash).
bool compare_requests_positive(const ExposureRequest *first,
			       const ExposureRequest *second) {
  return first->focus_encoder < second->focus_encoder;
}
bool compare_requests_negative(const ExposureRequest *first,
			       const ExposureRequest *second) {
  return first->focus_encoder > second->focus_encoder;
}

pthread_mutex_t mutex_request = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_done = PTHREAD_MUTEX_INITIALIZER;

// no mutex protection
std::list <ExposureRequest *> pending_requests;
// protected by mutex_request
std::list <ExposureRequest *> request_list;
// protected by mutex_done
std::list <ExposureRequest *> request_done;
// no mutex protection
std::list <OneMeasurement *> all_measurements;

pthread_cond_t request_lock = PTHREAD_COND_INITIALIZER;
pthread_cond_t finished_lock = PTHREAD_COND_INITIALIZER;

Filter focus_filter;

//****************************************************************
//        ScheduleExposure
//****************************************************************
void ScheduleExposure(ExposureRequest *r) {
  if (r->quantity > 0) {
    fprintf(stderr, "scheduling exposure at focus setting of %d\n",
	    r->focus_encoder);
  }
  pending_requests.push_back(r);
}

void PromotePendingRequests(void) {
  // put onto the request_list queue
  pending_requests.sort(preferred_direction == DIRECTION_POSITIVE ?
			compare_requests_positive : compare_requests_negative);
  
  pthread_mutex_lock (&mutex_request);
  for (auto r : pending_requests) {
    request_list.push_back(r);
  }
  pthread_cond_signal (&request_lock);
  pthread_mutex_unlock (&mutex_request);

  pending_requests.clear();
}
  
//****************************************************************
//        GetPoints
//****************************************************************
int GetPoints(double focus_position, int count, double exposure_time) {
  double non_converge_count = 0;
  int quit;
  int good_count = 0;
  double *hold_area = (double *) malloc(sizeof(double)*count);

  while(good_count < count &&
	non_converge_count < 5) {

    if (user_abort_requested()) break;

    exposure_flags flags("focus");
    flags.SetFilter(focus_filter);
    flags.subframe.box_bottom = box_bottom;
    flags.subframe.box_top    = box_top;
    flags.subframe.box_left   = box_left;
    flags.subframe.box_right  = box_right;
    
    char *this_image_filename = expose_image(exposure_time,
					     flags,
					     "FOCUS");

    char command_buffer[256];
    sprintf(command_buffer, "/home/mark/ASTRO/CURRENT/TOOLS/FOCUS_MODEL/find_match -i %s -s -g 0.5 | fgrep AnswerBlur > /tmp/focus.out",
	    this_image_filename);
    int return_value = system(command_buffer);
    fprintf(stderr, "find_match command returned %d\n", return_value);
      
    FILE *fp = fopen("/tmp/focus.out", "r");
    if (!fp) {
      fprintf(stderr, "Error: cannot open /tmp/focus.out\n");
    } else {
      double this_blur = -1.0;
      if (fscanf(fp, "AnswerBlur %lf", &this_blur) != 1) {
	fprintf(stderr, "cannot parse output of find_match (a)\n");
      }
      fprintf(stderr, "focus = %d, blur = %lf\n",
	      (int) focus_position, this_blur);
      
      if (this_blur > 0.0) {
	hold_area[good_count++] = this_blur;
      } else {
	non_converge_count++;
      }
      fclose(fp);
      unlink("/tmp/focus.out");
    }
  }

  if(good_count >= count) {
    double best = 1000000.0;
    int best_j = -1;
    int worst_j = -1;
    double worst = 0.0;

    if(good_count > 3) {
      int j;
      for(j = 0; j < good_count; j++) {
	if(hold_area[j] > worst) {
	  worst = hold_area[j];
	  worst_j = j;
	}
	if(hold_area[j] < best) {
	  best  = hold_area[j];
	  best_j = j;
	}
      }
      if(worst_j >= 0) hold_area[worst_j] = 0.0;
      if(best_j >= 0) hold_area[best_j] = 0.0;
    }
	
    while(good_count--) {
      if(hold_area[good_count] != 0.0)
	run_data.add(focus_position, hold_area[good_count]);
    }
    quit = 1;			// success
  } else {
    quit = -1;
  }
  return quit;
}

// Uncomment to use the pthreads version of focus
#define USE_NEW			// use PTHREADS version

#ifdef USE_NEW
// This is the PTHREADS version

void
focus(Image *initial_image,
      double exposure_time_val,
      long initial_encoder,
      int focus_time,
      Image *dark_image,
      Filter filter) {

  if (config == nullptr) {
    config = new SystemConfig;
    if (not isnormal(config->PixelScale())) {
      fprintf(stderr, "focus: pixel scale not found in SystemConfig. Can't focus.\n");
      return;
    }
    
    switch (config->GetOpticalConfiguration()) {
    case SC_ST9_Meade10:
      target_box_size = 72;
      low_threshold = 2.0;
      high_threshold = 8.0;
      max_blur = 10;		// this is the biggest star that
				// find_match will try
      hyperbola_C = 64.0; break;

    case SC_268M_Meade10:
      target_box_size = 150; // must be divisible by 3
      low_threshold = 10.0;
      high_threshold = 30.0;
      max_blur = 50;
      hyperbola_C = 15.06; break;

    case SC_ST9_C14_C63x:
      target_box_size = 72; // must be divisible by 3
      low_threshold = 2.0;
      high_threshold = 8.0;
      max_blur = 10;
      hyperbola_C = 64*5.5/10.0; break;
      
    case SC_268M_C14_Starizona:
      target_box_size = 180;
      low_threshold = 10.0;
      high_threshold = 30.0;
      max_blur = 50;
      hyperbola_C = 290.0*(3.9/20.0);
      break;

    case SC_ST9_C14_Starizona:
      target_box_size = 90; // must be divisible by 3
      low_threshold = 2.0;
      high_threshold = 8.0;
      max_blur = 10;
      hyperbola_C = 262.8; break;
      
    case SC_NONSTANDARD:
    default:
      fprintf(stderr, "focus: SystemConfig not recognized. Can't focus.\n");
      return;
    }
  }

  if (focuser_to_use == FOCUSER_COARSE) {
    hyperbola_C *= 120.3;
    MIN_TRAVEL = 0;
    MAX_TRAVEL = 439000;
    MIN_SOLUTION = 1000;
    MAX_SOLUTION = 438000;
  } else {
    target_box_size *= 3;
  }
  
  // get rid of any pre-existing result (param) file
  (void) unlink(PARAM_FILE_PATH);

  focus_filter = filter;	// remember the filter to use

  if (!inhibit_plotting) {
    fp_plot = popen("/home/mark/ASTRO/CURRENT/TOOLS/FOCUS/focus_plot3.py", "w");
    if (!fp_plot) {
      fprintf(stderr, "focus: unable to open plotter's pipe.\n");
      inhibit_plotting = 1;
    }
  }

  double good_focus = initial_encoder;

  // Create the imaging thread, which will wait until we put an
  // exposure request into the request queue.
  pthread_t imaging_thread;
  if (pthread_create(&imaging_thread,
		     NULL,	// attributes
		     &exposer_thread,
		     &exposure_time_val)) {
    fprintf(stderr, "Error creating imaging thread.\n");
  }

  // Get an image and do an initial star assessment
  exposure_flags flags("focus");
  if (initial_image == 0) {
    flags.SetFilter(focus_filter);
    flags.SetFilter(focus_filter);
    char *this_image_filename = expose_image_next(exposure_time_val, flags, "FOCUS_FIND");
    initial_image = new Image(this_image_filename);
  }
  //image_scale = config->PixelScale();
  //MAX_STARSIZE = (12.0/image_scale); // 12 arcsec is a pretty fat star

  if(dark_image) initial_image->subtract(dark_image);
  adjust_box(initial_image);
  fprintf(logfile, "Box set; left = %d, right = %d, top = %d, bottom = %d\n",
	  box_left, box_right, box_top, box_bottom);

  // Create initial population of requests
  const double span = max_blur*hyperbola_C;
  const static int NUM_STEPS = 11; // best kept odd
  const int delta = (int) (0.5 + span/NUM_STEPS);
  for (int i=0; i<NUM_STEPS; i++) {
    const int target_focus = good_focus + (i-NUM_STEPS/2)*delta;
    ExposureRequest *r = new ExposureRequest;
    r->quantity = 1;
    r->focus_encoder = target_focus;
    r->is_composite = false;
    r->image_filename = nullptr;
    r->corresponding_composite = 0;
    ScheduleExposure(r);
  }
  PromotePendingRequests();

  // Loop until we meet successful focus completion criteria
  int max_cycles = 7;
  do {
    ResultSummary results;
    
    FetchAndProcessExposures(&good_focus);

    if (user_abort_requested() || user_aborted) {
      fprintf(stderr, "Halting due to user-requested 'quit'\n");
      break;
    }
    
    // Calling assess_results will populate "results"
    assess_results(&results, good_focus);

    // Need stuff
    bool ready_to_quit = true; // might not be true
    if (results.useful_on_low_side < 3) {
      create_requests(3 - results.useful_on_low_side,
		      good_focus - (max_blur/2)*hyperbola_C,
		      good_focus - 2*hyperbola_C);
      ready_to_quit = false;
    }
    if (results.useful_on_high_side < 3) {
      create_requests(3 - results.useful_on_high_side,
		      good_focus + 2*hyperbola_C,
		      good_focus + (max_blur/2)*hyperbola_C);
      ready_to_quit = false;
    }
    if (results.useful_near_focus < 3) {
      create_requests(3 - results.useful_near_focus,
		      good_focus - 2*hyperbola_C,
		      good_focus + 2*hyperbola_C);
      ready_to_quit = false;
    }

    if (ready_to_quit) {
      //good_focus = DoFineFocus(good_focus);
      
      long final_focus = set_focus(good_focus);
      fprintf(stderr, "Coarse focus set to %ld\n", final_focus);
      fprintf(logfile, "Coarse focus set to %ld\n", final_focus);

      FILE *param = fopen(PARAM_FILE_PATH, "w");
      fprintf(param, "Focus = %d\n", (int) (good_focus + 0.5));
      fclose(param);

      break; // done!
    } else if (results.number_bad > 6) {
      fprintf(stderr, "Too many bad measurements. Terminating.\n");
      fprintf(stderr, "Resetting focus back to %ld\n",
	      initial_encoder);
      long final_focus = set_focus(initial_encoder);
      fprintf(stderr, "Focus set to %ld\n", final_focus);
      fprintf(logfile, "Too many bad measurements. Terminating.\n");
      fprintf(logfile, "Resetting focus back to %ld\n",
	      initial_encoder);
      fprintf(logfile, "Focus set to %ld\n", final_focus);
      break; // done, but unhappy
    } else {
      PromotePendingRequests();
    }
  } while(--max_cycles); // normal exit from loop with "break"

  // Preserve (log) the results
  {
    std::list <OneMeasurement *>::iterator it;
    for (it=all_measurements.begin(); it != all_measurements.end(); it++) {
      OneMeasurement *m = (*it);
      fprintf(logfile, "%s: ticks = %d, blur = %.3lf, %s\n",
	      m->image_filename,
	      m->focus_encoder,
	      m->measured_focus,
	      (m->is_composite ? "<composite>" : ""));
    }
  } // end of printing loop

  // tell the exposure thread to quit
  ExposureRequest r_quit;
  r_quit.quantity = -1; // the signal to quit
  ScheduleExposure(&r_quit);
  PromotePendingRequests();
  // ...and wait for the exposure thread to actually terminate
  if (pthread_join(imaging_thread, NULL)) {
    fprintf(stderr, "Error in thread rendezvous with imaging thread.\n");
  }

  long final_focus = set_focus(good_focus);
  fprintf(stderr, "Final coarse focus set to %ld\n", final_focus);
  fprintf(logfile, "Final coarse focus set to %ld\n", final_focus);
}

#else
int
focus(double exposure_time_val,
      long initial_encoder,
      int focus_time,
      Image *dark_image,
      Filter filter) {

  focus_filter = filter;	// remember the filter to use

  // Get an image and do an initial star assessment
  exposure_flags flags;
  flags.SetFilter(filter);
  flags.SetFilter(focus_filter);
  flags.SetReadoutMode(1);
  flags.SetBinning(1);
  flags.SetOffset(5);
  flags.SetGain(56);
  flags.SetOutputFormat(exposure_flags::E_uint32);
  const int num_points_pass_1 = 21;
  int delta_encoder = 20;

  int starting_point = initial_encoder - delta_encoder * num_points_pass_1/2;
  fprintf(stderr, "Moving focuser to initial position at %d\n",
	  starting_point);
  set_focus(starting_point);
  
  char *this_image_filename = expose_image(exposure_time_val, flags, "FOCUS_FIND");
  Image this_image(this_image_filename);
  if(dark_image) this_image.subtract(dark_image);
  adjust_box(&this_image);

  fprintf(stdout, "Getting first 5 points\n");

  for (int i=0; i<num_points_pass_1; i++) {
    if (i != 0) scope_focus(delta_encoder, FOCUSER_MOVE_RELATIVE, focuser_to_use);
    GetPoints(CumFocusPosition(focuser_to_use),
	      1, // cycle_count user entry being overridden
	      exposure_time_val);
  }

  Hyperbola h(initial_encoder);
  h.SetC(hyperbola_C);
  h.reset(initial_encoder);
  double best_guess;
  if ((best_guess = h.Solve(&run_data)) < 0.0) {
    fprintf(stderr, "focus: hyperbola failed. Don't know what to do.\n");
    exit(-1);
  } else {
    best_guess = h.GetTicks();
    fprintf(stderr, "focus: predicting good focus is at %lf\n",
	    best_guess);
  }

  delta_encoder = 8;
  const int num_points_pass_2 = 10;
  starting_point = best_guess - delta_encoder*num_points_pass_2/2;
  set_focus(starting_point);

  for (int i=0; i<num_points_pass_2; i++) {
    if (i != 0) scope_focus(delta_encoder, FOCUSER_MOVE_RELATIVE, focuser_to_use);
    GetPoints(CumFocusPosition(focuser_to_use),
	      1, // cycle_count,
	      exposure_time_val);
  }

  h.reset();

  FILE *param = fopen(PARAM_FILE_PATH, "w");
  
  if ((best_guess = h.Solve(&run_data)) < 0.0) {
    fprintf(stderr, "focus: hyperbola failed. Don't know what to do\n");
    fprintf(param, "Focus = -1\n");
  } else {
    best_guess = h.GetTicks();
    fprintf(stderr, "focus: final prediction is %lf\n", best_guess);
    set_focus((long) best_guess);
    fprintf(param, "Focus = %d\n", (int) (best_guess + 0.5));
  }
  fclose(param);
  return 0;
}
#endif

//****************************************************************
//        FetchAndProcessExposures
//****************************************************************
void FetchAndProcessExposures(double *current_estimate) {

  // order is sensitive here to prevent funny race condition
  while (request_list.size() || camera_is_busy || request_done.size()) {

    // lock the "request_done" list and wait for something to show up
    // there.
    pthread_mutex_lock (&mutex_done);
    while (request_done.size() == 0) {
      pthread_cond_wait (&finished_lock, &mutex_done);
    }

    ExposureRequest *r = request_done.front();
    request_done.pop_front();
    pthread_mutex_unlock (&mutex_done);

    // make sure we have a valid image filename
    char *this_image_filename = r->image_filename;
    assert (r->image_filename);
    assert (r->image_filename[0]);

    if (r->is_composite == false) {
      // kick off the analysis of the image
      char command_buffer[256];
      sprintf(command_buffer, "/home/mark/ASTRO/CURRENT/TOOLS/FOCUS_MODEL/find_match -m %d -i %s -s -g 0.5 | fgrep AnswerBlur > /tmp/focus.out",
	      max_blur, this_image_filename);
      int return_value = system(command_buffer);
      fprintf(stderr, "find_match command returned %d\n", return_value);
      
      FILE *fp = fopen("/tmp/focus.out", "r");
      if (!fp) {
	fprintf(stderr, "Error: cannot open /tmp/focus.out\n");
      } else {
	double this_blur = -1.0;
	if (fscanf(fp, "AnswerBlur %lf", &this_blur) != 1) {
	  fprintf(stderr, "Cannot parse output of find_match.\n");
	}
	fprintf(stderr, "focus = %d, blur = %lf\n",
		(int) r->focus_encoder, this_blur);
      
	OneMeasurement *m = new OneMeasurement;
	m->focus_encoder = r->focus_encoder;
	m->image_filename = strdup(r->image_filename);
	m->num_exposures = 1;
	m->measured_focus = this_blur;
	r->result = m;

	if (this_blur > 0.0 && this_blur <= max_blur) {
	  m->is_composite = false;
	  m->composite = 0;
	  all_measurements.push_back(m);
	  run_data.add(m->focus_encoder, this_blur);
	  if (fp_plot) {
	    fprintf(fp_plot, "point %d %lf\n",
		    m->focus_encoder, this_blur);
	    fflush(fp_plot);
	  }
	} else {
#if 0
	  // measurement failed, so we initiate a composite
	  m->is_composite = true;
	  m->composite = new CompositeImage(100, 100);
	  Image *image = new Image(this_image_filename);
	  int num_to_grab_for_composite = 4;
	  if (add_image(m->composite, image) == false) {
	    // bad image; will want an additional contribution to the
	    // composite.
	    num_to_grab_for_composite++;
	  }
	  delete image;

	  // schedule 4 exposures
	  for (int i = 0; i < num_to_grab_for_composite; i++) {
	    ExposureRequest *rr = new ExposureRequest;
	    rr->quantity = 1;
	    rr->focus_encoder = r->focus_encoder;
	    rr->is_composite = true;
	    rr->image_filename = 0;
	    rr->corresponding_composite = m;
	    ScheduleExposure(rr);
	  }
	  // Note that if measurement failed, we created a
	  // OneMeasurement for the composite, but we didn't put it
	  // into all_measurements. That will happen later when the
	  // composite is finished.
#endif
	  fprintf(stderr, "measurement of %.2lf exceeds max_blur (%d). Skipping.\n",
		  this_blur, max_blur);
	} // end if measurement failed
	fclose(fp);
	unlink("/tmp/focus.out");
      }
    } else {
      // we just received an image intended to be part of a composite
      OneMeasurement *m = r->corresponding_composite;
      Image *image = new Image(r->image_filename);
      m->num_exposures++;
      (void) add_image(m->composite, image);
      delete image;

      // once we've accumulated enough images, analyze the composite
      if (m->num_exposures > 4) {
	m->composite->WriteFITS("/tmp/composite.fits");
	char command_buffer[256];
	sprintf(command_buffer, "/home/mark/ASTRO/CURRENT/TOOLS/FOCUS_MODEL/find_match -i /tmp/composite.fits -s -g 5.0 | fgrep AnswerBlur > /tmp/focus.out");

	int return_value = system(command_buffer);
	fprintf(stderr, "find_match command returned %d\n", return_value);
      
	FILE *fp = fopen("/tmp/focus.out", "r");
	if (!fp) {
	  fprintf(stderr, "Error: cannot open /tmp/focus.out\n");
	} else {
	  double this_blur = -1.0;
	  if (fscanf(fp, "AnswerBlur %lf", &this_blur) != 1) {
	    fprintf(stderr, "Cannot parse output of find_match.\n");
	  }
	  fprintf(stderr, "focus = %d, blur = %lf (factor of 10)\n",
		  (int) m->focus_encoder, this_blur);
      
	  if (this_blur > 0.0 && this_blur <= max_blur) {
	    // scale back by factor of 10 since that was our expansion
	    // when we set up the composite
	    m->measured_focus = this_blur/10.0;
	    all_measurements.push_back(m);
	    run_data.add(m->focus_encoder, m->measured_focus);
	    if (fp_plot) {
	      fprintf(fp_plot, "point %d %lf\n",
		      m->focus_encoder, m->measured_focus);
	      fflush(fp_plot);
	    }
	  } else {
	    fprintf(stderr, "Warning: composite image did not yield good blur measurement!\n");
	  }
	  fclose(fp);
	  unlink("/tmp/focus.out");
	}
      }
    }
  }
  // now that all exposures have completed, try doing a hyperbola
  // fitting.
  Hyperbola h(*current_estimate);
  h.reset(*current_estimate);
  h.SetC(hyperbola_C);
  (void) h.Solve(&run_data);
  double next_guess = h.state_var[HYPER_R];
  if (h.NoSolution()) {
    fprintf(stderr, "focus: hyperbola failed. Randomly adding a point.\n");
    create_requests(1, (*current_estimate)-100, (*current_estimate)+100);
  } else {
    // hyperbola converged
    fprintf(stderr, "focus: updated focus prediction = %lf\n",
	    next_guess);
    *current_estimate = next_guess;
    if (fp_plot) {
      fprintf(fp_plot, "curve %lf %lf %lf\n",
	      h.state_var[0],
	      hyperbola_C*h.state_var[0],
	      next_guess);
      fflush(fp_plot);
    }
  }
}
    
//****************************************************************
//        make an exposure (done within the exposer_thread)
//****************************************************************
void do_exposer(double exposure_time, ExposureRequest *r) {
  exposure_flags flags("focus");
  flags.SetFilter(focus_filter);
  flags.SetFilter(focus_filter);
  flags.subframe.box_bottom = box_bottom;
  flags.subframe.box_top    = box_top;
  flags.subframe.box_left   = box_left;
  flags.subframe.box_right  = box_right;
  
  char *this_image_filename = expose_image(exposure_time,
					   flags,
					   "FOCUS");
  r->image_filename = strdup(this_image_filename);
}

  
//****************************************************************
//        This is the main loop of the exposure thread.
//****************************************************************
void *exposer_thread(void *exposure_time) {
  const double exp_time = * (double *)exposure_time;

  do {
    // wait for (and fetch) the next request
    pthread_mutex_lock (&mutex_request);
    while (request_list.size() == 0) {
      pthread_cond_wait (&request_lock, &mutex_request);
    }

    ExposureRequest *r = request_list.front();
    camera_is_busy = 1;
    request_list.pop_front();

    pthread_mutex_unlock (&mutex_request);

    // initiate exit from the pthread
    if (r->quantity < 0 || user_aborted) break;
    r->focus_encoder = set_focus(r->focus_encoder);
    fprintf(stderr, "exposer_thread set focuser to %d\n", r->focus_encoder);
    // make the exposure
    do_exposer(exp_time, r);

    // put onto the request_done list
    pthread_mutex_lock (&mutex_done);
    request_done.push_back(r);
    camera_is_busy = 0;
    pthread_cond_signal (&finished_lock);
    pthread_mutex_unlock (&mutex_done);
  } while (1);

  // all done
  return 0;
}

//****************************************************************
//        add_image()
// add_image() adds an image to a composite image that is under
// "construction." add_image() will return "true" if the addition was
// successful (if a star was found), and will return "false" if no
// star center could be established.
//****************************************************************
bool add_image(CompositeImage *composite_image,
	       Image *i) {
  const double background = i->HistogramValue(0.5);

  double max_x = 0.0;
  double max_y = 0.0;
  double brightest = 0.0;

  for (int row = 0; row < i->height; row++) {
    for (int col = 0; col < i->width; col++) {
      if (i->pixel(col, row) > brightest) {
	brightest = i->pixel(col, row);
	max_x = col;
	max_y = row;
      }
    }
  }

  const double limit = 18; // radius in pixels
  for (int loop = 0; loop < 10; loop++) {
    double offset_x = 0.0;
    double offset_y = 0.0;
    double pix_sum = 0.0;

    for (int row = 0; row < i->height; row++) {
      for (int col = 0; col < i->width; col++) {
	const double del_x = (col + 0.5) - max_x;
	const double del_y = (row + 0.5) - max_y;
	const double del_r = sqrt(del_x*del_x + del_y*del_y);

	if (del_r < limit) {
	  const double pix = i->pixel(col, row) - background;
	  offset_x += pix*del_x;
	  offset_y += pix*del_y;
	  pix_sum += pix;
	}
      }
    }
    fprintf(stderr, "trial x,y @ (%lf,%lf): offset_x = %lf, offset_y = %lf\n",
	    max_x, max_y, offset_x, offset_y);
    max_x = max_x + offset_x/pix_sum;
    max_y = max_y + offset_y/pix_sum;

    // make sure that we haven't fallen off the edge of the image
    // frame
    if ((!isnormal(max_x)) || (!isnormal(max_y)) ||
	max_x < 0.0 || max_y < 0.0 ||
	max_x >= i->width || max_y >= i->height) {
      return false;
    }
  }

  const double center_x = max_x;
  const double center_y = max_y;

  composite_image->AddStarToComposite(i, center_x, center_y);
  return true;
}
  
void create_requests(int num_requests, int low_limit, int high_limit) {
#if 0
  if (low_limit < MIN_TRAVEL) low_limit = MIN_TRAVEL;
  if (high_limit > MAX_TRAVEL) high_limit = MAX_TRAVEL;
#endif
  int range_delta = (high_limit - low_limit);
#if 0
  if (range_delta < 5 && high_limit + 5 > MAX_TRAVEL) {
      low_limit = high_limit - 5;
      range_delta = (high_limit - low_limit);
  }
  if (range_delta < 5 && low_limit - 5 < MIN_TRAVEL) {
      high_limit = low_limit + 5;
      range_delta = (high_limit - low_limit);
  }
#endif
  while (num_requests-- > 0) {
    const unsigned long r_value = 1733UL * (unsigned long) rand();
    int focus_value = low_limit + (r_value % range_delta);
    fprintf(stderr, "limits [%d -> %d]; selected %d\n", low_limit, high_limit, focus_value);
    
    //if (focus_value < 1) focus_value = 1;
    ExposureRequest *r = new ExposureRequest;
    r->quantity = 1;
    r->focus_encoder = focus_value;
    r->is_composite = false;
    r->image_filename = 0;
    r->corresponding_composite = 0;
    ScheduleExposure(r);
  }
}

// scan all resuls so far and put the category counts into "results"
void assess_results(ResultSummary *results, double focus_estimate) {
  results->number_bad = 0;
  results->useful_on_high_side = 0;
  results->useful_on_low_side = 0;
  results->useful_near_focus = 0;

  std::list <OneMeasurement *>::iterator it;
  for (it = all_measurements.begin(); it != all_measurements.end(); it++) {
    bool is_good = false;
    OneMeasurement *m = (*it);
    const double delta_focus = m->focus_encoder - focus_estimate;

    if (m->measured_focus > low_threshold && m->measured_focus < high_threshold) {
      if (delta_focus < 0.0) {
	results->useful_on_low_side++;
	is_good = true;
      } else {
	results->useful_on_high_side++;
	is_good = true;
      }
    } else if (m->measured_focus <= low_threshold && m->measured_focus > 0.0) {
      results->useful_near_focus++;
      is_good = true;
    }
    // if it wasn't any good, it must have been bad
    if (!is_good) {
      results->number_bad++;
    }
  }
  fprintf(stderr, "assess_results: %d good on low, %d good on high, %d good near focus, %d bad\n",
	  results->useful_on_low_side,
	  results->useful_on_high_side,
	  results->useful_near_focus,
	  results->number_bad);
}
    
void
do_special_test(void) {
  const int num_images = 5;
  CompositeImage *ci = new CompositeImage(100, 100);
  const char *filenames[] = { "/home/IMAGES/7-10-2015/image130.fits",
			      "/home/IMAGES/7-10-2015/image131.fits",
			      "/home/IMAGES/7-10-2015/image132.fits",
			      "/home/IMAGES/7-10-2015/image133.fits",
			      "/home/IMAGES/7-10-2015/image134.fits" };

  for (int i=0; i<num_images; i++) {
    Image *image = new Image(filenames[i]);
    add_image(ci, image);
    delete image;
  }

  ci->WriteFITS("/tmp/focus_image.fits");
  char command_buffer[256];
  sprintf(command_buffer, "/home/mark/ASTRO/CURRENT/TOOLS/FOCUS_MODEL/find_match -i %s -s -g 5.0 | fgrep AnswerBlur > /tmp/focus.out",
	  "/tmp/focus_image.fits");
  int return_value = system(command_buffer);
  fprintf(stderr, "find_match command returned %d\n", return_value);
      
  FILE *fp = fopen("/tmp/focus.out", "r");
  if (!fp) {
    fprintf(stderr, "Error: cannot open /tmp/focus.out\n");
  } else {
    double this_blur = -1.0;
    if(fscanf(fp, "AnswerBlur %lf", &this_blur) != 1) {
      fprintf(stderr, "Cannot parse output of find_match.\n");
    }
    fprintf(stderr, "focus = %d, blur = %lf\n",
	    (int) 1198, this_blur);
  }
}

double DoFineFocus(double coarse_focus) {
  const int fine_focus_range = 80*(hyperbola_C/64.0); // offset either side of best focus
  constexpr int num_fine_focus_points = 12;

  fprintf(stderr, "Starting DoFineFocus() with coarse estimate of %.1lf\n",
	  coarse_focus);

  // Generate new point requests
  create_requests(num_fine_focus_points,
		  coarse_focus - fine_focus_range,
		  coarse_focus + fine_focus_range);

  // Grab all existing points that can be reused as "fine" points
  for (auto m : all_measurements) {
    m->is_fine_focus = (fabs(m->focus_encoder - coarse_focus) < fine_focus_range and
			m->measured_focus < 3.0);
    if (m->is_fine_focus) m->measured_focus = -1.0; // so we know to reprocess later
  }

  // order is sensitive here to prevent funny race condition
  while (request_list.size() || camera_is_busy || request_done.size()) {

    // lock the "request_done" list and wait for something to show up
    // there.
    pthread_mutex_lock (&mutex_done);
    while (request_done.size() == 0) {
      pthread_cond_wait (&finished_lock, &mutex_done);
    }

    ExposureRequest *r = request_done.front();
    request_done.pop_front();
    pthread_mutex_unlock (&mutex_done);

    // make sure we have a valid image filename
    assert (r->image_filename);
    assert (r->image_filename[0]);

    OneMeasurement *m = new OneMeasurement;
    m->focus_encoder = r->focus_encoder;
    m->image_filename = strdup(r->image_filename);
    m->num_exposures = 1;
    m->measured_focus = FineMeasure(m->image_filename);
    m->is_composite = false;
    m->is_fine_focus = true;
    m->composite = 0;
    r->result = m;
    all_measurements.push_back(m);

    if (fp_plot) {
      fprintf(fp_plot, "point %d %lf\n",
	      m->focus_encoder, m->measured_focus);
      fflush(fp_plot);
    }
  } // all exposures have been processed


  bool repeat = false;
  double a, b, c;
  do {
    SolveParabola(&a, &b, &c);

    // compute stddev
    double sum_err_sq = 0.0;
    int num = 0;
    for (auto m : all_measurements) {
      if (m->is_fine_focus) {
	double model = a*m->focus_encoder*m->focus_encoder + b*m->focus_encoder + c;
	m->err = m->measured_focus - model;
	sum_err_sq += (m->err*m->err);
	num++;
      }
    }
    repeat = false;
    const double stddev = sqrt(sum_err_sq/num);
    for (auto m : all_measurements) {
      if (m->is_fine_focus and fabs(m->err) > 2.0*stddev) {
	cerr << "Deleting apparent bad point: "
	     << m->focus_encoder << ": "
	     << m->measured_focus << endl;
	m->is_fine_focus = false;
	repeat = true;
      }
    }
  } while(repeat);

  double min = -b/(2.0*a);
  fprintf(stderr, "Fine focus minimum at %.1lf\n", min);
  return min;
}

int find_blob(Image &i, double *x_center, double *y_center) {
  double brightest = -99e99;
  *x_center = *y_center = -1.0;
  
  for (int x=0; x<i.width; x++) {
    for (int y=0; y<i.height; y++) {
      const double v = i.pixel(x,y);
      if (v > brightest) {
	brightest = v;
	*x_center = x;
	*y_center = y;
	//cout << "Center set to ("
	//   << *x_center << ", "
	//   << *y_center << ")\n";
      }
    }
  }
  constexpr int offset = 10;
  int subimage_left = (*x_center) - offset;
  int subimage_right = (*x_center) + offset;
  int subimage_bottom = (*y_center) - offset;
  int subimage_top = (*y_center) + offset;

  //cout << "box = [" << subimage_left << ", "
  //   << subimage_right << "] x ["
  //   << subimage_bottom << ", "
  //   << subimage_top << "]\n";

  double centroid_x = 0.0;
  double centroid_y = 0.0;
  double sum_pixels = 0.0;
  for (int x=subimage_left; x<subimage_right; x++) {
    for (int y=subimage_bottom; y < subimage_top; y++) {
      if (x < 0 or y < 0 or x >= i.width or y >= i.height) continue;
      const double v = i.pixel(x,y);
      const double del_x = x - *x_center;
      const double del_y = y - *y_center;
      centroid_x += (del_x * v);
      centroid_y += (del_y * v);
      sum_pixels += v;
    }
  }
  double x_adj = centroid_x/sum_pixels;
  double y_adj = centroid_y/sum_pixels;
  //cout << "centroid adjustment = ("
  //   << x_adj << ", " << y_adj << ")\n";
  *x_center += x_adj;
  *y_center += y_adj;
  return 0;
}

double FineMeasure(const char *filename) {
  Image image(filename);
  double x_center, y_center;
  const double median = image.statistics()->MedianPixel;

  find_blob(image, &x_center, &y_center);

  constexpr double max_r = 10;
  GRunData points;
  points.reset();
  
  for (int x=(int)(x_center - max_r); x <= (int)(x_center+max_r); x++) {
    for (int y=(int)(y_center - max_r); y <= (int)(y_center+max_r); y++) {
      const double value = image.pixel(x,y) - median;
      const double del_x = (x_center - x);
      const double del_y = (y_center - y);
      const double r = sqrt(del_x*del_x + del_y*del_y);
      if (r < max_r) {
	//cout << "Adding point: r = " << r
	//   << ", value = " << value << endl;
	points.add(del_x, del_y, value);
      }
    }
  }

  Gaussian g;
  g.reset();
  int status = nlls_gaussian(&g, &points);
  cout << "Status = " << (status == 0 ? "Okay" : "No converge") << endl;
  cout << "Scaling = " << g.state_var[0] << endl;
  cout << "Shape = " << g.state_var[1] << endl;
  return g.state_var[1];
}  

void SolveParabola(double *a, double *b, double *c) {
  int num_fine_measurements = 0;
  for (auto m : all_measurements) {
    if (m->is_fine_focus and m->measured_focus < 0.0) {
      m->measured_focus = FineMeasure(m->image_filename);
    }
    if (m->is_fine_focus) num_fine_measurements++;
  }

  fprintf(stderr, "Num_fine_measurements = %d\n", num_fine_measurements);

  // Now find the best parabola to fit these points
  gsl_matrix *X = gsl_matrix_alloc(num_fine_measurements, 3);
  gsl_vector *y = gsl_vector_alloc(num_fine_measurements);
  gsl_vector *q = gsl_vector_alloc(3);
  gsl_matrix *cov = gsl_matrix_alloc(3, 3);

  int i=0;
  for (auto m: all_measurements) {
    if (m->is_fine_focus) {
      gsl_matrix_set(X, i, 0, 1.0);
      gsl_matrix_set(X, i, 1, m->focus_encoder);
      gsl_matrix_set(X, i, 2, m->focus_encoder*m->focus_encoder);
      gsl_vector_set(y, i, m->measured_focus);
      i++;
      cout << "Fine point: " << m->focus_encoder
	   << ", " << m->measured_focus << endl;
    }
  }

  double chisq;
  gsl_multifit_linear_workspace *work = gsl_multifit_linear_alloc(num_fine_measurements, 3);
  gsl_multifit_linear(X, y, q, cov, &chisq, work);
  gsl_multifit_linear_free(work);

  *a = gsl_vector_get(q, 2);
  *b = gsl_vector_get(q, 1);
  *c = gsl_vector_get(q, 0);
}  

double aperture_sum(double centerx, double centery, Image &image) {
  ImageInfo *info = image.GetImageInfo();
  double pixel_scale = 1.52; // arcsec per pixel default
  if (info && info->CDeltValid()) {
    pixel_scale = info->GetCDelt1();
  }

  
  // 8 arcsecond radius should be plenty
  const double radius = 6.0/pixel_scale;
  const double radius_sq = radius*radius;
  int pixel_count = 0;
  double sum = 0.0;

  for (int del_y=-radius; del_y<radius; del_y++) {
    for (int del_x=-radius; del_x<radius; del_x++) {
      const double d_sq = (del_x*del_x + del_y*del_y);
      if (d_sq <= radius_sq) {
	sum += image.pixel(del_x+(int)(centerx+0.5),
			   del_y+(int)(centery+0.5));
	pixel_count++;
      }
    }
  }

  return sum/pixel_count;
}
      
