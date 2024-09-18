#include "circle_box.h"
#include "model.h"
#include <Image.h>
#include <assert.h>
#include <stdio.h>
#include "build_ref_image.h"
#include "gaussian_blur.h"

//#define FULL_DEBUG

Image *RefImageUnscaled(int width,
			int height,
			Model *m,
			double integrated_flux);

// Build a synthetic image. The image is built "magnified" -- later on
// we will shrink it down to the correct size.
Image *RefImage(int width, int height, Model *m, double integrated_flux) {
  const int Magnification = 5;

  Model mag_model = *m; // clone orig model into a "magnified model"
  mag_model.center_x *= Magnification;
  mag_model.center_y *= Magnification;
  mag_model.defocus_width *= Magnification;
  mag_model.gaussian_sigma *= Magnification;
  mag_model.collimation_x *= Magnification;
  mag_model.collimation_y *= Magnification;

  Image *mag_image = RefImageUnscaled(width * Magnification,
				      height * Magnification,
				      &mag_model,
				      integrated_flux);

  // collapse the image by "unmagnifying" it.
  Image *result = new Image(height, width);
  for (int col = 0; col < width; col++) {
    for (int row = 0; row < height; row++) {
      double pixel_sum = 0.0;
      for (int j=0; j < Magnification; j++) {
	for (int k=0; k < Magnification; k++) {
	  pixel_sum += mag_image->pixel(col*Magnification+j,
					row*Magnification+k);
	}
      }
      result->pixel(col, row) = pixel_sum;
    }
  }
  delete mag_image;
  return result;
}

// Build a doughnut synthetic image made up of 5 concentric rings.
Image *RefImageUnscaled(int width,
			int height,
			Model *m,
			double integrated_flux) {
  constexpr int num_rings = 5;
  // constructor of NoGaussian will zero all pixels, guaranteed.
  Image *NoGaussian = new Image(height, width);

  // build NoGaussian pixel by pixel; each ring has an outer radius
  // and an inner radius.
  const double outer_circle_radius = m->defocus_width;
  const double inner_circle_radius = outer_circle_radius *
    m->obstruction_fraction;
  const double ring_width = (outer_circle_radius - inner_circle_radius)/num_rings;
  const double del_col_x = m->collimation_x/num_rings;
  const double del_col_y = m->collimation_y/num_rings;
  const double illuminated_area =
    M_PI * (outer_circle_radius*outer_circle_radius -
	    inner_circle_radius*inner_circle_radius);
  // intensity in units of ADU per full pixel
  const double intensity = integrated_flux/illuminated_area;

  #ifdef FULL_DEBUG
  FILE *d_fp = fopen("/tmp/refimage.txt", "a");
  fprintf(d_fp, "outer R = %lf, inner R = %lf\n",
	  outer_circle_radius, inner_circle_radius);
  fprintf(d_fp, "center @ (%lf, %lf)\n",
	  m->center_x, -m->center_y);
  #endif

  for (int ring = 0; ring < num_rings; ring++) {
    const double outer_ring = outer_circle_radius - ring*ring_width;
    const double inner_ring = outer_ring - ring_width;
    const double center_x = m->center_x+ring*del_col_x;
    const double center_y = -(m->center_y+ring*del_col_y);
    //fprintf(stderr, "outer radius = %.3lf, inner = %.3lf, (x,y) = ( %.2lf, %.2lf)\n",
    //	    outer_ring, inner_ring, center_x, center_y);

    // For each pixel in the image, calculate the area overlap between
    // that pixel and the current ring (both inner ring edge and outer
    // ring edge).
    for (int x = 0; x < width; x++) {
      for (int y = 0; y < height; y++) {
	// box_bottom is y, box_top is (y+1.0)
	// box_left is x, box_right is (x+1.0)
	double outer_overlap_area = area_in_circle(center_x,
						   center_y,
						   outer_ring,
						   -y,
						   -y-1,
						   x,
						   x+1.0);
	double inner_overlap_area = area_in_circle(center_x,
						   center_y,
						   inner_ring,
						   -y,
						   -y-1,
						   x,
						   x+1.0);
#ifdef FULL_DEBUG
	if (outer_overlap_area != 0.0 || inner_overlap_area != 0.0) {
	  fprintf(d_fp, "(%d, %d): outer = %lf, inner = %lf\n",
		  x, y, outer_overlap_area, inner_overlap_area);
	}
#endif
	const double illuminated_part = outer_overlap_area - inner_overlap_area;
	assert(!isnan(illuminated_part));
	assert(outer_overlap_area <= 1.0 && outer_overlap_area >= 0.0);
	assert(inner_overlap_area <= 1.0 && inner_overlap_area >= 0.0);
      
	// Store the light from this ring into the image
	NoGaussian->pixel(x, y) += (intensity * illuminated_part);
	/* printf("(%d,%d)-> [%lf,%lf]\n",
	   x, y, outer_overlap_area, inner_overlap_area); */
      }
    }
  }
  //fprintf(stderr, "\n");

  NoGaussian->WriteFITS("/tmp/no_gaussian.fits");
  // Now apply a gaussian blur to the resulting image.
  Image *blurred_image = apply_blur(NoGaussian, m->gaussian_sigma);
  blurred_image->WriteFITS("/tmp/gaussian_blur.fits");

  delete NoGaussian;
  return blurred_image;
}
					   
