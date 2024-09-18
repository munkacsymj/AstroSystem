//#include <gsl/gsl_vector.h>
//#include <gsl/gsl_matrix.h>
//#include <gsl/gsl_permutation.h>
//#include <gsl/gsl_linalg.h>
#include "/home/mark/gsl-2.4/gsl/gsl_vector.h"
#include "/home/mark/gsl-2.4/gsl/gsl_matrix.h"
#include "/home/mark/gsl-2.4/gsl/gsl_rng.h"
#include "/home/mark/gsl-2.4/gsl/gsl_blas.h"
#include "/home/mark/gsl-2.4/gsl/gsl_multifit.h"
#include "/home/mark/gsl-2.4/gsl/gsl_permutation.h"
#include "/home/mark/gsl-2.4/gsl/gsl_linalg.h"


#include <Image.h>
#include "model.h"
#include "estimate_params.h"
#include "build_ref_image.h"
#include "residuals.h"
#include <stdio.h>
#include <unistd.h>		// getopt()
#include <iostream>

// forward declaration
double total_residual(Model *m_init, FocusParams *param, Image *normalized_image);

void usage(void) {
  fprintf(stderr, "usage: find_match [-m max] -i image.fits -s -g gaussian_value\n");
  exit(-2);
}

void FindInitialRange(double &a, double &b,
		      double &residual_a, double &residual_b,
		      Model &init_trial, FocusParams &param, Image *known_image);
double parab_minimum(std::vector<double> &x, std::vector<double> &y);

double v_min(std::vector<double> &a) {
  double min = a[0];
  for (double x : a) {
    if (x < min) min = x;
  }
  return min;
}
double v_max(std::vector<double> &a) {
  double max = a[0];
  for (double x : a) {
    if (x > max) max = x;
  }
  return max;
}

int main(int argc, char **argv) {
  int option_char;
  bool single_dof = false;	// single degree of freedom if true
  double specified_gaussian = 0.0; // if single_dof == true
  double final_answer;
  double max_considered {10.0}; 
  bool converged = false;
  char *image_filename = 0;
  
  Image *known_image = 0;

  while((option_char = getopt(argc, argv, "i:sm:g:")) > 0) {
    switch (option_char) {
    case 's':
      single_dof = true;
      break;

    case 'm':
      max_considered = atof(optarg);
      break;

    case 'g':
      specified_gaussian = atof(optarg);
      if (specified_gaussian < 0.0 || specified_gaussian > 25.0) {
	fprintf(stderr, "option -g: valid values only between 0..25\n");
	usage();
      }
      break;

    case 'i':
      image_filename = optarg;
      known_image = new Image(optarg);
      if (known_image == 0) {
	fprintf(stderr, "Unable to open image file '%s'\n",
		optarg);
      }
      break;

    case '?':
    default:
      fprintf(stderr, "Invalid agrument: %c\n", option_char);
      usage();
    }
  }
  
  if (known_image == 0) {
    usage();
  }

  Model trial;
  //trial.defocus_width = 1.0;
  trial.defocus_width = 1.95;
  trial.obstruction_fraction = 0.40;
  trial.max_radius = max_considered;
  
  //trial.gaussian_sigma = 1.0;
  if (specified_gaussian != 0.0 || single_dof) {
    trial.gaussian_sigma = specified_gaussian;
  } else {
    trial.gaussian_sigma = 1.05;
  }

  double median_pixel = known_image->HistogramValue(0.3);
  for (int row = 0; row < known_image->height; row++) {
    for (int col = 0; col < known_image->width; col++) {
      known_image->pixel(col, row) = known_image->pixel(col, row) - median_pixel;
    }
  }
  

  FocusParams param;
  // get the star center
  param.max_width_to_consider = 2.5 * max_considered;
  estimate_params(known_image, param);
  if (param.success == false) {
    return -1; // failed to find a center
  }
  
  fprintf(stderr, "Center estimate = (%lf,%lf)\n",
	  param.center_x, param.center_y);
  trial.center_x = param.center_x;
  trial.center_y = param.center_y;

  if (single_dof) {
    // use "golden section search"
    int cycle = 1;
    // the golden ratio
    const double gr = (sqrt(5.0)-1.0)/2.0;
    double a = 0.01;
    //double b = 10.0;
    double b = max_considered;
    double residual_a;
    double residual_b;

    FindInitialRange(a, b, residual_a, residual_b, trial, param, known_image);
    
    double c = b - gr*(b-a);
    double d = a + gr*(b-a);

    Model trial_c = trial;
    trial_c.defocus_width = c;
    double residual_c = total_residual(&trial_c, &param, known_image);
    Model trial_d = trial;
    trial_d.defocus_width = d;
    double residual_d = total_residual(&trial_d, &param, known_image);

    // Two ways of doing this: the "Golden Section Search" way and the
    // "Parabola Assist" way
    //#define PARABOLA_ASSIST
#define GOLDEN_SECTION_SEARCH
#ifdef GOLDEN_SECTION_SEARCH
    while (cycle < 30 && fabs(c-d) > 0.01) {
      fprintf(stderr, "Cycle %d: Checking new points %.2lf & %.2lf\n",
	      cycle++, c, d);
      fprintf(stderr, "res(%.2lf) = %.2lf; res(%.2lf) = %.2lf\n",
	      c, residual_c, d, residual_d);
      if (residual_c < residual_d) {
	b = d;
	d = c;
	c = b - gr*(b-a);
	residual_d = residual_c;
	trial_c.defocus_width = c;
	residual_c = total_residual(&trial_c, &param, known_image);
      } else {
	a = c;
	c = d;
	d = a + gr*(b-a);
	residual_c = residual_d;
	trial_d.defocus_width = d;
	residual_d = total_residual(&trial_d, &param, known_image);
      }
    }
#else // else must be PARABOLA_ASSIST
    vector<double> trials;
    vector<double> residuals;

    trials.push_back(a);
    residuals.push_back(residual_a);
    trials.push_back(d);
    residuals.push_back(residual_d);
    trials.push_back(c);
    residuals.push_back(residual_c);
    trials.push_back(b);
    residuals.push_back(residual_b);
    double para_min_prior = -1.0;
    double para_min = -2.0;

    while (cycle < 30 && fabs(para_min_prior - para_min) > 0.002) {
      Model new_point = trial;
      fprintf(stderr, "Cycle %d: Checking between limits %.3lf & %.3lf\n",
	      cycle++, a, b);
      // solve the parabola
      para_min_prior = para_min;
      para_min = parab_minimum(trials, residuals);
      fprintf(stderr, "    Parab min is at %.3lf\n", para_min);
      trials.push_back(para_min);
      new_point.defocus_width = para_min;
      const double new_residual = total_residual(&new_point, &param, known_image);
      residuals.push_back(new_residual);

      // Now delete the worst point
      double worst = -1.0;
      double worst_ticks = -1.0;
      int worst_index = -1;
      for (unsigned int i=0; i<trials.size(); i++) {
	if (residuals[i] > worst) {
	  worst = residuals[i];
	  worst_ticks = trials[i];
	  worst_index = i;
	}
      }
      trials.erase(trials.begin()+worst_index);
      residuals.erase(residuals.begin()+worst_index);
      a = v_min(trials);
      b = v_max(trials);
      if (para_min == a) {
	// oops ... bad stuff
	Model fixup = trial;
	fixup.defocus_width = (worst_ticks + para_min)/2.0;
	fprintf(stderr, "BAD PARAB: inserting fixup point.\n");
	trials.push_back(fixup.defocus_width);
	residuals.push_back(total_residual(&fixup, &param, known_image));
      }
    }
    b = a = para_min;
      
#endif
    printf("AnswerBlur %.3lf\n", (b+a)/2.0);
    return 0; // means success
  }

  int quit;
  int loop_count = 0;
  double best_residual = 9.0e99;
  do {
    loop_count++;
    fprintf(stderr, "\nIteration %d starting:\n", loop_count);
    fprintf(stderr, "trial.defocus_width = %lf\n", trial.defocus_width);
    fprintf(stderr, "trial.gaussian_sigma = %lf\n", trial.gaussian_sigma);

    Image *trial_image = RefImage(known_image->width, known_image->height,
				  &trial, param.total_flux);
    // calculate residual wrt original image
    // copy known_image (from the camera) into temp_image
    Image temp_image(known_image->height, known_image->width);
    for (int row = 0; row < temp_image.height; row++) {
      for (int col = 0; col < temp_image.width; col++) {
	temp_image.pixel(col, row) = known_image->pixel(col, row);
      }
    }

    // compute the error residuals between the known_image (now in
    // temp_image) and the synthetic trial_image
    Residuals residuals(&temp_image, trial_image, &trial);
    double residual_measurement = residuals.RMSError();
    fprintf(stderr, "current residuals (rms) = %lf\n", residual_measurement);
    if (residual_measurement < best_residual) {
      best_residual = residual_measurement;
    }

    // Calculate the partial differential of the model around this point;
    const double delta_defocus = 0.01;
    const double delta_gaussian = 0.001;

    Model gradient;
    gradient = trial;
    gradient.defocus_width += delta_defocus;

    Image *gradient_defocus_image = RefImage(temp_image.width, 
					     temp_image.height,
					     &gradient, param.total_flux);

    gradient.defocus_width = trial.defocus_width;
    gradient_defocus_image->subtract(trial_image);
    gradient_defocus_image->scale(1.0/delta_defocus);

    Image *gradient_gaussian_image = 0;
    
    if (!single_dof) {
      gradient.gaussian_sigma += delta_gaussian;

      gradient_gaussian_image = RefImage(temp_image.width, 
					 temp_image.height,
					 &gradient, param.total_flux);

      gradient_gaussian_image->subtract(trial_image);
      gradient_gaussian_image->scale(1.0/delta_gaussian);
    }

    // now perform the NLLS estimation
    //****************************************************************
    //        Allocate the three gsl structures: matrix, product, and permutation
    //****************************************************************
    gsl_vector *product = 0;
    
    if (single_dof) {
      const int order = 1;
      gsl_matrix *matrix = gsl_matrix_calloc(order, order);
      if(matrix == 0) return -1;

      product = gsl_vector_calloc(order);
      if(!product) {
	fprintf(stderr, "nlls: allocation of product vector failed.\n");
      }

      gsl_permutation *permutation = gsl_permutation_alloc(order);
      if(!permutation) {
	fprintf(stderr, "nlls: permutation create failed.\n");
      }

      double err_sq = 0.0;

      // Set the matrix and product values; N is the number of pixels
      // being used and "order" is 2 (two unknown parameters being
      // estimated)
      // Throughout, index = 0 ==> defocus_param
      //             index = 1 ==> gaussian_blur

      fprintf(stderr, "Using %d residual err points.\n", residuals.NumPoints());

      for(int n = 0; n < residuals.NumPoints(); n++) {
	const int x = residuals.ResidualX(n);
	const int y = residuals.ResidualY(n);
	const double grad_defocus = gradient_defocus_image->pixel(x, y);
      
	err_sq += residuals.ResidualErr(n) * residuals.ResidualErr(n);

	(*gsl_vector_ptr(product, 0)) += grad_defocus * residuals.ResidualErr(n);
       
	(*gsl_matrix_ptr(matrix, 0, 0)) += grad_defocus * grad_defocus;
      }

      int sig_num;

      fprintf(stdout, "----------------\n");
      gsl_matrix_fprintf(stdout, matrix, "%f");
      fprintf(stdout, "----------------\n");
      gsl_vector_fprintf(stdout, product, "%f");

      if(gsl_linalg_LU_decomp(matrix, permutation, &sig_num)) {
	fprintf(stderr, "nlls: gsl_linalg_LU_decomp() failed.\n");
	return -1;
      }

      if(gsl_linalg_LU_svx(matrix, permutation, product)) {
	fprintf(stderr, "nlls: gls_linalg_LU_solve() failed.\n");
	return -1;
      }
      gsl_matrix_free(matrix);
      gsl_permutation_free(permutation);
    } else {
      const int order = 2;
      gsl_matrix *matrix = gsl_matrix_calloc(order, order);
      if(matrix == 0) return -1;

      product = gsl_vector_calloc(order);
      if(!product) {
	fprintf(stderr, "nlls: allocation of product vector failed.\n");
      }

      gsl_permutation *permutation = gsl_permutation_alloc(order);
      if(!permutation) {
	fprintf(stderr, "nlls: permutation create failed.\n");
      }

      double err_sq = 0.0;

      // Set the matrix and product values; N is the number of pixels
      // being used and "order" is 2 (two unknown parameters being
      // estimated)
      // Throughout, index = 0 ==> defocus_param
      //             index = 1 ==> gaussian_blur

      fprintf(stderr, "Using %d residual err points.\n", residuals.NumPoints());

      for(int n = 0; n < residuals.NumPoints(); n++) {
	const int x = residuals.ResidualX(n);
	const int y = residuals.ResidualY(n);
	const double grad_defocus = gradient_defocus_image->pixel(x, y);
	const double grad_gaussian = gradient_gaussian_image->pixel(x, y);
      
	err_sq += residuals.ResidualErr(n) * residuals.ResidualErr(n);

	(*gsl_vector_ptr(product, 0)) += grad_defocus * residuals.ResidualErr(n);
	(*gsl_vector_ptr(product, 1)) += grad_gaussian * residuals.ResidualErr(n);
       
	(*gsl_matrix_ptr(matrix, 0, 0)) += grad_defocus * grad_defocus;
	(*gsl_matrix_ptr(matrix, 1, 1)) += grad_gaussian * grad_gaussian;
	(*gsl_matrix_ptr(matrix, 0, 1)) += grad_gaussian * grad_defocus;
	(*gsl_matrix_ptr(matrix, 1, 0)) += grad_gaussian * grad_defocus;
      }

      int sig_num;

      fprintf(stdout, "----------------\n");
      gsl_matrix_fprintf(stdout, matrix, "%f");
      fprintf(stdout, "----------------\n");
      gsl_vector_fprintf(stdout, product, "%f");

      if(gsl_linalg_LU_decomp(matrix, permutation, &sig_num)) {
	fprintf(stderr, "nlls: gsl_linalg_LU_decomp() failed.\n");
	return -1;
      }

      if(gsl_linalg_LU_svx(matrix, permutation, product)) {
	fprintf(stderr, "nlls: gls_linalg_LU_solve() failed.\n");
	return -1;
      }
      gsl_matrix_free(matrix);
      gsl_permutation_free(permutation);
    }

    // delta(defocus) is in product[0]
    // delta(gaussian) is in product[1]
      
    double delta_focus_param = gsl_vector_get(product, 0);
    double delta_gaussian_param = 
      (single_dof ? 0.0 : gsl_vector_get(product, 1));

    // if we have a small defocus width, cut adjustment in half to avoid oscillation
    if (trial.defocus_width < 1.0) delta_focus_param /= 2.0;

    gsl_vector_free(product);
    
    fprintf(stderr, "   delta_focus = %lf, delta_gaussian = %lf\n",
	    delta_focus_param, delta_gaussian_param);

    trial.defocus_width += delta_focus_param;
    trial.gaussian_sigma += (delta_gaussian_param);

    if (trial.defocus_width < 0) {
      trial.defocus_width = 0.001;
    }
    if (trial.defocus_width > 30.0) {
      trial.defocus_width = 30.0;
    }
    if (trial.gaussian_sigma < 0) {
      trial.gaussian_sigma = 1.0;
    }
    if (trial.gaussian_sigma > 30.0) {
      trial.gaussian_sigma = 30.0;
    }

    if (fabs(delta_focus_param) < 0.01 && fabs(delta_gaussian_param) < 0.01) {
      quit = 1;
      // we've only had a real convergence if it resulted in really
      // good residuals
      if (fabs(residual_measurement - best_residual)/best_residual < 0.01) {
	converged = true;
      } else {
	converged = false;
      }
      final_answer = trial.defocus_width;
    }
    if (loop_count < 8) quit = 0;
    if (loop_count > 30) quit = 1;

    if (quit) {
      trial_image->WriteFITS("/tmp/synthetic_image.fits");
    }

    delete gradient_defocus_image;
    delete gradient_gaussian_image;
    delete trial_image;
  } while (!quit);

  printf("AnswerBlur %.3lf\n",
	 (converged ? final_answer : -1.0));
  if (converged) {
    ImageInfo info(image_filename);
    info.SetFocusBlur(final_answer);
  }
  if (known_image) {
    delete known_image;
  }
}
	
double total_residual(Model *m_init, FocusParams *param, Image *normalized_image) {
  double residual_err = 0.0;
  const double ref_x = param->center_x;
  const double ref_y = param->center_y;
  int residual_count = 0;
  const double max_rsquare = param->max_width_to_consider*param->max_width_to_consider/4.0;

  Image *trial_image = RefImage(normalized_image->width, normalized_image->height,
				m_init, param->total_flux);
    // calculate residual wrt original image
  for (int row = 0; row < normalized_image->height; row++) {
    for (int col = 0; col < normalized_image->width; col++) {
      const double del_x = (col + 0.5) - ref_x;
      const double del_y = (row + 0.5) - ref_y;
      const double r_squared = del_x * del_x + del_y * del_y;
      if (r_squared < max_rsquare) {
	const double err = normalized_image->pixel(col, row) - trial_image->pixel(col, row);
	residual_err += (err*err);
	residual_count++;
      }
    }
  }
  const double rms_residual = sqrt(residual_err/residual_count);
  fprintf(stderr, "RMS residual at %.2lf is %.2lf\n",
	  m_init->defocus_width, rms_residual);
  return rms_residual;
}

#if 0
static void printerror( int status)
{
    /*****************************************************/
    /* Print out cfitsio error messages and exit program */
    /*****************************************************/


  if (status) {
    fits_report_error(stderr, status); /* print error report */

    exit( status );    /* terminate the program, returning error status */
  }
  return;
}
#endif

void FindInitialRange(double &a, double &b,
		      double &residual_a, double &residual_b,
		      Model &init_trial, FocusParams &param, Image *known_image) {
  constexpr int NUM_TRIES = 8;
  double trials[NUM_TRIES];
  double results[NUM_TRIES];

  double delta = b-a;
  trials[0] = a;
  trials[1] = a+ 0.04*delta;
  trials[2] = trials[1] + 0.06*delta;
  trials[3] = trials[2] + 0.05*delta;
  trials[4] = trials[3] + 0.1*delta;
  trials[5] = trials[4] + 0.1*delta;
  trials[6] = trials[5] + 0.3*delta;
  trials[NUM_TRIES-1] = b;

  double lowest_residual = 9.9e99;
  int best_try_index = -1;

  for (int i=0; i<NUM_TRIES; i++) {
    Model trial = init_trial;
    trial.defocus_width = trials[i];
    const double result = total_residual(&trial, &param, known_image);
    results[i] = result;
    if (result < lowest_residual) {
      lowest_residual = result;
      best_try_index = i;
    }
  }

  if (best_try_index == 0) {
    b = trials[1];
    fprintf(stderr, "Search narrowed to start of range [%.1lf : %.1lf], (residuals of %.1lf .. %.1lf)\n",
	    a, b, results[0], results[1]);
    residual_a = results[0];
    residual_b = results[1];
  } else if (best_try_index == NUM_TRIES-1) {
    a = trials[NUM_TRIES-2];
    fprintf(stderr, "Search narrowed to top of range [%.1lf : %.1lf], (residuals of %.1lf .. %.1lf)\n",
	    a, b, results[NUM_TRIES-2], results[NUM_TRIES-1]);
    residual_a = results[NUM_TRIES-2];
    residual_b = results[NUM_TRIES-1];
  } else {
    a = trials[best_try_index-1];
    b = trials[best_try_index+1];
    fprintf(stderr, "Search narrowed to range [%.1lf : %.1lf], (residuals of %.1lf .. %.1lf .. %.1lf)\n",
	    a, b, results[best_try_index-1], results[best_try_index],
	    results[best_try_index+1]);
    residual_a = results[best_try_index-1];
    residual_b = results[best_try_index+1];
  }
  
}


double parab_minimum(std::vector<double> &x, std::vector<double> &y) {
  const int num_measurements = x.size();

  // Now find the best parabola to fit these points
  gsl_matrix *X = gsl_matrix_alloc(num_measurements, 3);
  gsl_vector *Y = gsl_vector_alloc(num_measurements);
  gsl_vector *q = gsl_vector_alloc(3);
  gsl_matrix *cov = gsl_matrix_alloc(3, 3);

  for (int i=0; i<num_measurements; i++) {
    gsl_matrix_set(X, i, 0, 1.0);
    gsl_matrix_set(X, i, 1, x[i]);
    gsl_matrix_set(X, i, 2, x[i]*x[i]);
    gsl_vector_set(Y, i, y[i]);
    cout << "Measurement: " << x[i]
	 << ", " << y[i] << std::endl;
  }

  double chisq;
  gsl_multifit_linear_workspace *work = gsl_multifit_linear_alloc(num_measurements, 3);
  gsl_multifit_linear(X, Y, q, cov, &chisq, work);
  gsl_multifit_linear_free(work);

  const double a = gsl_vector_get(q, 2);
  const double b = gsl_vector_get(q, 1);

  const double min_x = -b/(2*a);

  gsl_matrix_free(X);
  gsl_vector_free(Y);
  gsl_vector_free(q);
  gsl_matrix_free(cov);
  return min_x;
}
   
