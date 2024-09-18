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

// forward declaration
double total_residual(Model *m_init, FocusParams *param, Image *normalized_image);

void usage(void) {
  fprintf(stderr, "usage: find_match -i image.fits -s -g gaussian_value\n");
  exit(-2);
}

// find_match: take an image that contains a single out-of-focus star
// and calculate three parameters for that star:
//    1. The amount of blur (defocus) -- derived from the diameter of
// the star's disk
//    2,3. The (x,y) "skew shift" that best describes the blurred
// disk.
//
// "Skew shift" is a term I invented to describe how different
// diameters of the out-of-focus donut are displaced from the "true"
// center of the star. Think of a bulls-eye dartboard with a whole
// series of concentric rings. Now image that each ring is slightly
// displaced from the true center by an amount equal to (x,y) times
// the radius of that particular ring.
//
// Oh, and on top of the well-defined doughnut we also allow for some
// amount of gaussian blur to be applied.

int main(int argc, char **argv) {
  int option_char;
  double specified_gaussian = 0.0; 
  double final_answer = 0.0;
  bool converged = false;
  char *image_filename = 0;
  
  Image *known_image = 0;

  while((option_char = getopt(argc, argv, "i:g:")) > 0) {
    switch (option_char) {
    case 'g':
      specified_gaussian = atof(optarg);
      if (specified_gaussian < 0.0 || specified_gaussian > 25.0) {
	fprintf(stderr, "option -g: valid values only between 0..25\n");
	usage();
      }
      break;

      // Use "-i filename" to specify the name of the file to be
      // analyzed 
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
  
  // it's an error to run this program without specifying an image
  // filename. 
  if (known_image == 0) {
    usage();
  }

  // we will keep modifying "trial" until we get the best match to the
  // image. 
  Model trial;
  trial.defocus_width = 3.752;
  trial.obstruction_fraction = 0.40;
  trial.collimation_x = 0.0;
  trial.collimation_y = 0.0;

  if (specified_gaussian != 0.0) {
    trial.gaussian_sigma = specified_gaussian;
  } else {
    trial.gaussian_sigma = 1.05;
  }

  // The star is relatively small compared to the entire image, so
  // taking the median value (actually, not quite the median value; we
  // take the pixel value that has roughly 1/3 of the pixels darker
  // and 2/3 of the pixels lighter) of the image will give us the dark sky
  // background. We subtract that from all pixels.
  double median_pixel = known_image->HistogramValue(0.3);
  for (int row = 0; row < known_image->height; row++) {
    for (int col = 0; col < known_image->width; col++) {
      known_image->pixel(col, row) = known_image->pixel(col, row) - median_pixel;
    }
  }

  FocusParams param;
  // get the star center
  estimate_params(known_image, param);
  if (param.success == false) {
    return -1; // failed to find a center
  }
  
  fprintf(stderr, "Center estimate = (%lf,%lf)\n",
	  param.center_x, param.center_y);
  trial.center_x = param.center_x;
  trial.center_y = param.center_y;
  trial.background = param.background;

  int quit;
  int loop_count = 0;
  double best_residual = 9.0e99;
  
  // Keep improving the model's parameters until the residual error
  // between the model of the star and the actual image stops getting
  // any better.
  do {
    loop_count++;
    const int loop_type = loop_count % 4;
    fprintf(stderr, "\nIteration %d starting:\n", loop_count);
    fprintf(stderr, "trial.defocus_width = %lf\n", trial.defocus_width);
    fprintf(stderr, "trial.gaussian_sigma = %lf\n", trial.gaussian_sigma);

    // Build a synthetic image
    Image *trial_image = RefImage(known_image->width, known_image->height,
				  &trial, param.total_flux);
    // Store it into an image file on the disk
    trial_image->WriteFITS("/tmp/small_image.fits");
    
    // calculate residual wrt original image
    // copy known_image (from the camera) into temp_image
    Image temp_image(known_image->height, known_image->width);
    for (int row = 0; row < temp_image.height; row++) {
      for (int col = 0; col < temp_image.width; col++) {
	temp_image.pixel(col, row) = known_image->pixel(col, row);
      }
    }

    // Create the residual errors by subtracting the model from the
    // "real" image (known_image).
    Image delta_image(known_image->height, known_image->width);
    delta_image.add(known_image);
    delta_image.subtract(trial_image);
    // And save the subtracted image onto the disk.
    delta_image.WriteFITS("/tmp/residual_image.fits");

    // compute the error residuals between the known_image (now in
    // temp_image) and the synthetic trial_image
    Residuals residuals(&temp_image, trial_image, &trial);
    double residual_measurement = residuals.RMSError();
    fprintf(stderr, "current residuals (rms) = %lf\n", residual_measurement);
    if (residual_measurement < best_residual) {
      best_residual = residual_measurement;
    }

    // Calculate the partial derivative of the model around this
    // point; this is too complex to do analytically, so we calculate
    // the derivative by seeing how much "y" changes for small changes
    // in "x". These 5 delta_zzzz are the 5 x values that can change.
    const double delta_defocus = 0.01;
    const double delta_center_x = 0.01;
    const double delta_center_y = 0.01;
    const double delta_coll_x = 0.01;
    const double delta_coll_y = 0.01;

    Model gradient;
    gradient = trial;

    ////////////////////////////////
    //   Defocus: how do residuals change for a small change in "defocus"?
    ////////////////////////////////
    gradient.defocus_width += delta_defocus;
    Image *gradient_defocus_image = RefImage(temp_image.width, 
					     temp_image.height,
					     &gradient, param.total_flux);

    gradient.defocus_width = trial.defocus_width;
    gradient_defocus_image->subtract(trial_image);
    gradient_defocus_image->scale(1.0/delta_defocus);
    gradient_defocus_image->WriteFITS("/tmp/defocus_gradient.fits");

    ////////////////////////////////
    //   Center_x: how do residuals change for a small change in "center_x"?
    ////////////////////////////////
    Image *gradient_center_x_image = 0;
    
    gradient.center_x += delta_center_x;

    gradient_center_x_image = RefImage(temp_image.width, 
				       temp_image.height,
				       &gradient, param.total_flux);

    gradient.center_x = trial.center_x;
    gradient_center_x_image->subtract(trial_image);
    gradient_center_x_image->scale(1.0/delta_center_x);
    gradient_center_x_image->WriteFITS("/tmp/center_x_gradient.fits");

    ////////////////////////////////
    //   Center_y: how do residuals change for a small change in "center_y"?
    ////////////////////////////////
    Image *gradient_center_y_image = 0;
    
    gradient.center_y += delta_center_y;

    gradient_center_y_image = RefImage(temp_image.width, 
				       temp_image.height,
				       &gradient, param.total_flux);

    gradient.center_y = trial.center_y;
    gradient_center_y_image->subtract(trial_image);
    gradient_center_y_image->scale(1.0/delta_center_y);
    gradient_center_y_image->WriteFITS("/tmp/center_y_gradient.fits");

    ////////////////////////////////
    //   Coll_x: how do residuals change for a small change in "skew_x"?
    ////////////////////////////////
    Image *gradient_coll_x_image = 0;
    
    gradient.collimation_x += delta_coll_x;

    gradient_coll_x_image = RefImage(temp_image.width, 
				     temp_image.height,
				     &gradient, param.total_flux);

    gradient.collimation_x = trial.collimation_x;
    gradient_coll_x_image->subtract(trial_image);
    gradient_coll_x_image->scale(1.0/delta_coll_x);
    gradient_coll_x_image->WriteFITS("/tmp/coll_x_gradient.fits");
    

    ////////////////////////////////
    //   Coll_y: how do residuals change for a small change in "skew_y"?
    ////////////////////////////////
    Image *gradient_coll_y_image = 0;
    
    gradient.collimation_y += delta_coll_y;

    gradient_coll_y_image = RefImage(temp_image.width, 
				     temp_image.height,
				     &gradient, param.total_flux);

    gradient.collimation_y = trial.collimation_y;
    gradient_coll_y_image->subtract(trial_image);
    gradient_coll_y_image->scale(1.0/delta_coll_y);
    gradient_coll_y_image->WriteFITS("/tmp/coll_y_gradient.fits");

    // now perform the NLLS (non-linear least squares) estimation
    //****************************************************************
    //        Allocate the three gsl structures: matrix, product, and permutation
    //****************************************************************

    constexpr int max_order = 5;
    constexpr int order = 5;
    Image *residual_images[max_order] = { 
				      gradient_center_x_image,
				      gradient_center_y_image,
				      gradient_defocus_image,
				      gradient_coll_x_image,
				      gradient_coll_y_image };

    gsl_vector *product = 0;

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
    //             index = 1 ==> center_x
    //             index = 2 ==> center_y
    //             index = 3 ==> collimation_x
    //             index = 4 ==> collimation_y

    fprintf(stderr, "Using %d residual err points.\n", residuals.NumPoints());

    for(int n = 0; n < residuals.NumPoints(); n++) {
      const int x = residuals.ResidualX(n);
      const int y = residuals.ResidualY(n);
      double gradient[order];
      const double residual = residuals.ResidualErr(n);
      
      err_sq += (residual * residual);

      for (int i=0; i< order; i++) {
	gradient[i] = residual_images[i]->pixel(x, y);
	//fprintf(stderr, " %lf,", gradient[i]);
      }
      //fprintf(stderr, "\n");
      
      for (int i=0; i< order; i++) {
	(*gsl_vector_ptr(product, i)) += (gradient[i]*residual);

	for (int k=0; k<order; k++) {
	  (*gsl_matrix_ptr(matrix, i, k)) += (gradient[i]*gradient[k]);
	}
      }
    }

    int sig_num;

    /*fprintf(stdout, "----------------\n");
    gsl_matrix_fprintf(stdout, matrix, "%f");
    fprintf(stdout, "----------------\n");
    gsl_vector_fprintf(stdout, product, "%f");*/

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

    
    double adjustments[order];
    for (unsigned int k=0; k<order; k++) {
      adjustments[k] = gsl_vector_get(product, k);
    }

    gsl_vector_free(product);

    // What we get from that are the small changes to the model's
    // trial parameters that will reduce the residual errors towards a minimum.
    fprintf(stderr,
	    "   deltas: focus = %.3lf, Xcenter = %.3lf, Ycenter = %.3lf, Xcoll = %.3lf, Ycoll = %.3lf\n",
	    adjustments[2], adjustments[0], adjustments[1], adjustments[3], adjustments[4]);
    fprintf(stderr, "     [residual measurement = %lf]\n", residual_measurement);

    static Model old_trial;
      
    // Okay, in theory the division by 2 is completely wrong. However,
    // everything works so much better if you do this. Take each
    // adjustment and cut it in half, then apply the adjustment. Yes,
    // this probably takes longer to converge, but when the entire
    // adjustment was applied, the whole thing would often diverge and
    // blow up.
    trial.center_x += (adjustments[0]/2.0);
    trial.center_y += (adjustments[1]/2.0);
    trial.defocus_width += (adjustments[2]/2.0);
    trial.collimation_x += (adjustments[3]/2.0);
    trial.collimation_y += (adjustments[4]/2.0);

    // This is another odd one. The loop would sometimes start
    // bouncing back and forth between two solutions on either side of
    // "correct". And so, every so often we take a pair of solutions
    // and average them together. This never seems to do anything bad,
    // and sometimes is a dramatic improvement.
    if (loop_count > 6) {
      if (loop_type == 0) {
	old_trial = trial;
      } else if (loop_type == 1) {
	fprintf(stderr, "      (performing an average of last two.)\n");
	trial.center_x = (trial.center_x + old_trial.center_x)/2.0;
	trial.center_y = (trial.center_y + old_trial.center_y)/2.0;
	trial.defocus_width = (trial.defocus_width + old_trial.defocus_width)/2.0;
	trial.collimation_x = (trial.collimation_x + old_trial.collimation_x)/2.0;
	trial.collimation_y = (trial.collimation_y + old_trial.collimation_y)/2.0;
      }
    }
	
    if (fabs(adjustments[0]) < 0.0001 /* && fabs(adjustments[3]) < 0.01 */) {
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
    // Always go at least 8 trips through the loop, but never go more
    // than 30.
    if (loop_count < 8) quit = 0;
    if (loop_count > 30) quit = 1;

    if (quit) {
      // Save the final model doughnut image
      trial_image->WriteFITS("/tmp/synthetic_image.fits");
    }

    for (unsigned int k=0; k<order; k++) {
      delete residual_images[k];
    }
    delete trial_image;
  } while (!quit);

  // Print the amount of collimation error
  printf("AnswerBlur %.3lf, collimation_x = %.3lf, collimation_y = %.3lf\n",
	 (converged ? final_answer : -1.0), trial.collimation_x, trial.collimation_y);
  if (converged) {
    ImageInfo info(image_filename);
    info.SetFocusBlur(final_answer);
  }

  if (known_image) {
    delete known_image;
  }
}
	
// Calculate the total error between a model image and a real
// image. Return as a scalar double.
double total_residual(Model *m_init, FocusParams *param, Image *normalized_image) {
  double residual_err = 0.0;
  const double ref_x = param->center_x;
  const double ref_y = param->center_y;
  int residual_count = 0;

  // Build the model image
  Image *trial_image = RefImage(normalized_image->width, normalized_image->height,
				m_init, param->total_flux);
    // calculate residual wrt original image
  for (int row = 0; row < normalized_image->height; row++) {
    for (int col = 0; col < normalized_image->width; col++) {
      const double del_x = (col + 0.5) - ref_x;
      const double del_y = (row + 0.5) - ref_y;
      const double r_squared = del_x * del_x + del_y * del_y;
      // only look at pixels within sqrt(100) pixels of the center of
      // the star.
      if (r_squared < 100.0) {
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

