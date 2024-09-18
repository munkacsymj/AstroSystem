/*  apbfdfind.cc -- Gaussian convolution kernel
 *
 *  Copyright (C) 2021 Mark J. Munkacsy

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

#include <math.h>
#include <algorithm>		// std::max()
#include <stdio.h>
#include "egauss.h"
#include "apbfdfind.h"
#include "apconvolve.h"

#define FWHM2SIGMA 0.42467 

void CopyImageWithBoundaries(Image &i_tgt,
			     Image &i_src,
			     int x_boundary,
			     int y_boundary) {
  // Copy the entire source image
  for (int y=0; y<i_src.height; y++) {
    for (int x=0; x<i_src.width; x++) {
      i_tgt.pixel(x+x_boundary, y+y_boundary) = i_src.pixel(x,y);
    }
  }
  
  // Set up boundary rows
  for (int i=0; i<y_boundary; i++) {
    for (int j=0; j<i_tgt.width; j++) {
      i_tgt.pixel(j,i) = i_tgt.pixel(j, y_boundary);
    }
  }
  for (int i=i_src.height+y_boundary; i<i_tgt.height; i++) {
    for (int j=0; j<i_tgt.width; j++) {
      i_tgt.pixel(j,i) = i_tgt.pixel(j, i_tgt.height-1-y_boundary);
    }
  }
  // Set up boundary columns
  for (int j=0; j<x_boundary; j++) {
    for (int i=0; i<i_tgt.height; i++) {
      i_tgt.pixel(j,i) = i_tgt.pixel(x_boundary,i);
    }
  }
  for (int j=i_tgt.width-x_boundary; j<i_tgt.width; j++) {
    for (int i=0; i<i_tgt.height; i++) {
      i_tgt.pixel(j,i) = i_tgt.pixel(i_tgt.width-1-x_boundary,i);
    }
  }
}

void ap_bfdfind(Image &im,	// input image
		RunParams &rp,	// input params, from params.h
		DAOStarlist &stars) { // output starlist, from params.h
  fprintf(stderr, "ap_bfdfind: setup convolution kernel size for %.3lf\n",
	  rp.fwhm_psf);
  rp.gauss = SetupEGParams(rp.fwhm_psf*FWHM2SIGMA,
			   rp.ratio,
			   rp.theta,
			   rp.nsigma);
  rp.gauss->relerr = SetupKernel(*rp.gauss);

  // norm = NO
  // Boundary extension:
  // x += (1 + gauss.nx/2)
  // y += (1 + gauss.ny/2)
  rp.boundary_x = (1+rp.gauss->nx/2);
  rp.boundary_y = (1+rp.gauss->ny/2);
  //fprintf(stderr, "boundary_x = %d, boundary_y = %d\n",
  //	  rp.boundary_x, rp.boundary_y);
  Image img(im.height + 2*rp.boundary_y,
	    im.width + 2*rp.boundary_x);
  CopyImageWithBoundaries(img, im, rp.boundary_x, rp.boundary_y);
  //img.WriteFITSFloat("/tmp/extended_image.fits");
  rp.convolution = new Image(im.height, im.width);

  apfconvolve(*rp.gauss, rp, img, *rp.convolution);
  
}
  
    
void ap_detect(Image &cnv,	// convolved image
	       EGParams &gauss,
	       RunParams &rp,
	       DAOStarlist &stars,
	       int *rows_to_exclude) {
  double detection_threshold = rp.gauss->relerr * rp.threshold;
  //fprintf(stderr, "Gaussian relerr = %lf, final threshold = %lf\n",
  // rp.gauss->relerr, detection_threshold);

  const int nxhalf = gauss.nx/2;
  const int nyhalf = gauss.ny/2;
  for (int y=nyhalf; y < cnv.height-nyhalf; y++) {
    if (rows_to_exclude[y]) continue;
    for (int x=nxhalf; x<cnv.width-nxhalf; x++) {
      DAOStar *newstar;
      const double pixvalue = cnv.pixel(x,y);
      if (pixvalue < detection_threshold) continue;

      // Test whether this density enhancement is a local maximum
      for (int j= 0; j< gauss.ny; j++) { // row adjust
	for (int k= 0; k< gauss.nx; k++) { // col adjust
	  const int index = j*gauss.nx + k;
	  if (gauss.skip[index] == 0 and
	      pixvalue < cnv.pixel(x+k-nxhalf,
				   y+j-nyhalf)) goto next_pixel;
	}
      }

      // Yes, it is. Add it to the star list
      newstar = new DAOStar;
      newstar->nx = x;
      newstar->ny = y;
      stars.push_back(newstar);

    next_pixel:
      ;
    }
  }
}

void ap_sharp_round(DAOStarlist &stars, Image &image, RunParams &rp) {
  const int nhalf = std::min(rp.gauss->nx/2, rp.gauss->ny/2);
  const int xmiddle = rp.gauss->nx/2;
  const int ymiddle = rp.gauss->ny/2;
  
  for(auto star : stars) {
    double sum2 = 0.0;
    double sum4 = 0.0;

    // calculate roundness
    for (int j = 0; j < nhalf; j++) {
      for (int k = 0; k < nhalf; k++) {
	const double v1 = rp.convolution->pixel(star->nx-k, star->ny-j);
	const double v2 = rp.convolution->pixel(star->nx+k, star->ny+j);
	const double v3 = rp.convolution->pixel(star->nx-j, star->ny+k);
	const double v4 = rp.convolution->pixel(star->nx+j, star->ny-k);

	sum2 += (v1+v2 - v3 - v4);
	sum4 += (fabs(v1) + fabs(v2) + fabs(v3) + fabs(v4));
      }
    }

    if (sum2 == 0.0) {
      star->round1 = 0.0;
    } else if (sum4 <= 0.0) {
      star->round1 = 9e99;
    } else {
      star->round1 = 2*sum2/sum4;
    }

    if (star->round1 > 1.9) {
      fprintf(stderr, "star [%d,%d]: sum2 = %lf, sum4 = %lf\n",
	      star->nx, star->ny, sum2, sum4);
    }

    // Now calculate sharpness
    double temp = 0.0;
    int npixels = rp.gauss->num_pts-1; // excludes center pixel as well

    for (int y=0; y<rp.gauss->ny; y++) {
      for (int x=0; x<rp.gauss->nx; x++) {
	if (rp.gauss->skip[y*rp.gauss->nx+x]
	    or
	    (x == xmiddle and y == ymiddle)) {
	  continue;
	}
	const double pixval = image.pixel(star->nx-xmiddle+x,
					  star->ny-ymiddle+y);
	temp += (pixval - rp.median);
      }
    }

    star->sharp = temp;
    if (rp.convolution->pixel(star->nx,star->ny) <= 0.0 ||
	npixels <= 0) {
      star->sharp = NAN;
    } else {
      star->sharp = (image.pixel(star->nx,star->ny) - rp.median - temp/npixels)/
	rp.convolution->pixel(star->nx, star->ny);
    }
  }
}

static inline double square(double x) { return x*x; }

void ap_xy_round(DAOStarlist &stars, Image &image, RunParams &rp) {
  const double xhalf = rp.gauss->nx/2 - 0.5;
  const double yhalf = rp.gauss->ny/2 - 0.5;
  const int xmiddle = rp.gauss->nx/2;
  const int ymiddle = rp.gauss->ny/2;
  const double skymode = rp.median;
  const double xsigsq = square(rp.fwhm_psf / 2.35482);
  const double ysigsq = square(rp.ratio * rp.fwhm_psf / 2.35482);

  for(auto star : stars) {
    double hx, hy; // the two roundness scores
    {
      // First, work in x dimension
      double sumgd = 0.0;
      double sumgsq = 0.0;
      double sumg = 0.0;
      double sumd = 0.0;
      double sumdx = 0.0;
      double sdgdx = 0.0;
      double sdgdxsq = 0.0;
      double sddgdx = 0.0;
      double sgdgdx = 0.0;
      double p = 0.0;
      int n = 0;

      for (int k=0; k<rp.gauss->nx; k++) {
	double sg = 0.0;
	double sd = 0.0;
	for (int j=0; j<rp.gauss->ny; j++) {
	  const double wt = (ymiddle - abs(j-ymiddle));
	  const double pixval = image.pixel(star->nx-xmiddle+k,
					    star->ny-ymiddle+j);
	  sd += (pixval - skymode) * wt;
	  sg += rp.gauss->gkernel[j*rp.gauss->nx+k] * wt;

	  //fprintf(stderr, "xyr @ [%d,%d], sg = %lf (kern=%lf, wt = %lf)\n",
	  //	  k, j, rp.convolution->pixel(k,j)*wt, rp.convolution);
	}
	//fprintf(stderr, "row %d: sg = %lf\n", k, sg);
	if (sg <= 0.0) continue;
	const double wt = (xmiddle - abs(k - xmiddle));
	sumgd += wt * sg * sd;
	sumgsq += wt * sg * sg;
	sumg += wt * sg;
	sumd += wt * sd;
	sumdx += wt * sd * (xmiddle - k);
	p += wt;
	n++;
	const double dgdx = sg * (xmiddle - k);
	sdgdxsq += wt * dgdx*dgdx;
	sdgdx += wt*dgdx;
	sddgdx += wt * sd * dgdx;
	sgdgdx += wt * sg * dgdx;
      }

      // need at least three points to estimate the x height, position
      // and local sky brightness of the star
      if (n <= 2 or p <= 0.0) {
	star->valid = false;
	fprintf(stderr, "star at [%d,%d] invalid (x) due to n (%d) or p (%lf).\n",
		star->nx, star->ny, n, p);
	continue;
      }

      // solve for the height of the best-fitting gaussian to the
      // xmarginal. Reject the star if the height is non-positive.
      hx = sumgsq - (sumg * sumg)/p;
      if (hx <= 0.0) {
	star->valid = false;
	//fprintf(stderr, "star at [%d,%d] invalid due to negative height.\n",
	//	star->nx, star->ny);
	continue;
      }

      hx = (sumgd - sumg * sumd/p) / hx;
      if (hx <= 0.0) {
	star->valid = false;
	//fprintf(stderr, "star at [%d,%d] invalid due to low center.\n",
	//	star->nx, star->ny);
	continue;
      }

      // Solve for the new X centroid
      const double skylvl = (sumd - hx*sumg)/p;
      double dx = (sgdgdx - (sddgdx - sdgdx*(hx*sumg + skylvl * p)))/
	(hx * sdgdxsq / xsigsq);
      if (fabs(dx) > xhalf) {
	if (sumd == 0.0) {
	  dx = 0.0;
	} else {
	  dx = sumdx / sumd;
	}
	if (fabs(dx) > xhalf) {
	  dx = 0.0;
	}
      }
      //star->x = star->nx - xmiddle + 2 + dx; // is the +1 wrong??
      star->x = star->nx + dx;
      //fprintf(stderr, "star init at [%d,%d]: dx = %lf, new [x] = %lf\n",
      //      star->nx, star->ny, dx, star->x);
    }

    {				// Now work in the Y dimension
      double sumgd = 0.0;
      double sumgsq = 0.0;
      double sumg = 0.0;
      double sumd = 0.0;
      double sumdy = 0.0;
      double sdgdy = 0.0;
      double sdgdysq = 0.0;
      double sddgdy = 0.0;
      double sgdgdy = 0.0;
      double p = 0.0;
      int n = 0;

      for (int j=0; j<rp.gauss->ny; j++) {
	double sg = 0.0;
	double sd = 0.0;
	for (int k=0; k<rp.gauss->nx; k++) {
	  const double wt = (xmiddle - abs(k-xmiddle));
	  const double pixval = image.pixel(star->nx-xmiddle+k,
					    star->ny-ymiddle+j);
	  sd += (pixval - skymode) * wt;
	  sg += rp.gauss->gkernel[j*rp.gauss->nx+k] * wt;
	}
	if (sg <= 0.0) continue;
	const double wt = (ymiddle - abs(j - ymiddle));
	sumgd += wt * sg * sd;
	sumgsq += wt * sg * sg;
	sumg += wt * sg;
	sumd += wt * sd;
	sumdy += wt * sd * (j - ymiddle);
	p += wt;
	n++;
	const double dgdy = sg * (ymiddle - j);
	sdgdysq += wt * dgdy*dgdy;
	sdgdy += wt*dgdy;
	sddgdy += wt * sd * dgdy;
	sgdgdy += wt * sg * dgdy;
      }

      // need at least three points to estimate the y height, position
      // and local sky brightness of the star
      if (n <= 2 or p <= 0.0) {
	star->valid = false;
	//fprintf(stderr, "star at [%d,%d] invalid (y) due to n (%d) or p (%lf).\n",
	//	star->nx, star->ny, n, p);
	continue;
      }

      // solve for the height of the best-fitting gaussian to the
      // xmarginal. Reject the star if the height is non-positive.
      hy = sumgsq - (sumg * sumg)/p;
      if (hy <= 0.0) {
	star->valid = false;
	//fprintf(stderr, "star at [%d,%d] invalid due to negative height.\n",
	//	star->nx, star->ny);
	continue;
      }

      hy = (sumgd - sumg * sumd/p) / hy;
      if (hy <= 0.0) {
	star->valid = false;
	//fprintf(stderr, "star at [%d,%d] invalid due to low center.\n",
	//	star->nx, star->ny);
	continue;
      }

      // Solve for the new Y centroid
      const double skylvl = (sumd - hy*sumg)/p;
      double dy = (sgdgdy - (sddgdy - sdgdy*(hy*sumg + skylvl * p)))/
	(hy * sdgdysq / ysigsq);
      if (fabs(dy) > yhalf) {
	if (sumd == 0.0) {
	  dy = 0.0;
	} else {
	  dy = sumdy / sumd;
	}
	if (fabs(dy) > yhalf) {
	  dy = 0.0;
	}
      }
      //star->y = star->ny - ymiddle + 2 + dy; // is the +1 wrong??
      star->y = star->ny + dy;
      //fprintf(stderr, "star init at [%d,%d]: dy = %lf, new [y] = %lf\n",
      //      star->nx, star->ny, dy, star->y);
      star->valid = true;
    }
    star->round2 = 2.0 * (hx - hy)/(hx + hy);
  } // end loop over all detections
}

void ap_test(DAOStarlist &stars, Image &image, RunParams &rp) {
  for (auto star : stars) {
    if (star->valid) {
      //fprintf(stderr, "star at [%d,%d]: round1 = %lf, round2 = %lf, sharp = %lf\n",
      //      star->nx, star->ny, star->round1, star->round2, star->sharp);
      if (star->sharp < rp.sharplo or star->sharp > rp.sharphi) {
	//fprintf(stderr, "star at [%d,%d]: sharpness test failed (%lf)\n",
	//	star->nx, star->ny, star->sharp);
	star->valid = false;
      } else if (star->round1 < rp.roundlo or star->round1 > rp.roundhi) {
	//fprintf(stderr, "star at [%d,%d]: roundness_1 test failed (%lf)\n",
	//	star->nx, star->ny, star->round1);
	star->valid = false;
      } else if (star->round2 < rp.roundlo or star->round2 > rp.roundhi) {
	//fprintf(stderr, "star at [%d,%d]: roundness_2 test failed (%lf)\n",
	//		star->nx, star->ny, star->round2);
	star->valid = false;
      } else if (star->x < 0.5 or star->x > image.width+0.5 or
		 star->y < 0.5 or star->y > image.height+0.5) {
	//fprintf(stderr, "star at [%d,%d]: star off edge @ (%lf,%lf)\n",
	//	star->nx, star->ny, star->x, star->y);
	star->valid = false;
      }
    }
  }
}
    
