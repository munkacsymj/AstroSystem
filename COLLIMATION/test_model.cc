#include <gsl/gsl_vector.h>
#include <gsl/gsl_matrix.h>
#include <gsl/gsl_permutation.h>
#include <gsl/gsl_linalg.h>
#include <Image.h>
#include "model.h"
#include "estimate_params.h"
#include "build_ref_image.h"
#include "residuals.h"
#include <stdio.h>
#include <unistd.h>		// getopt()

void usage(void) {
  fprintf(stderr,
	  "usage: test_model image.fits defocus_width gaussian_sigma\n");
  exit(-2);
}

int main(int argc, char **argv) {
  double specified_gaussian = 0.0; // if single_dof == true
  double spec_defocus = 0.0;
  char *image_filename = argv[1];

  if (argc != 4) usage();
  sscanf(argv[2], "%lf", &spec_defocus);
  sscanf(argv[3], "%lf", &specified_gaussian);

  Image *known_image = new Image(image_filename);
  if (known_image == 0) {
    usage();
  }

  fprintf(stderr, "Using image %s with defocus = %.3lf, gaussian = %.3lf\n",
	  image_filename, spec_defocus, specified_gaussian);

  Model trial;
  trial.defocus_width = spec_defocus;
  trial.obstruction_fraction = 0.40;
  trial.gaussian_sigma = specified_gaussian;

  int brightest_pixel_x;
  int brightest_pixel_y;
  double brightest_pixel_intensity = 0.0;

  double median_pixel = known_image->HistogramValue(0.3);
  for (int row = 0; row < known_image->height; row++) {
    for (int col = 0; col < known_image->width; col++) {
      known_image->pixel(col, row) = known_image->pixel(col, row) - median_pixel;
      if (known_image->pixel(col, row) > brightest_pixel_intensity) {
	brightest_pixel_intensity = known_image->pixel(col, row);
	brightest_pixel_x = col;
	brightest_pixel_y = row;
      }
    }
  }

  FocusParams param;
  // get the star center
  estimate_params(known_image, param);
  fprintf(stderr, "Center estimate = (%lf,%lf)\n",
	  param.center_x, param.center_y);
  trial.center_x = param.center_x;
  trial.center_y = param.center_y;

  Image *trial_image = RefImage(known_image->width, known_image->height,
				&trial, param.total_flux);

  int brightest_model_x;
  int brightest_model_y;
  double brightest_model_intensity = 0.0;
  
  // calculate residual wrt original image
  Image temp_image(known_image->height, known_image->width);
  for (int row = 0; row < temp_image.height; row++) {
    for (int col = 0; col < temp_image.width; col++) {
      temp_image.pixel(col, row) = known_image->pixel(col, row);
      if (temp_image.pixel(col, row) > brightest_model_intensity) {
	brightest_model_intensity = temp_image.pixel(col, row);
	brightest_model_x = col;
	brightest_model_y = row;
      }
    }
  }

  Residuals residuals(&temp_image, trial_image, &trial);
  double residual_measurement = residuals.RMSError();
  fprintf(stderr, "current residuals (rms) = %lf\n", residual_measurement);

  fprintf(stderr, "brightest image pixel:\n");
  fprintf(stderr, "col = %d, row = %d\n", brightest_pixel_x, brightest_pixel_y);
  fprintf(stderr, "center_x = %lf, center_y = %lf\n",
	  trial.center_x, trial.center_y);
  fprintf(stderr, "brightest model pixel:\n");
  fprintf(stderr, "col = %d, row = %d\n", brightest_model_x, brightest_model_y);
  fprintf(stderr, "center_x = %lf, center_y = %lf\n",
	  trial.center_x, trial.center_y);
  
  // print two .csv tables. The first is from the image
  printf("--------image----------\n");
  for (int row = 0; row < temp_image.height; row++) {
    for (int col = 0; col < temp_image.width; col++) {
      double x_offset = col + 0.5 - trial.center_x;
      double y_offset = row + 0.5 - trial.center_y;
      double r = sqrt(x_offset*x_offset + y_offset * y_offset);
      printf("%lf, %lf\n", r, temp_image.pixel(col, row));
    }
  }
    
  // the other table is for the model
  printf("--------model----------\n");
  for (int row = 0; row < trial_image->height; row++) {
    for (int col = 0; col < trial_image->width; col++) {
      double x_offset = col + 0.5 - trial.center_x;
      double y_offset = row + 0.5 - trial.center_y;
      double r = sqrt(x_offset*x_offset + y_offset * y_offset);
      printf("%lf, %lf\n", r, trial_image->pixel(col, row));
    }
  }
    

  // Calculate the partial differential of the model around this point;
  const double delta_defocus = 0.1;
  const double delta_gaussian = 0.01;

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
    
  gradient.gaussian_sigma += delta_gaussian;

  gradient_gaussian_image = RefImage(temp_image.width, 
				     temp_image.height,
				     &gradient, param.total_flux);

  gradient_gaussian_image->subtract(trial_image);
  gradient_gaussian_image->scale(1.0/delta_gaussian);

  // now perform the NLLS estimation
  //****************************************************************
  //        Allocate the three gsl structures: matrix, product, and permutation
  //****************************************************************
  gsl_vector *product = 0;
    
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

    if (grad_defocus != 0) {
      fprintf(stderr, "@(%d, %d), r = %.2lf, resid = %.1lf, grad_d = %.1lf, grad_gaus = %.1lf, prod = %.0lf\n",
	      x, y, residuals.ResidualR(n), residuals.ResidualErr(n),
	      grad_defocus, grad_gaussian, grad_defocus*residuals.ResidualErr(n));
    }
      
    err_sq += residuals.ResidualErr(n) * residuals.ResidualErr(n);

    (*gsl_vector_ptr(product, 0)) += grad_defocus * residuals.ResidualErr(n);
    (*gsl_vector_ptr(product, 1)) += grad_gaussian * residuals.ResidualErr(n);
       
    (*gsl_matrix_ptr(matrix, 0, 0)) += grad_defocus * grad_defocus;
    (*gsl_matrix_ptr(matrix, 1, 1)) += grad_gaussian * grad_gaussian;
    (*gsl_matrix_ptr(matrix, 0, 1)) += grad_gaussian * grad_defocus;
    (*gsl_matrix_ptr(matrix, 1, 0)) += grad_gaussian * grad_defocus;
  }

  fprintf(stderr, "----> prod_sum = %.0lf\n", (*gsl_vector_ptr(product, 0)));

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

  // delta(defocus) is in product[0]
  // delta(gaussian) is in product[1]
      
  double delta_focus_param = gsl_vector_get(product, 0);
  double delta_gaussian_param = gsl_vector_get(product, 1);

  gsl_vector_free(product);
    
  fprintf(stderr, "   delta_focus = %lf, delta_gaussian = %lf\n",
	  delta_focus_param, delta_gaussian_param);
}
	
