/*  star_sync.cc -- Program to synchronize and add alignment stars
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
#include <unistd.h>		// pick up sleep(), getopt()
#include <stdlib.h>		// pick up atof()
#include <stdio.h>
#include <named_stars.h>
#include <Image.h>
#include <time.h>
#include "scope_api.h"
#include <mount_model.h>
#include <system_config.h>

// define one or the other
#define GEMINI
//#define LX200

void scope_error(char *response, ScopeResponseStatus Status) {
  const char *type = "Okay";

  if(Status == Okay) type = "Okay";
  if(Status == TimeOut) type = "TimeOut";
  if(Status == Aborted) type = "Aborted";

  fprintf(stderr, "ERROR: %s, string = '%s'\n", type, response);
}

static void usage(void) {
  fprintf(stderr,
	  "{lx200} usage: star_sync [-z] [-q] [-l] [-t] [-n starname]\n");
  fprintf(stderr,
	  "{mi250} usage: star_sync [-q] -n starname [-d dd:mm:ss -r hh:mm:ss] \n");
  exit(-2);
}

int main(int argc, char **argv) {
  int option_char;
  // int recalculate = 0;		// "-r" recalculate only
  int zero_model = 0;		// command line option "-z" zero model
  int list_model = 0;		// "-l" list model parameters
  int quick_update = 0;		// "-q" only update H0, D0
  int telescope_sync = 1;	// "-t" internal only; don't sync the scope
  Image *sync_image = 0;	// "-i image_name" use provided image
  char *starname = 0;
  char *declination_string = 0;
  char *ra_string = 0;

  while((option_char = getopt(argc, argv, "tzlqn:i:d:r:")) > 0) {
    switch (option_char) {
    case 'n':
      starname = optarg;
      break;
      
      // -r and -d are used to enter an arbitrary dec/ra position
      // instead of using -n starname.
    case 'r':
      ra_string = optarg;
      break;

    case 'd':
      declination_string = optarg;
      break;

    case 'i':
      sync_image = new Image(optarg);
      if(!sync_image) {
	fprintf(stderr, "star_sync: cannot open image file %s\n", optarg);
      }
      break;

    case 't':
      telescope_sync = 0;
      break;

    case 'z':
      zero_model++;
      break;

    case 'q':
      quick_update++;
      break;

    case 'l':
      list_model++;
      break;

    case '?':			// invalid argument
    default:
      fprintf(stderr, "Invalid argument.\n");
      exit(2);
    }
  }

  DEC_RA commanded_pos;
  if(starname && declination_string == 0 && ra_string == 0) {
    NamedStar named_star(starname);
    if(!named_star.IsKnown()) {
      fprintf(stderr, "Don't know of star named '%s'\n", starname);
      exit(2);
    }

    commanded_pos = named_star.Location();
  } else if (declination_string && ra_string && starname == 0) {
    int status = STATUS_OK;
    commanded_pos = DEC_RA(declination_string, ra_string, status);
    if (status != STATUS_OK) {
      fprintf(stderr, "star_sync: invalid dec/ra string: %s, %s\n",
	      declination_string, ra_string);
      usage();
    }
  } else {
    if((zero_model == 0 &&
	// recalculate == 0 &&
	list_model == 0) ||
       quick_update) {
      usage();
    }
  }

  if(zero_model) {
#ifdef LX200
    fprintf(stderr, "Setting model params to zero.\n");
    zero_mount_model();
    fprintf(stderr, "Erasing sync points.\n");
    clear_sync_points();
#else
    fprintf(stderr, "Don't know how to do this over serial port for Gemini\n");
#endif
  }

  if(sync_image) {
    if(!starname) {
      fprintf(stderr, "star_sync: -i options requires -n also.\n");
      exit(2);
    }
    // we define a sync star as one that has over 200,000 ADU of
    // brightness in the star "blob"
    IStarList *s_list = sync_image->GetIStarList();
    int num_blobs = 0;
    int big_blob = 0;
    for(int j=0; j<s_list->NumStars; j++) {
      if(s_list->IStarPixelSum(j) > 400000.0) {
	num_blobs++;
	big_blob = j;
      }
    }

    if(num_blobs != 1) {
      fprintf(stderr, "Cannot sync: %d blobs found.\n", num_blobs);
      exit(2);
    } else {
      double offset_pix_h;
      double offset_pix_v;

      offset_pix_h = s_list->StarCenterX(big_blob) - sync_image->width/2;
      offset_pix_v = s_list->StarCenterY(big_blob) - sync_image->height/2;

      // convert from an offset in pixels to an offset in DEC/RA
      const double RADIANSPerMICRON = atan(1.0e-6/(100.0*25.4/1000.0));
      const double XFORM_scale = 20.0*RADIANSPerMICRON;
      const double XFORM_scale_h = XFORM_scale/cos(commanded_pos.dec());
      
      fprintf(stderr, "star_sync: star catalog position = (%s, %s)\n",
	      commanded_pos.string_dec_of(),
	      commanded_pos.string_ra_of());

      DEC_RA orig_pos = commanded_pos;
      // signs: top of image has small Y, so offset_pix_v is < 0. True
      // center is farther North than the star.
      commanded_pos =
	DEC_RA(orig_pos.dec() - (offset_pix_v*XFORM_scale),
			     // left of image has small X, so
			     // offset_pix_h is < 0. True center is
			     // farther East than the star. A spot to
			     // the East has a larger RA.
	       orig_pos.ra_radians() - (offset_pix_h*XFORM_scale_h));

      fprintf(stderr, "    using image center position = (%s, %s)\n",
	      commanded_pos.string_dec_of(),
	      commanded_pos.string_ra_of());

    }
  }
      
  connect_to_scope();
  if(starname) {
    if (/*extern*/system_config.IsAP1200() and telescope_sync) {
      fprintf(stderr, "Performing hardware sync of mount\n");
      scope_sync(&commanded_pos);
    }
#if 0 // GEMINI
    if(quick_update) {
      Synchronize(commanded_pos); // a Gemini message (":CM")
    } else {
      Additional_Alignment(commanded_pos); // a Gemini message (":Cm")
    }
#endif

#if 0 // not GEMINI
    fprintf(stderr, "Saving sync point information.\n");
    add_obs_to_model(commanded_pos);
    if(telescope_sync || quick_update) {
      fprintf(stderr, "Forcing D0 & H0\n");
      quick_sync_model(commanded_pos);
    } else {
      fprintf(stderr, "Recalculating mount model.\n");
      recalculate_model();
    }
#endif
  }

#if 0 // true LX200
  if(recalculate) {
    recalculate_model();
    list_model = 1;
  }
#endif

  if(list_model) {
#if 0 // LX200
    print_mount_model(stdout);

    MountModel model;
    GetModel(model);
    printf("(all values in arcmin)\n");
    printf("Az error = %.2lf, El error = %.2lf\n",
	   model.AzError/60.0, model.ElError/60.0);
    printf("Non-perpendicularity at pole = %.2lf, at equator = %.2lf\n",
	   model.NPError/60.0, model.NEError/60.0);
    printf("Offset Dec = %.2lf, RA = %.2lf\n",
	   model.IDError/60.0, model.IHError/60.0);
    printf("Mirror flop Dec = %.2lf, RA = %.2lf\n",
	   model.FDError/60.0, model.FRError/60.0);
    printf("Flexure: counterweight = %.2lf, tube = %.2lf\n",
	   model.CFError/60.0, model.TFError/60.0);
#endif
  }
}
  
