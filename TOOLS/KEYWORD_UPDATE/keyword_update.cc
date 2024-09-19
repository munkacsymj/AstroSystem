/*  keyword_update.cc -- Program to fix FITS keywords (prior to
 *  submission to AAVSO image processing pipeline)
 *
 *  Copyright (C) 2016 Mark J. Munkacsy
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
#include <stdio.h>
#include <string.h>
#include <unistd.h> 		// for getopt()
#include <stdlib.h>		// for atof()
#include <Image.h>
#include <Filter.h>
#include <named_stars.h>
#include <dec_ra.h>
#include <alt_az.h>
#include <math.h>

void usage(void) {
  fprintf(stderr, "usage: keyword_update [-n objectname] -i image.fits\n");
  exit(-2);
}

const char *convert_filter_name(const char *filter) {
  if (filter[0] == '\'') filter++;
  
  if (filter[0] == 'V' && filter[1] == 'c') return "V";
  if (filter[0] == 'R' && filter[1] == 'c') return "R";
  if (filter[0] == 'I' && filter[1] == 'c') return "I";
  if (filter[0] == 'B' && filter[1] == 'c') return "B";

  if (filter[0] == 'V' ||
      filter[0] == 'R' ||
      filter[0] == 'I' ||
      filter[0] == 'B') return filter;

  fprintf(stderr, "keyword_update: filter name not recognized: '%s'\n",
	  filter);
  return "";
}
    
// Command-line options:
//
// -i image.fits                // filename of image
// -n object_name               // starname
// -f                           // force all updates
//

int main(int argc, char **argv) {
  int ch;			// option character
  char image_filename[256];
  char object_name[256];
  bool force = false;
  NamedStar *named_star = 0;

  object_name[0] = 0;
  image_filename[0] = 0;

  while((ch = getopt(argc, argv, "fn:i:")) != -1) {
    switch(ch) {
    case 'n':			// object name
      if (strlen(optarg) >= sizeof(object_name)) {
	fprintf(stderr, "keyword_update: ERROR: object name too long.\n");
	usage();
	/*NOTREACHED*/
      }
      
      strcpy(object_name, optarg);
      named_star = new NamedStar(object_name);
      if (!named_star->IsKnown()) {
	fprintf(stderr, "Don't know of star named '%s'\n", object_name);
	usage();
	/*NOTREACHED*/
      }
      break;

    case 'f':
      force = true;
      break;

    case 'i':			// image file name
      if (strlen(optarg) >= sizeof(image_filename)) {
	fprintf(stderr, "keyword_update: ERROR: image filename too long.\n");
	usage();
	/*NOTREACHED*/
      }
      strcpy(image_filename, optarg); // save the filename
      break;

    case '?':
    default:
      usage();
      /*NOTREACHED*/
    }
  }

  // must have an image
  if(image_filename[0] == 0) {
    usage();
    /*NOTREACHED*/
  }

  ImageInfo info(image_filename);

  // K E Y W O R D S                                                Checked
  // -------------------------------------------------------------------------
  //    NominalDecRA           OBJCTRA       TString                   X 
  //                           OBJCTDEC      TString                   X 
  //    Focus                  FOCUS         TLong (msec)
  //    ExposureStartTime      DATE-OBS      TString
  //    ExposureDuration       EXPOSURE      TDouble (secs)
  //    NorthIsUp              NORTH-UP      TLOGICAL
  //    RotationAngle          ROTATION      TDouble (radians)
  //    Filter                 FILTER        TString                   X
  //    Object                 OBJECT        TString                   X
  //    HourAngle              HA_NOM        TDouble (radians)
  //    Altitude               ELEVATIO      TDouble (radians)
  //    Azimuth                AZIMUTH       TDouble (radians)
  //    PSFpar1,2              PSF_P1/2      TDouble (pixels)
  //    Blur_x,y               BLUR_X/Y      TDouble (pixels)
  //    Observer               OBSERVER      TString
  //    AmbientTemp            TAMBIENT      TDouble (Degree C)
  //    CCD Temp               TCCD          TDouble (Degree C)
  //    Site Lat/Lon           SITELAT/LON   TDouble (degrees)
  //    Plate scale            CDELT1/2      TDouble (arcsec/pixel)    X
  //    CCD Gain               EGAIN         TDouble (e/ADU)           X
  //    Calibration Status     CALSTAT       TString (see comments)
  //    Focus Blur             FOC-BLUR      TDouble (pixels)
  //    Airmass                AIRMASS       TDouble (atmospheres)     X

  
  //********************************
  //    OBJECT
  //********************************
  if (force or not info.ObjectValid()) {
    if (object_name[0] == 0) {
      fprintf(stderr, "keyword_update: ERROR: need [-n objectname]\n");
      usage();
      /*NOTREACHED*/
    }
    info.SetObject(object_name);
    fprintf(stderr, "OBJECT = %s\n", object_name);
  }
  
  //********************************
  //    FILTER
  //********************************
  Filter filter{info.GetFilter()};
  info.SetValueString("FILTER", filter.AAVSO_FilterName());
  fprintf(stderr, "FILTER = '%s'\n", filter.AAVSO_FilterName());
  
  //********************************
  //    CDELT1/2
  //********************************
  info.SetValueString("CUNIT1", "DEG");
  info.SetValueString("CUNIT2", "DEG");

  double cdelt = 1.52/3600.0; // deg/pixel
  info.SetCdelt(cdelt, cdelt);
  fprintf(stderr, "CDELT1 = %lf\n", cdelt);
  fprintf(stderr, "CDELT2 = %lf\n", cdelt);

  //********************************
  //    OBJCTRA/DEC
  //********************************
  if (not info.KeywordPresent("OBJCTRA")) {
    info.SetValueString("OBJCTRA", named_star->Location().string_ra_of());
    fprintf(stderr, "OBJCTRA = %s\n", named_star->Location().string_ra_of());
  }

  if (not info.KeywordPresent("OBJCTDEC")) {
    info.SetValueString("OBJCTDEC", named_star->Location().string_fulldec_of());
    fprintf(stderr, "OBJCTDEC = %s\n", named_star->Location().string_fulldec_of());
  }

  //********************************
  //    DATE-OBS
  //********************************
  {
    JULIAN exposure_time(info.GetExposureStartTime());
    fprintf(stderr, "Exposure date = %lf\n", exposure_time.day());
    if (!named_star) {
      fprintf(stderr, "ERROR: -n starname not provided.\n");
      usage();
    }
    ALT_AZ loc_alt_az(named_star->Location(), exposure_time);
    const double altitude_deg = loc_alt_az.altitude_of() * 180.0 / M_PI;
    fprintf(stderr, "Altitude = %.2lf (deg)\n", altitude_deg);
    double airmass = 1.0/sin((M_PI/180.0)*(altitude_deg +
					   244.0/(165.0 + 47.0*pow(altitude_deg, 1.1))));
    info.SetAirmass(airmass);
    fprintf(stderr, "AIRMASS = %lf\n", airmass);
  }

  info.WriteFITS();
}
