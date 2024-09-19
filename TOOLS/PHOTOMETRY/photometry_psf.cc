/*  photometry_psf.cc -- use IRAF DAOPHOTto perform photometry on an image
 *
 *  Copyright (C) 2007, 2018, 2019 Mark J. Munkacsy
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
#include <string.h>
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

//#define DELETE_TEMPS

void usage(void) {
      fprintf(stderr,
	      "usage: photometry_psf -i image.fits\n");
      exit(-2);
}

void setup_iraf(FILE *fp, double fwhmpsf, double std_dev, double egain, double exposure_time) {
  fprintf(fp, "cd /tmp\n");
  fprintf(fp, "noao\n");
  fprintf(fp, "digiphot\n");
  fprintf(fp, "daophot\n");

  // DATAPARS
  fprintf(fp, "print \"setting datapars.*\"\n");
  fprintf(fp, "datapars.fwhmpsf=%.2f\n", fwhmpsf);
  fprintf(fp, "datapars.scale=1.0\n"); // affects aperture/annulus interpretation
  fprintf(fp, "datapars.sigma=%.1f\n", std_dev);
  fprintf(fp, "datapars.readnoi=13.0\n");
  fprintf(fp, "datapars.epadu=%.3lf\n", egain);
  fprintf(fp, "datapars.itime=%.3f\n", exposure_time);
  fprintf(fp, "datapars.datamin=1.0\n");
  fprintf(fp, "datapars.datamax=65000.0\n");
  fprintf(fp, "datapars.airmass=\"AIRMASS\"\n");
  fprintf(fp, "datapars.filter=\"FILTER\"\n");

  // FINDPARS,   CENTERPARS
  // nothing needs to be set for findpars or centerpars

  // FITSKYPARS
  // fprintf(fp, "fitskypars.annulus=%.2f\n", fwhmpsf*3);
  // fprintf(fp, "fitskypars.dannulu=%.2f\n", 10.0);
  // fprintf(fp, "photpars.apertur=%.2f\n", fwhmpsf*3);
  fprintf(fp, "print \"setting fitskypars.*\"\n");
  fprintf(fp, "fitskypars.annulus=%.2f\n", 10.0); // inner radius of sky annulus
  fprintf(fp, "fitskypars.dannulu=%.2f\n", 10.0); // width of sky annulus
  fprintf(fp, "fitskypars.salgorithm=\"mode\"\n");

  // PHOTPARS
  fprintf(fp, "print \"setting photpars.*\"\n");
  fprintf(fp, "photpars.apertur=%.2f\n", 3.0);

  // DAOPARS
  fprintf(fp, "print \"setting daopars.*\"\n");
  fprintf(fp, "daopars.function=\"moffat25\"\n");
  fprintf(fp, "daopars.varorder=1\n"); // constant PSF over entire image
  fprintf(fp, "daopars.nclean=3\n"); // clean up of PSF stars
  fprintf(fp, "daopars.saturated=no\n");
  fprintf(fp, "daopars.psfrad=11.0\n");
  fprintf(fp, "daopars.fitrad=3.0\n");
  fprintf(fp, "daopars.recenter=yes\n");
  fprintf(fp, "daopars.fitsky=no\n");
  fprintf(fp, "daopars.groupsky=yes\n");
}

int main(int argc, char **argv) {
  int ch;			// option character
  char *image_filename = 0;	// filename of the .fits image file

  // Command line options:
  // -i imagefile.fits
  // -u                     Do not write PSF par1 & par2 into file

  while((ch = getopt(argc, argv, "d:s:nui:o:")) != -1) {
    switch(ch) {
    case 'i':			// image filename
      image_filename = optarg;
      break;

    case '?':
    default:
      usage();
   }
  }

  // image filename arguments are required
  if(image_filename == 0) {
    usage();
  }

  double exposure_time;
  double egain = 3.0; // e-/ADU default
  //double readnoise = 10.0; // e-
  {
    fitsfile *fits_fp;
    int status = 0;
    char exposure_time_string[80];
    char gain_string[80];

    if(fits_open_file(&fits_fp, image_filename, READONLY, &status)) {
      fits_report_error(stderr, status); /* print error report */
      exit(status);
    }

    if(fits_read_keyword(fits_fp, "EXPOSURE", exposure_time_string,
			 NULL, &status)) {
      exposure_time_string[0] = 0;
      status = 0;
    }

    if(fits_read_keyword(fits_fp, "EGAIN", gain_string, NULL, &status)) {
      gain_string[0] = 0;
      status = 0;
    }
    fits_close_file(fits_fp, &status);

    if(exposure_time_string[0]) {
      sscanf(exposure_time_string, "%lf", &exposure_time);
    } else {
      fprintf(stderr, "Photometry: using default exposure time (1.0)\n");
      exposure_time = 1.0;
    }
    if(gain_string[0]) {
      sscanf(gain_string, "%lf", &egain);
    } else {
      fprintf(stderr, "Photometry: using default gain of %.2lf\n",
	      egain);
    }
  }
    
  Image image(image_filename);
  Background bkgd(&image);
  //double skyvalue = bkgd.Value(image.width/2, image.height/2);

  //double median = image.statistics()->MedianPixel;
  const double std_dev = bkgd.Stddev();

  fprintf(stderr, "sky background standard deviation = %.1f\n", std_dev);

  char uniqname[12];
  sprintf(uniqname, "%05u", (unsigned int) getpid());
  char phot_filename[128];
  sprintf(phot_filename, "/tmp/photometry%s", uniqname);
  char coords_file[128];
  sprintf(coords_file, "/tmp/coords%s", uniqname);
  char psf_parfile[128];
  sprintf(psf_parfile, "/tmp/psf.par%s", uniqname);

  FILE *fp_coord = fopen(coords_file, "w");
  if(!fp_coord) {
    fprintf(stderr, "photometry: cannot create /tmp/coords\n");
    exit(-2);
  }
  IStarList old_list(image_filename);
  int star_index;
  for(star_index=0; star_index < old_list.NumStars; star_index++) {
    IStarList::IStarOneStar *star;
    star = old_list.FindByIndex(star_index);
    fprintf(fp_coord, "%f %f\n",
	    1.0+star->StarCenterX(), 1.0+star->StarCenterY());
  }
  fclose(fp_coord);

  // now create a script
  char script_name[128];
  sprintf(script_name, "/tmp/script%s.cl", uniqname);
  FILE *fp = fopen(script_name, "w");
  if(!fp) {
    perror("Cannot create file: ");
    fprintf(stderr, "photometry: cannot create script file\n");
    exit(-2);
  }

  const double fwhmpsf = 2.6;

  unlink(psf_parfile);

  fprintf(fp, "print \"starting photmetry_psf script.\"\n");
  setup_iraf(fp, fwhmpsf, std_dev, egain, exposure_time);

  fprintf(fp, "imdelete /tmp/imagez%s verify-\n", uniqname);
  fprintf(fp, "imdelete /tmp/psf_out%s verify-\n", uniqname);
  fprintf(fp, "delete /tmp/image_stars%s verify-\n", uniqname);
  fprintf(fp, "print \"running rfits\"\n");
  fprintf(fp, "rfits %s \"\" /tmp/imagez%s short_header-\n",
	  image_filename, uniqname);
  
  fprintf(fp, "print \"running phot.*\"\n");
  fprintf(fp, "phot /tmp/imagez%s coords=\"%s\" output=\"%s\" interactive=no verify=no verbose=no\n",
	  uniqname, coords_file, phot_filename);
  fprintf(fp, "print \"phot finished\"\n");

  fprintf(fp, "pstselect /tmp/imagez%s %s /tmp/pstfile%s 25 verify=no\n",
	  uniqname, phot_filename, uniqname);
  fprintf(fp, "print \"pstselect finished\"\n");
  fprintf(fp, "psf image=/tmp/imagez%s photfile=%s pstfile=/tmp/pstfile%s psfimage=/tmp/psf_out%s opstfile=/tmp/dummy1_%s groupfile=/tmp/dummy2_%s interactive=no showplot=no verbose=no verify=no\n",
	  uniqname, phot_filename, uniqname, uniqname, uniqname, uniqname);
  fprintf(fp, "print \"psf finished\"\n");
  fprintf(fp, "nstar image=/tmp/imagez%s groupfile=/tmp/dummy2_%s psfimage=/tmp/psf_out%s nstarfile=/tmp/dummy3_%s rejfile=/tmp/dummy4_%s verbose=yes verify=no\n",
	  uniqname, uniqname, uniqname, uniqname, uniqname);
  fprintf(fp, "logout\n");

  fclose(fp);
  
  // Remove any old (leftover) files that might collide with this invocation
  char dummy1_name[128];
  char dummy2_name[128];
  char dummy3_name[128];
  char dummy4_name[128];
  char psf_name[128];
  char psf_fitsname[128];
  sprintf(dummy1_name, "/tmp/dummy1_%s", uniqname);
  sprintf(dummy2_name, "/tmp/dummy2_%s", uniqname);
  sprintf(dummy3_name, "/tmp/dummy3_%s", uniqname);
  sprintf(dummy4_name, "/tmp/dummy4_%s", uniqname);
  sprintf(psf_name, "/tmp/psf_out%s", uniqname);
  sprintf(psf_fitsname, "/tmp/psf_out%s.fits", uniqname);

  unlink(dummy1_name);
  unlink(dummy2_name);
  unlink(dummy3_name);
  unlink(dummy4_name);
  unlink(psf_name);
  unlink(psf_fitsname);
  /* unlink(coords_file); */
  int num_cycles = 5;
  bool cycle_again = true;

  do {
    // Execute the command
    char command_buffer[512];
    sprintf(command_buffer, "cd /home/mark/iraf0; ecl < %s > /tmp/script.out_r%s 2>&1;update_pst_list -t /tmp/pstfile%s -p /tmp/dummy3_%s -o /tmp/messages.txt\n",
	    script_name, uniqname, uniqname, uniqname);
    fprintf(stderr, "Invoking iraf: %s\n", command_buffer);
    if(system(command_buffer)) {
      fprintf(stderr, "iraf script returned an error code.\n");
    }
    fprintf(stderr, "...iraf returned.\n");

    FILE *messages = fopen("/tmp/messages.txt", "r");
    if (!messages) {
      fprintf(stderr, "photometry_psf: /tmp/messages.txt file can't be opened.\n");
      break;
    }
    char buffer[180];
    if (fgets(buffer, sizeof(buffer), messages)) {
      if (strcmp(buffer, "MODIFIED\n") == 0) {
	// Need to re-run things.
	sprintf(script_name, "/tmp/script_r%s.cl", uniqname);
	FILE *fp = fopen(script_name, "w");
	if(!fp) {
	  perror("Cannot create file: ");
	  fprintf(stderr, "photometry: cannot create script_r file\n");
	  exit(-2);
	}

	fprintf(fp, "print \"starting photmetry_psf script.\"\n");
	setup_iraf(fp, fwhmpsf, std_dev, egain, exposure_time);
	fprintf(fp, "psf image=/tmp/imagez%s photfile=%s pstfile=/tmp/pstfile%s psfimage=/tmp/psf_out%s opstfile=/tmp/dummy1_%s groupfile=/tmp/dummy2_%s interactive=no showplot=no verbose=no verify=no\n",
		uniqname, phot_filename, uniqname, uniqname, uniqname, uniqname);
	fprintf(fp, "print \"psf finished\"\n");
	fprintf(fp, "nstar image=/tmp/imagez%s groupfile=/tmp/dummy2_%s psfimage=/tmp/psf_out%s nstarfile=/tmp/dummy3_%s rejfile=/tmp/dummy4_%s verbose=yes verify=no\n",
		uniqname, uniqname, uniqname, uniqname, uniqname);
	fprintf(fp, "logout\n");

	fclose(fp);
	
	unlink(dummy1_name);
	unlink(dummy2_name);
	unlink(dummy3_name);
	unlink(dummy4_name);
	unlink(psf_name);
	unlink(psf_fitsname);

	cycle_again = true;
	
      } else if (strcmp(buffer, "OKAY\n") == 0) {
	// done.
	cycle_again = false;
	break;
      } else {
	fprintf(stderr, "photometry_psf: ERROR: messages.txt random content: %s\n", buffer);
      }
    } else {
      fprintf(stderr, "photmetry_psf: unable to read /tmp/messages.txt\n");
    }
    // copy the rest of the messages file onto stderr
    while(fgets(buffer, sizeof(buffer), messages)) {
      fputs(buffer, stderr);
    }
    fclose(messages);
  } while (cycle_again && (num_cycles--) > 0); // always exit from loop using "break"
    

  //****************************************************************
  //        Perform allstar photometry
  //****************************************************************
  // Need to re-run things.
  sprintf(script_name, "/tmp/script_f%s.cl", uniqname);
  fp = fopen(script_name, "w");
  if(!fp) {
    perror("Cannot create file: ");
    fprintf(stderr, "photometry: cannot create script_f file\n");
    exit(-2);
  }

  fprintf(fp, "print \"starting photmetry_psf script.\"\n");
  setup_iraf(fp, fwhmpsf, std_dev, egain, exposure_time);
  fprintf(fp, "allstar image=/tmp/imagez%s photfile=%s psfimage=/tmp/psf_out%s.fits allstarfile=/tmp/dummy5_%s rejfile=/tmp/dummy6_%s subimage=/tmp/imagez_sub%s verbose=yes verify=no\n",
	  uniqname, phot_filename, uniqname, uniqname, uniqname, uniqname);
  fprintf(fp, "print \"allstar finished\"\n");
  fprintf(fp, "logout\n");

  char dummy5_filename[94];
  char dummy6_filename[94];
  char subimage_filename[94];

  sprintf(dummy5_filename, "/tmp/dummy5_%s", uniqname);
  sprintf(dummy6_filename, "/tmp/dummy6_%s", uniqname);
  sprintf(subimage_filename, "/tmp/imagez_sub%s", uniqname);
  unlink(dummy5_filename);
  unlink(dummy6_filename);
  unlink(subimage_filename);

  fclose(fp);
  
  // Execute the command
  char command_buffer[512];
  sprintf(command_buffer, "cd /home/mark/iraf0; ecl < %s > /tmp/script.out_f%s 2>&1\n",
	  script_name, uniqname);
  fprintf(stderr, "Invoking iraf: %s\n", command_buffer);
  if(system(command_buffer)) {
    fprintf(stderr, "iraf script returned error code.\n");
  }
  fprintf(stderr, "...iraf returned.\n");

  sprintf(command_buffer, "allstar2istar -i %s -t %s",
	  image_filename, dummy5_filename);
  fprintf(stderr, "Importing photometry into %s\n", image_filename);
  if(system(command_buffer)) {
    fprintf(stderr, "allstar2istar returned error code.\n");
  }

#ifdef DELETE_TEMPS
  unlink(dummy1_name);
  unlink(dummy2_name);
  unlink(psf_name);
  unlink(coords_file);
  unlink(script_name);
  unlink(phot_filename);

  char script_out[80];
  sprintf(script_out, "/tmp/script.out%s", uniqname);
  unlink(script_out);

#endif
}
