/*  photometry.cc -- use IRAF to perform photometry on an image
 *
 *  Copyright (C) 2007, 2018 Mark J. Munkacsy
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
#include <astro_db.h>
#include "background.h"
#include <gendefs.h>

//#define DELETE_TEMPS

void usage(void) {
      fprintf(stderr,
	      "usage: photometry [-u] -i image.fits [-d dark.fits] [-s flat.fits] [-o output.fits]\n");
      exit(-2);
}

int main(int argc, char **argv) {
  int ch;			// option character
  char *image_filename = 0;	// filename of the .fits image file
  char *output_filename = 0;
  char *flat_filename = nullptr;
  char *dark_filename = nullptr;
  int inhibit_keyword_update = 0;
  bool do_all_stars = false;

  // Command line options:
  // -i imagefile.fits
  // -o outputfile.fits     Write photometry into different FITS file
  // -u                     Do not write PSF par1 & par2 into file
  // -a                     Include all stars, not just those matched with star_match

  while((ch = getopt(argc, argv, "d:s:anui:o:")) != -1) {
    switch(ch) {
    case 'a':
      do_all_stars = true;
      break;

    case 's':
      flat_filename = optarg;
      break;

    case 'd':
      dark_filename = optarg;
      break;

    case 'u':
      inhibit_keyword_update = 1;
      break;

    case 'i':			// image filename
      image_filename = optarg;
      break;

    case 'o':
      output_filename = optarg;
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

  if(output_filename == 0) output_filename = image_filename;
  
  double exposure_time;
  Image image(image_filename);
  ImageInfo *info = image.GetImageInfo();
  Filter filter;
  if (info->FilterValid()) {
    filter = info->GetFilter();
  } 

  if (info->ExposureDurationValid()) {
    exposure_time = info->GetExposureDuration();
  } else {
    fprintf(stderr, "Photometry: using default exposure time (1.0)\n");
    exposure_time = 1.0;
  }
    
  double egain = 1.6; // e-/ADU default
  if (info->EGainValid()) {
    egain = info->GeteGain();
  } else {
    fprintf(stderr, "Photometry: using default gain of %.2lf\n",
	    egain);
  }

  bool obs_time_okay = false;
  JULIAN exposure_midpoint;
  if (info->ExposureMidpointValid()) {
    obs_time_okay = true;
    exposure_midpoint = info->GetExposureMidpoint();
  }

  if (dark_filename) {
    Image dark(dark_filename);
    image.subtract(&dark);
  }
  if (flat_filename) {
    Image flat(flat_filename);
    image.scale(&flat);
  }

  double pixel_scale = 1.52; // default for ST-9
  if (info and info->CDeltValid()) {
    pixel_scale = info->GetCDelt1();
  }

  Background bkgd(&image);
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
    fprintf(stderr, "photometry: cannot create /tmp/coords\n");
    exit(-2);
  }
  IStarList old_list(image_filename);
  int star_index;
  std::vector<int> requested_stars;
  for(star_index=0; star_index < old_list.NumStars; star_index++) {
    IStarList::IStarOneStar *star;
    star = old_list.FindByIndex(star_index);
    if (do_all_stars or (star->validity_flags & CORRELATED)) {
      requested_stars.push_back(star_index);
      fprintf(fp_coord, "%f %f\n",
	      1.0+star->StarCenterX(), 1.0+star->StarCenterY());
    }
  }
  fclose(fp_coord);

  // now create a script
  char script_name[64];
  sprintf(script_name, "/tmp/script%s.cl", uniqname);
  FILE *fp = fopen(script_name, "w");
  if(!fp) {
    perror("Cannot create file: ");
    fprintf(stderr, "photometry: cannot create script file\n");
    exit(-2);
  }

  char clean_image_filename[128];
  sprintf(clean_image_filename, "/tmp/image_clean%s.fits", uniqname);

  // create an uncompressed version of the file
  image.WriteFITSFloatUncompressed(clean_image_filename);

  const double fwhmpsf = 2.6;
  const double aperture_arcsec = 6.84;
  const double aperture_pixels = aperture_arcsec/pixel_scale;
  const double annulus_inner_arcsec = 25.0;
  const double annulus_inner_pixels = annulus_inner_arcsec/pixel_scale;
  const double annulus_width_arcsec = 3*aperture_arcsec; // coincidence?
  const double annulus_width_pixels = annulus_width_arcsec/pixel_scale;

  fprintf(stderr, "photometry: using aperture of %.1lf pixels\n",
	  aperture_pixels);

  unlink(psf_parfile);

  fprintf(fp, "noao\n");
  fprintf(fp, "digiphot\n");
  fprintf(fp, "apphot\n");
  fprintf(fp, "imdelete /tmp/imagez%s verify-\n", uniqname);
  fprintf(fp, "imdelete /tmp/psf_out%s verify-\n", uniqname);
  fprintf(fp, "delete /tmp/image_stars%s verify-\n", uniqname);
  fprintf(fp, "rfits %s \"\" /tmp/imagez%s short_header-\n",
	  clean_image_filename, uniqname);
  fprintf(fp, "datapars.fwhmpsf=%.2f\n", fwhmpsf);
  fprintf(fp, "datapars.scale=1.0\n"); // affects aperture/annulus interpretation
  fprintf(fp, "datapars.sigma=%.1f\n", std_dev*2.0);
  fprintf(fp, "datapars.readnoi=13.0\n");
  fprintf(fp, "datapars.datamax=%.1lf\n", 1048480.0); // correct for 32-bit 4x4 bin
  fprintf(fp, "datapars.epadu=%.3lf\n", egain);
  fprintf(fp, "datapars.itime=%.3f\n", exposure_time);
  //  fprintf(fp, "fitskypars.annulus=%.2f\n", fwhmpsf*3);
  // fprintf(fp, "fitskypars.dannulu=%.2f\n", 10.0);
  // fprintf(fp, "photpars.apertur=%.2f\n", fwhmpsf*3);
  fprintf(fp, "fitskypars.annulus=%.2f\n", annulus_inner_pixels); // inner radius of sky annulus
  fprintf(fp, "fitskypars.dannulu=%.2f\n", annulus_width_pixels); // width of sky annulus
  fprintf(fp, "fitskypars.salgorithm=\"mode\"\n");
  fprintf(fp, "fitskypars.skyvalue=%.3f\n", 105.0);
  fprintf(fp, "photpars.apertur=%.2f\n", aperture_pixels);
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
#ifdef DELETE_TEMPS
  fprintf(fp, "imdelete /tmp/imagez%s verify-\n", uniqname);
  fprintf(fp, "imdelete /tmp/psf_out%s verify-\n", uniqname);
#endif
  fprintf(fp, "logout\n");

  fclose(fp);
  
  char dummy1_name[64];
  char dummy2_name[64];
  char psf_name[64];
  sprintf(dummy1_name, "/tmp/dummy1%s", uniqname);
  sprintf(dummy2_name, "/tmp/dummy2%s", uniqname);
  sprintf(psf_name, "/tmp/pstfile%s", uniqname);

  unlink(dummy1_name);
  unlink(dummy2_name);
  unlink(psf_name);
  /* unlink(coords_file); */
  char command_buffer[132];
  sprintf(command_buffer, "cd " IRAF_ROOT "; ecl < %s > /tmp/script.out%s 2>&1\n",
	  script_name, uniqname);
  if(system(command_buffer)) {
    fprintf(stderr, "iraf script returned error code.\n");
  }

  fp = fopen(psf_parfile, "r");
  double par1 = -1.0;
  double par2 = -1.0;
  if(fp) {
    if(fscanf(fp, "%lf %lf", &par1, &par2) != 2) {
      fprintf(stderr, "problem parsing output of psf_file.\n");
    }
    fprintf(stderr, "par1 = %f par2 = %f\n", par1, par2);
    fclose(fp);
    unlink(psf_parfile);

    if(!inhibit_keyword_update) {
      ImageInfo info(image_filename);
      info.SetPSFPar(par1, par2);
      info.WriteFITS();
    }
  }

  fp = fopen(phot_filename, "r");
  if(!fp) {
    fprintf(stderr, "Cannot open output photometry file.\n");
    exit(-2);
  }

  AstroDB *astro_db = nullptr;
  std::list<AstroDB::InstMagMeasurement> inst_mags;
  const char *astro_db_filename = HasAstroDBInDirectory(output_filename);
  if (astro_db_filename) {
    astro_db = new AstroDB(JSON_READWRITE, astro_db_filename);
  }

  char input_buffer[1024];
  while(fgets(input_buffer, sizeof(input_buffer), fp)) {
    if(input_buffer[0] == '#') continue;

    double measured_photometry;
    double measured_flux;
    double magnitude_error;
    int star_id;

    if(sscanf(input_buffer+41, "%d", &star_id) != 1 ||
       star_id < 0 || star_id > old_list.NumStars) {
      fprintf(stderr, "trouble (1) parsing '%s'\n", input_buffer);
      // and skip the next four lines...
      char *status;
      status = fgets(input_buffer, sizeof(input_buffer), fp);
      status = fgets(input_buffer, sizeof(input_buffer), fp);
      status = fgets(input_buffer, sizeof(input_buffer), fp);
      status = fgets(input_buffer, sizeof(input_buffer), fp);
      if (!status) {
	fprintf(stderr, "trouble (1b) parsing photometry output.\n");
      }
    } else {
      IStarList::IStarOneStar *star= old_list.FindByIndex(requested_stars[star_id-1]);
      char err_msg[32];
      // skip next three lines and read the fourth
      char *status;
      status = fgets(input_buffer, sizeof(input_buffer), fp);
      status = fgets(input_buffer, sizeof(input_buffer), fp);
      status = fgets(input_buffer, sizeof(input_buffer), fp);
      status = fgets(input_buffer, sizeof(input_buffer), fp);
      if (!status) {
	fprintf(stderr, "trouble (1a) parsing photometry output.\n");
      }
      sscanf(input_buffer+51, "%s", err_msg);
      int success = 0;
      if(strcmp(err_msg, "INDEF") != 0) {
	if(sscanf(input_buffer+51, "%lf", &measured_photometry) != 1 ||
	   sscanf(input_buffer+37, "%lf", &measured_flux) != 1 ||
	   sscanf(input_buffer+58, "%lf", &magnitude_error) != 1 ||
	   sscanf(input_buffer+69, "%s", err_msg) != 1) {
	  fprintf(stderr, "trouble (2) parsing '%s'\n", input_buffer);
	} else if(strcmp(err_msg, "NoError") == 0) {
	  // success
	  /* if (magnitude_error > 0.0 && !inhibit_snr_correction) {
	    measured_photometry -= (1.5 * magnitude_error);
	    } */
	  star->photometry = measured_photometry;
	  star->flux = measured_flux;
	  // Sadly, star->flux isn't written into the .fits starlist
	  // table. However, nlls_counts does get written.
	  star->nlls_counts = measured_flux;
	  star->validity_flags |= (PHOTOMETRY_VALID | ERROR_VALID);
	  star->magnitude_error = magnitude_error;
	  success = 1;

	  double airmass = 0.0;
	  if (obs_time_okay and (star->validity_flags & DEC_RA_VALID)) {
	    ALT_AZ alt_az(star->dec_ra, exposure_midpoint);
	    airmass = alt_az.airmass_of();
	  }
	  
	  inst_mags.push_back(AstroDB::InstMagMeasurement{star->StarName,
		measured_photometry,
		magnitude_error,
		airmass});
	}
      }
      if(!success) {
	fprintf(stderr, "photometry: bad measurement for %s\n",
		star->StarName);
	star->validity_flags &= (~PHOTOMETRY_VALID);
      }
    }
  }
  fclose(fp);
	
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
  old_list.SaveIntoFITSFile(output_filename, 1);

  if (astro_db) {
    juid_t exp_juid = astro_db->LookupExposure(output_filename);
    if (exp_juid) {
      JSON_Expression *image_exp = astro_db->FindByJUID(exp_juid);
      JSON_Expression *directive_exp = image_exp->Value("directive");
      juid_t directive = 0;
      if (directive_exp) {
	directive = directive_exp->Value_int();
      }
      juid_t inst_mags_juid =
	astro_db->AddInstMags(exp_juid, filter.NameOf(), directive, "aperture", "snr", inst_mags);
      if (par1 > 0.0) {
	astro_db->AddPSF(inst_mags_juid, par1, par2);
      }
    } else {
      fprintf(stderr, "output_filename '%s' not found in astro_db.json; nothing added to astro_db.\n",
	      output_filename);
    }
    sleep(1);			// make sure timestamp is later than the modify time for the FITS file
    astro_db->SyncAndRelease();
    delete astro_db;
  }
}
