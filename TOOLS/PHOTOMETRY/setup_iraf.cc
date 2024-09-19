/*  setup_iraf.cc -- Test program used to refine the IRAF scripts used
 *  for photometry
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
#include <sys/types.h>		// for getpid()
#include <stdio.h>
#include <unistd.h> 		// for getopt(), getpid()
#include <stdlib.h>		// for atof()
#include <Image.h>
#include <HGSC.h>
#include <IStarList.h>
#include "background.h"
#include <fitsio.h>
#include <gendefs.h>

void usage(void) {
      fprintf(stderr,
	      "usage: setup_iraf -i image.fits\n");
      exit(-2);
}

int main(int argc, char **argv) {
  int ch;			// option character
  char *image_filename = 0;	// filename of the .fits image file
  Image *dark_image = 0;
  Image *flat_image = 0;

  // Command line options:
  // -i imagefile.fits

  while((ch = getopt(argc, argv, "d:s:i:")) != -1) {
    switch(ch) {
    case 'd':
      dark_image = new Image(optarg);
      break;

    case 's':
      flat_image = new Image(optarg);
      break;
      
    case 'i':			// image filename
      image_filename = optarg;
      break;

    case '?':
    default:
      usage();
   }
  }

  // all four command-line arguments are required
  if(image_filename == 0) {
    usage();
  }

  double exposure_time;
  Image image(image_filename);
  ImageInfo *info = image.GetImageInfo();
  
  if (info->ExposureDurationValid()) {
    exposure_time = info->GetExposureDuration();
  } else {
    fprintf(stderr, "Setup_Iraf: using default exposure time (1.0)\n");
    exposure_time = 1.0;
  }
    
  if(dark_image) image.subtract(dark_image);
  if(flat_image) image.scale(dark_image);
  
  Background bkgd(&image);
  double skyvalue = bkgd.Value(image.width/2, image.height/2);

  double median = image.statistics()->MedianPixel;

  // now calculate the std dev of the background.  We do that by
  // looking at all pixels at and darker than the median.  This should
  // ignore the way that stars "contaminate" the overall std dev of
  // the pixel distribution.
  double sum_sq = 0.0;
  int pixel_count = 0;
  int x, y;
  for(y = 0; y < image.height; y++) {
    for(x = 0; x < image.width; x++) {
      double pix_value = image.pixel(x, y);
      if(pix_value <= median) {
	pixel_count++;
	double diff = median - pix_value;
	sum_sq += diff*diff;
      }
    }
  }
  double std_dev = sqrt(sum_sq/pixel_count);
  fprintf(stderr, "image standard deviation = %.1f\n", std_dev);

  char uniqname[8];
  sprintf(uniqname, "%05u", (unsigned int) getpid());
  char phot_filename[64];
  sprintf(phot_filename, "/tmp/photometry%s", uniqname);
  char coords_file[64];
  sprintf(coords_file, "/tmp/coords%s", uniqname);
  char psf_parfile[64];
  sprintf(psf_parfile, "/tmp/psf.par%s", uniqname);

  FILE *fp_coord = fopen(coords_file, "w");
  if(!fp_coord) {
    fprintf(stderr, "setup_iraf: cannot create /tmp/coords\n");
    exit(-2);
  }
  IStarList old_list(image_filename);
  int star_index;
  for(star_index=0; star_index < old_list.NumStars; star_index++) {
    IStarList::IStarOneStar *star;
    star = old_list.FindByIndex(star_index);
    fprintf(fp_coord, "%f %f\n", star->StarCenterX(), star->StarCenterY());
  }
  fclose(fp_coord);

  // now create a script
  const char *script_name = "/tmp/script.cl";
  FILE *fp = fopen(script_name, "w");
  if(!fp) {
    fprintf(stderr, "setup_iraf: cannot create script file\n");
    exit(-2);
  }

  const double fwhmpsf = 4.0;

  unlink(psf_parfile);

  fprintf(fp, "noao\n");
  fprintf(fp, "digiphot\n");
  fprintf(fp, "apphot\n");
  fprintf(fp, "imdelete /tmp/imagez%s verify-\n", uniqname);
  fprintf(fp, "imdelete /tmp/psf_out%s verify-\n", uniqname);
  fprintf(fp, "delete /tmp/image_stars%s verify-\n", uniqname);
  fprintf(fp, "rfits %s \"\" /tmp/imagez%s short_header-\n",
	  image_filename, uniqname);
  fprintf(fp, "datapars.fwhmpsf=%.2f\n", fwhmpsf);
  fprintf(fp, "datapars.sigma=%.1f\n", std_dev*2.0);
  fprintf(fp, "datapars.readnoi=13.0\n");
  fprintf(fp, "datapars.epadu=2.8\n");
  fprintf(fp, "datapars.itime=%.3f\n", exposure_time);
  //  fprintf(fp, "fitskypars.annulus=%.2f\n", fwhmpsf*3);
  // fprintf(fp, "fitskypars.dannulu=%.2f\n", 10.0);
  // fprintf(fp, "photpars.apertur=%.2f\n", fwhmpsf*3);
  fprintf(fp, "fitskypars.annulus=%.2f\n", 6.0);
  fprintf(fp, "fitskypars.dannulu=%.2f\n", 4.0);
  fprintf(fp, "fitskypars.salgorithm=\"ofilter\"\n");
  fprintf(fp, "fitskypars.skyvalue=%.3f\n", skyvalue);
  fprintf(fp, "photpars.apertur=%.2f\n", 2.5);
  fprintf(fp, "phot /tmp/imagez%s coords=\"%s\" output=\"%s\" interactive=no verify=no verbose=no\n",
	  uniqname, coords_file, phot_filename);
  fprintf(fp, "print \"phot finished\"\n");
  fprintf(fp, "daophot\n");
  fprintf(fp, "pstselect /tmp/imagez%s %s /tmp/pstfile%s 25 verify=no\n",
	  uniqname, phot_filename, uniqname);
  fprintf(fp, "print \"pstselect finished\"\n");
  fprintf(fp, "psf image=/tmp/imagez%s photfile=%s pstfile=/tmp/pstfile%s psfimage=/tmp/psf_out%s opstfile=/tmp/dummy1%s groupfile=/tmp/dummy2%s interactive=no showplot=no verbose=no verify=no\n",
	  uniqname, phot_filename, uniqname, uniqname, uniqname, uniqname);
  fprintf(fp, "print \"psf finished\"\n");
  fprintf(fp, "hselect /tmp/psf_out%s PAR? yes > %s\n",
	  uniqname, psf_parfile);
  fprintf(fp, "logout\n");

  fclose(fp);
  
  char psf_name[64];
  sprintf(psf_name, "/tmp/pstfile%s", uniqname);

  unlink(psf_name);
 /* unlink(coords_file); */
  char command_buffer[132];
  sprintf(command_buffer, "cd /home/mark; cl < %s > /tmp/script.out%s 2>&1\n",
	  script_name, uniqname);
  if(system(command_buffer)) {
    fprintf(stderr, "iraf script returned error code.\n");
  }

  fp = fopen(psf_parfile, "r");
  if(fp) {
    double par1, par2;
    if(fscanf(fp, "%lf %lf", &par1, &par2) != 2) {
      fprintf(stderr, "error parsing psf_out file\n");
    }
    fprintf(stderr, "par1 = %f par2 = %f\n", par1, par2);
    fclose(fp);
  }

  fprintf(stderr, "image       file is /tmp/imagez%s\n", uniqname);
  fprintf(stderr, "coordinates file is %s\n", coords_file);
  fprintf(stderr, "psf image   file is /tmp/psf_out%s\n", uniqname);
  fprintf(stderr, "photometry  file is %s\n", phot_filename);
  fprintf(stderr, "pst list    file is /tmp/pstfile%s\n", uniqname);
}
