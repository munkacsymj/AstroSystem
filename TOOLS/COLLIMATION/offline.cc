#include <gsl/gsl_vector.h>
#include <gsl/gsl_permutation.h>
#include <gsl/gsl_linalg.h>
#include <gsl/gsl_matrix.h>
#include <unistd.h>
#include <stdio.h>
#include <math.h>
#include <list>

const int FS_A     = 2;		// A (blur at perfect focus)
const int FS_B     = 3;		// B (A/B = slope as deviate from
				// perfect focus)
const int FS_X0    = 1;		// focus error at t0
const int FS_R0    = 0;		// slope of focus change vs time

int order = 4;
const int MAX_ORDER = 4;
int constrained_FS_B = 104.5;	// also used as the initial value,
				// even when not constrained

void usage(void) {
  fprintf(stderr,
	  "usage: offline [-a] [-b] [-f nnn] < analyze_composite.out\n");
  fprintf(stderr, "        -b    Constrain param B to fixed value.\n");
  fprintf(stderr, "        -a    Constrain param A to fixed value.\n");
  fprintf(stderr, "        -f nn Use explicit focus in/out adjust param.\n");
  exit(-2);
}

class focus_state {
public:
  double state_var[MAX_ORDER];
  double mel;
};

struct obs_data {
  int N;
  double *yi;			// blur
  double *ti;			// time
  double *err;			// errors
  double **t;			// partial derivatives
  double *raw_focus;
  double *focus_pos;		//
};

struct one_obs {
  double blur;
  double t;			// zero time is at the beginning of
				// the day's run
  double raw_focus;
  double focus_pos;
};

typedef std::list<one_obs *> obs_list;

// solve() returns 0 on success, -1 on failure
int solve(obs_list *d,
	  double t1, // current time
	  
	  focus_state *state);

double focus_ratio = 1.0;

/*
 * input file format:
 * time,blur,focus
 *
 */
obs_list *read_all_obs(FILE *fp) {
  obs_list *all_list = new obs_list;
  double last_raw_focus = 0.0;
  double last_adjusted_focus = 0.0;
  double baseline_t0 = 0.0;
  bool first_obs = true;
  
  while(!feof(fp)) {
    char buffer[256];

    if(fgets(buffer, sizeof(buffer), fp)) {
      double t, blur, raw_focus;
      int num_stars;
      double smear;
      if(sscanf(buffer, "%lf,%lf,%lf, %d, %lf",
		&t, &raw_focus, &blur, &num_stars, &smear) == 5) {

	if (blur < 1.0 || smear > 0.33) continue;

	if(first_obs) {
	  first_obs = false;
	  last_raw_focus = raw_focus;
	  last_adjusted_focus = 0;
	  baseline_t0 = t;
	}

	struct one_obs *oo = new one_obs;
	oo->blur = blur;
	oo->t = t - baseline_t0;
	oo->raw_focus = raw_focus;
	all_list->push_back(oo);

	double delta_pos = raw_focus - last_raw_focus;
	if(delta_pos <= 0) last_adjusted_focus += delta_pos;
	else last_adjusted_focus += delta_pos * focus_ratio;
	oo->focus_pos = last_adjusted_focus;
	last_raw_focus = raw_focus;

	fprintf(stderr, "t= %lf, focus= %lf, pos= %lf\n",
		oo->t, oo->blur, oo->focus_pos);
      }
    }
  }

  return all_list;
}

void compute_partials(obs_data *all, double t1, focus_state *state) {
  const double var_A = state->state_var[FS_A];
  const double var_B = state->state_var[FS_B];
  const double var_X0 = state->state_var[FS_X0];
  const double var_R0 = state->state_var[FS_R0];

  for(int k=0; k<all->N; k++) {
    const double pos_err = all->focus_pos[k] - (var_X0 + var_R0 * (all->ti[k]-t1));
    const double factor = 1.0 + pos_err*pos_err/(var_B*var_B);
    const double sqrt_factor = sqrt(factor);

    const double modeled_value = var_A * sqrt_factor;

    all->err[k] = all->yi[k] - modeled_value;
    // partial derivative wrt A
    all->t[FS_A][k] = sqrt_factor;
    //partial derivative wrt B
    all->t[FS_B][k] = (-var_A/(var_B*var_B*var_B))*pos_err*pos_err/sqrt_factor;
    //partial derivative wrt X0
    all->t[FS_X0][k] = -(var_A/(var_B*var_B))*pos_err/sqrt_factor;
    // partial derivative wrt R0
    all->t[FS_R0][k] = all->t[FS_X0][k] * all->ti[k];

    fprintf(stderr, "act_blur = %.2lf, model_blur = %.2lf\n",
	    all->yi[k], modeled_value);

    fprintf(stderr, "err = %.2lf, partials = %lf, %lf, %lf, %lf\n",
    	     all->err[k], all->t[FS_A][k], all->t[FS_B][k], all->t[FS_X0][k],
    	     all->t[FS_R0][k]);
  }
  fprintf(stderr, "----------\n");
}
  

int main(int argc, char **argv) {
  int option_char;
  bool constrain_a = false;
  bool constrain_b = false;

  while((option_char = getopt(argc, argv, "abf:")) > 0) {
    switch (option_char) {
    case 'a':
      constrain_a = true;
      break;

    case 'b':
      constrain_b = true;
      break;

    case 'f':
      sscanf(optarg, "%lf", &focus_ratio);
      break;

    case '?':
    default:
      fprintf(stderr, "Invalid argument.\n");
      usage();
    }
  }
  
  fprintf(stderr, "Using focus ratio of %lf\n", focus_ratio);

  // Sort out constraints
  if (constrain_a && constrain_b) {
    order = 2;
  } else if (constrain_a) {
    fprintf(stderr, "Error: cannot constrain A without also constraining B\n");
    usage();
  } else if (constrain_b) {
    order = 3;
  } else {
    order = 4;
  }

  obs_list *all_obs = read_all_obs(stdin);
  obs_list::iterator it;
  obs_list *all_so_far = new obs_list;

  for (it = all_obs->begin(); it != all_obs->end(); it++) {
    // all_so_far holds all the observations up to the current one
    all_so_far->push_back(*it);
    double time_now = (*it)->t; // known elsewhere as t1
    double current_focus = (*it)->focus_pos;

    // pick off the most recent hour's worth (but make sure there's at
    // least 10, if 10 exist)
    obs_list *recent = new obs_list;
    obs_list::iterator it0;
    const double distance_back = 12.0/24.0; // twelve hour
    it0 = all_so_far->end();
    do {
      it0--;
      if ((*it0)->t >= (time_now - distance_back)) {
	recent->push_front(*it0);
      } else if (recent->size() < 10) {
	recent->push_front(*it0);
      } else {
	break;
      }
    } while (it0 != all_so_far->begin());

    focus_state state;

    const int result = solve(recent, time_now, &state);
    if (result == 0) {
      printf("%.4lf, %.3lf, %.0lf, %.1lf, %.1lf\n",
	     time_now, state.state_var[FS_A], current_focus,
	     state.state_var[FS_X0], state.state_var[FS_R0]);
    } else {
      printf("%.4lf, no solution\n", time_now);
    }
    delete recent;
  }

} // end main()

int solve(obs_list *d,
	  double t1, //current time
	  focus_state *state) {
  
  obs_list::iterator it;
  const int count = d->size();
  obs_data *all = new obs_data;
  all->N = count;
  all->yi = new double[count];
  all->ti = new double[count];
  all->err = new double[count];
  all->t = new double * [MAX_ORDER];
  all->raw_focus = new double[count];
  all->focus_pos = new double[count];
  for(int j=0; j<MAX_ORDER; j++) all->t[j] = new double[count];

  int i = 0;
  for(it = d->begin(); it != d->end(); it++) {
    const one_obs *oo = (*it);
    all->yi[i] = oo->blur;
    all->ti[i] = oo->t;
    all->raw_focus[i] = oo->raw_focus;
    all->focus_pos[i] = oo->focus_pos;

    i++;
  }

  state->state_var[FS_A] = 1.6;
  state->state_var[FS_B] = 104.5;
  state->state_var[FS_X0] = 0.0;
  state->state_var[FS_R0] = 0.0;

  int loop_count = 0;
  bool clamped = false;

  gsl_set_error_handler_off();

  do {
    clamped = false;
    compute_partials(all, t1, state);
  
    gsl_matrix *matrix = gsl_matrix_calloc(order, order);
    if(matrix == 0) {
      fprintf(stderr, "gsl_matrix_calloc() returned 0\n");
      return -1;
    }

    gsl_vector *product = gsl_vector_calloc(order);
    if(product == 0) {
      fprintf(stderr, "gsl_vector_calloc() returned 0\n");
      return -1;
    }

    gsl_permutation *permutation = gsl_permutation_alloc(order);
    if(permutation == 0) {
      fprintf(stderr, "gsl_permutation_alloc() returned 0\n");
      return -1;
    }

    double err_sq = 0.0;

    for(int n=0; n < count; n++) {
      for(int b = 0; b < order; b++) {
	(*gsl_vector_ptr(product, b)) += all->t[b][n] * all->err[n];

	for(int c = b; c < order; c++) {
	  (*gsl_matrix_ptr(matrix, b, c)) += all->t[b][n] * all->t[c][n];
	}
      }

      err_sq += all->err[n] * all->err[n];
    }
    for(int b=0; b<order; b++) {
      for(int c = b+1; c < order; c++) {
	(*gsl_matrix_ptr(matrix, c, b)) = (*gsl_matrix_ptr(matrix, b, c));
      }
    }

    for(int b=0; b<order; b++) {
      for(int c = 0; c < order; c++) {
	fprintf(stderr, " %11.3lf", *gsl_matrix_ptr(matrix, c, b));
      }
      fprintf(stderr, "\n");
    }

    fprintf(stderr, "\n");
    for(int b=0; b<order; b++) {
      fprintf(stderr, " %12.4lf", *gsl_vector_ptr(product, b));
    }
    fprintf(stderr, "\n");
    
    int sig_num;

    if(gsl_linalg_LU_decomp(matrix, permutation, &sig_num)) {
      fprintf(stderr, "gsl_linalg_LU_decomp() failed.\n");
      return -1;
    }

    if(gsl_linalg_LU_svx(matrix, permutation, product)) {
      fprintf(stderr, "gsl_linalg_LU_svx() failed.\n");
      return -1;
    }

    gsl_matrix_free(matrix);
    gsl_permutation_free(permutation);

    double delta_a = 0.0;
    double delta_b = 0.0;
    double delta_x0 = 0.0;
    double delta_r0 = 0.0;

    delta_x0 = gsl_vector_get(product, FS_X0);
    delta_r0 = gsl_vector_get(product, FS_R0);
    if (order == 4) {
      delta_b = gsl_vector_get(product, FS_B);
    } else {
      delta_b = 0.0;
    }

    if (order >= 3) {
      delta_a = gsl_vector_get(product, FS_A);
    } else {
      delta_a = 0.0;
    }
      
    gsl_vector_free(product);

    state->mel = sqrt(err_sq/(all->N-2));

#if 0
    if(fabs(delta_b) > 0.1*state->state_var[FS_B]) {
      fprintf(stderr, "FS_B has been clamped.\n");
      if(delta_b < 0.0) {
	delta_b = -0.1*state->state_var[FS_B];
      } else {
	delta_b = 0.1*state->state_var[FS_B];
      }
    }
#endif

    fprintf(stderr, "delta_a = %lf\n", delta_a);
    fprintf(stderr, "delta_b = %lf\n", delta_b);
    fprintf(stderr, "delta_x0 = %lf\n", delta_x0);
    fprintf(stderr, "delta_r0 = %lf\n", delta_r0);

    state->state_var[FS_A] += delta_a;
    state->state_var[FS_B] += delta_b;
    state->state_var[FS_X0] += delta_x0;
    state->state_var[FS_R0] += delta_r0;

    if (state->state_var[FS_A] < 0.5) {
      state->state_var[FS_A] = 0.5;
      clamped = true;
    }
    if (state->state_var[FS_X0] < -500.0) {
      state->state_var[FS_X0] = -500.0;
      clamped = true;
    }
    if (state->state_var[FS_X0] > 500.0) {
      state->state_var[FS_X0] = 500.0;
      clamped = true;
    }

    fprintf(stderr, "Current values:\n");
    fprintf(stderr, "   A  = %lf\n", state->state_var[FS_A]);
    fprintf(stderr, "   B  = %lf\n", state->state_var[FS_B]);
    fprintf(stderr, "   X0 = %lf\n", state->state_var[FS_X0]);
    fprintf(stderr, "   R0 = %lf\n", state->state_var[FS_R0]);
    fprintf(stderr, "    (err = %lf)\n", state->mel);

    loop_count++;
    fprintf(stderr, "end of loop %d\n", loop_count);
  } while(loop_count < 10);

  fprintf(stderr, "Final values:\n");
  fprintf(stderr, "   A  = %lf\n", state->state_var[FS_A]);
  fprintf(stderr, "   B  = %lf\n", state->state_var[FS_B]);
  fprintf(stderr, "   X0 = %lf\n", state->state_var[FS_X0]);
  fprintf(stderr, "   R0 = %lf\n", state->state_var[FS_R0]);
  fprintf(stderr, "    (err = %lf)\n", state->mel);

  for(int j=0; j<MAX_ORDER; j++) delete [] all->t[j];
  delete all;
  return (clamped ? -1 : 0); // success, unless clamped on final cycle
}
      
  
  
