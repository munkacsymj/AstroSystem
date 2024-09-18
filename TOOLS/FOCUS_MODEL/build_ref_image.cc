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

Image *RefImage(int width, int height, Model *m, double integrated_flux) {
  const int Magnification = 5;

  Model mag_model = *m;
  mag_model.center_x *= Magnification;
  mag_model.center_y *= Magnification;
  mag_model.defocus_width *= Magnification;
  mag_model.gaussian_sigma *= Magnification;

  Image *mag_image = RefImageUnscaled(width * Magnification,
				      height * Magnification,
				      &mag_model,
				      integrated_flux);

  // collaps the image
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

Image *RefImageUnscaled(int width,
			int height,
			Model *m,
			double integrated_flux) {
  // constructor of NoGaussian will zero all pixels, guaranteed.
  Image NoGaussian(height, width);

  // build NoGaussian pixel by pixel
  const double outer_circle_radius = m->defocus_width;
  const double inner_circle_radius = outer_circle_radius *
    m->obstruction_fraction;
  const double illuminated_area =
    M_PI * (outer_circle_radius*outer_circle_radius -
	    inner_circle_radius*inner_circle_radius);
  const double intensity = integrated_flux/illuminated_area;

  #ifdef FULL_DEBUG
  FILE *d_fp = fopen("/tmp/refimage.txt", "a");
  fprintf(d_fp, "outer R = %lf, inner R = %lf\n",
	  outer_circle_radius, inner_circle_radius);
  fprintf(d_fp, "center @ (%lf, %lf)\n",
	  m->center_x, -m->center_y);
  #endif

  for (int x = 0; x < width; x++) {
    for (int y = 0; y < height; y++) {
      // box_bottom is y, box_top is (y+1.0)
      // box_left is x, box_right is (x+1.0)
      double outer_overlap_area = area_in_circle(m->center_x,
						 -m->center_y,
						 outer_circle_radius,
						 -y,
						 -y-1,
						 x,
						 x+1.0);
      double inner_overlap_area = area_in_circle(m->center_x,
						 -m->center_y,
						 inner_circle_radius,
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
      double illuminated_part = outer_overlap_area - inner_overlap_area;
      assert(!isnan(illuminated_part));
      assert(outer_overlap_area <= 1.0 && outer_overlap_area >= 0.0);
      assert(inner_overlap_area <= 1.0 && inner_overlap_area >= 0.0);
      
      NoGaussian.pixel(x, y) = intensity * illuminated_part;
      /* printf("(%d,%d)-> [%lf,%lf]\n",
	 x, y, outer_overlap_area, inner_overlap_area); */
    }
  }
  fprintf(stderr, "\n");

  NoGaussian.WriteFITS("/tmp/no_gaussian.fits");
  Image *blurred_image = apply_blur(&NoGaussian, m->gaussian_sigma);
  blurred_image->WriteFITS("/tmp/gaussian_blur.fits");
  return blurred_image; // need to copy image somewhere useful
}
					   
