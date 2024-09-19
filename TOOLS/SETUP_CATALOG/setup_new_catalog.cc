/*  setup_new_catalog.cc -- creates a catalog for a new object
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
#include <math.h>
#include <stdio.h>
#include <unistd.h> 		// for getopt()
#include <stdlib.h>		// for atof()
#include <string.h>		// strdup()
#include <named_stars.h>
#include "HGSC.h"
#include <fitsio.h>
#include <gendefs.h>

// print fitsio error messages
static void printerror( int status);
HGSCList *ExtractWidefield(HGSCList *fullList,
			   double   north_lim,
			   double   south_lim,
			   double   east_lim,
			   double   west_lim,
			   int      wrap_occurs,
			   double   image_width, // radians
			   double   image_height);

class HGSC_REGIONS {
public:
  // These dec/ra methods provide all information in radians
  double north_dec(int region) { return regions[region].north_ext; }
  double south_dec(int region) { return regions[region].south_ext; }
  double high_ra(int region) { return regions[region].high_ra; }
  double low_ra(int region) { return regions[region].low_ra; }
  int region_number(int region) { return
				       regions[region].region_number; }
  int number_regions(void) { return region_count; }
  const char *region_filename(int region);

  HGSC_REGIONS(void);
  ~HGSC_REGIONS(void);
private:
  int region_count;
  struct one_region {
    double north_ext;
    double south_ext;
    double high_ra;
    double low_ra;
    int region_number;
  } *regions;
};

const char *
HGSC_REGIONS::region_filename(int region) {
  struct one_region *this_region = regions+region;
  char declination_letter;
  int declination_band;
  static char dirname_code[16];

  if(this_region->north_ext > 0.0) {
    declination_letter = 'n';
    declination_band = ((int)(0.1+this_region->south_ext*180.0/(M_PI*7.5)));
  } else {
    declination_letter = 's';
    declination_band = ((int)(-(-0.1+this_region->north_ext*180.0/(M_PI*7.5))));
  }
  sprintf(dirname_code, "%c%02d%c0/%04d.gsc",
	  declination_letter,
	  (int)(7.5*declination_band+0.1),
	  (declination_band % 2 ? '3' : '0'),
	  this_region->region_number);
  return (const char *) dirname_code;
}

HGSC_REGIONS::~HGSC_REGIONS(void) {
  delete [] regions;
}

// We maintain a file in /usr/local/ASTRO that holds a list of all the
// HGSC regions.  In that file, each region has its four corners
// listed and enough information to figure out the HGSC catalog
// filename for that region.
HGSC_REGIONS::HGSC_REGIONS(void) {
  const char *hgsc_index_file = HGSC_CATALOG_DIR "/hgsc_regions.fits";
  fitsfile *fptr;       /* pointer to the FITS file, defined in fitsio.h */
  int status = 0;

  /* open the file, verify we have read access to the file. */
  if ( fits_open_file(&fptr, hgsc_index_file, READONLY, &status) ) {
    printerror( status );
    exit(2);
  }
  
  // find the Table extent
  do {
    int hdu_type;
    if(fits_get_hdu_type(fptr, &hdu_type, &status)) {
      printerror(status);
    }
    if(hdu_type == ASCII_TBL) break;
    if(fits_movrel_hdu(fptr, 1, &hdu_type, &status)) {
      fprintf(stderr, "Error trying to find GSC table.\n");
      printerror(status);
    }
  } while(1);
  fprintf(stderr, "found HGSC index table okay.\n");

  {

    struct gsc_fields {
      int colnum;
      const char *colname;
    } gsc_col_data[] = {
      { 0, "REG_NO" },
      { 0, "RA_H_LOW" },		// hours
      { 0, "RA_M_LOW" },		// mins
      { 0, "RA_S_LOW" },		// secs
      { 0, "RA_H_HI" },
      { 0, "RA_M_HI" },
      { 0, "RA_S_HI" },
      { 0, "DECSI_LO" },		// sign
      { 0, "DEC_D_LO" },
      { 0, "DEC_M_LO" },
      { 0, "DECSI_HI" },		// sign
      { 0, "DEC_D_HI" },
      { 0, "DEC_M_HI" },
    };

    static const int REG_NO   = 0;
    static const int RA_H_LOW = 1;
    static const int RA_M_LOW = 2;
    static const int RA_S_LOW = 3;
    static const int RA_H_HI  = 4;
    static const int RA_M_HI  = 5;
    static const int RA_S_HI  = 6;
    static const int DECSI_LO = 7;
    static const int DEC_D_LO = 8;
    static const int DEC_M_LO = 9;
    static const int DECSI_HI = 10;
    static const int DEC_D_HI = 11;
    static const int DEC_M_HI = 12;

    static const int num_fields = sizeof(gsc_col_data)/sizeof(gsc_col_data[0]);

    // fill in the gsc_col_data[] array with column numbers
    // corresponding to the desired column names
    int i;
    for(i=0; i<num_fields; i++) {
      char colname[32];
      strcpy(colname, gsc_col_data[i].colname);
      if(fits_get_colnum(fptr, CASEINSEN,
			 colname,
			 &gsc_col_data[i].colnum,
			 &status)) {
	fprintf(stderr, "Error finding gsc column named %s\n",
		gsc_col_data[i].colname);
	printerror(status);
      }
    }

    // Now loop through all records in the table
    long num_rows;
    if(fits_get_num_rows(fptr, &num_rows, &status)) {
      printerror(status);
    }
    region_count = num_rows;

    regions = new one_region[region_count];

    int *region_nums = new int [num_rows];
    int *ra_h_low    = new int [num_rows];
    int *ra_m_low    = new int [num_rows];
    double *ra_s_low = new double [num_rows];
    int *ra_h_hi     = new int [num_rows];
    int *ra_m_hi     = new int [num_rows];
    double *ra_s_hi  = new double [num_rows];
    char **decsi_lo  = new char * [num_rows];
    int *dec_d_lo    = new int [num_rows];
    double *dec_m_lo = new double [num_rows];
    char **decsi_hi  = new char * [num_rows];
    int *dec_d_hi    = new int [num_rows];
    double *dec_m_hi = new double [num_rows];

    static const int GSC_SIGN_LEN = 2;
    char *namepool = new char[2 * num_rows * GSC_SIGN_LEN];
    for(i=0; i<num_rows; i++) {
      decsi_lo[i] = namepool + 2*i*GSC_SIGN_LEN;
      decsi_hi[i] = namepool + (2*i+1)*GSC_SIGN_LEN;
    }

    fits_read_col(fptr, TSTRING, gsc_col_data[DECSI_LO].colnum, 1, 0,
		  num_rows, 0, decsi_lo, 0, &status);
    fits_read_col(fptr, TSTRING, gsc_col_data[DECSI_HI].colnum, 1, 0,
		  num_rows, 0, decsi_hi, 0, &status);
    fits_read_col(fptr, TINT, gsc_col_data[RA_H_LOW].colnum, 1, 0,
		  num_rows, 0, ra_h_low, 0, &status);
    fits_read_col(fptr, TINT, gsc_col_data[RA_M_LOW].colnum, 1, 0,
		  num_rows, 0, ra_m_low, 0, &status);
    fits_read_col(fptr, TDOUBLE, gsc_col_data[RA_S_LOW].colnum, 1, 0,
		  num_rows, 0, ra_s_low, 0, &status);
    fits_read_col(fptr, TINT, gsc_col_data[RA_H_HI].colnum, 1, 0,
		  num_rows, 0, ra_h_hi, 0, &status);
    fits_read_col(fptr, TINT, gsc_col_data[RA_M_HI].colnum, 1, 0,
		  num_rows, 0, ra_m_hi, 0, &status);
    fits_read_col(fptr, TDOUBLE, gsc_col_data[RA_S_HI].colnum, 1, 0,
		  num_rows, 0, ra_s_hi, 0, &status);
    fits_read_col(fptr, TINT, gsc_col_data[DEC_D_LO].colnum, 1, 0,
		  num_rows, 0, dec_d_lo, 0, &status);
    fits_read_col(fptr, TDOUBLE, gsc_col_data[DEC_M_LO].colnum, 1, 0,
		  num_rows, 0, dec_m_lo, 0, &status);
    fits_read_col(fptr, TINT, gsc_col_data[DEC_D_HI].colnum, 1, 0,
		  num_rows, 0, dec_d_hi, 0, &status);
    fits_read_col(fptr, TDOUBLE, gsc_col_data[DEC_M_HI].colnum, 1, 0,
		  num_rows, 0, dec_m_hi, 0, &status);
    fits_read_col(fptr, TINT, gsc_col_data[REG_NO].colnum, 1, 0,
		  num_rows, 0, region_nums, 0, &status);

    if(status != 0) {
      fprintf(stderr, "Error reading fits columns\n");
      printerror(status);
    }

    for(i=0; i<num_rows; i++) {
      one_region *this_region= regions + i;

      this_region->region_number = region_nums[i];
      this_region->north_ext = (M_PI/180.0) * (dec_d_hi[i] + dec_m_hi[i]/60.0);
      if(decsi_hi[i][0] == '-')
	this_region->north_ext = -this_region->north_ext;
      this_region->south_ext = (M_PI/180.0) * (dec_d_lo[i] + dec_m_lo[i]/60.0);
      if(decsi_lo[i][0] == '-')
	this_region->south_ext = -this_region->south_ext;
      if(this_region->north_ext < 0.0) {
	double swap = this_region->north_ext;
	this_region->north_ext = this_region->south_ext;
	this_region->south_ext = swap;
      }
      this_region->high_ra = (M_PI/12.0) *
	(ra_h_hi[i] + ra_m_hi[i]/60.0 + ra_s_hi[i]/3600.0);
      if(this_region->high_ra == 0.0)
	this_region->high_ra = (2.0*M_PI);

      this_region->low_ra = (M_PI/12.0) *
	(ra_h_low[i] + ra_m_low[i]/60.0 + ra_s_low[i]/3600.0);
    }

    fprintf(stderr, "total of %ld rows read\n", num_rows);

    delete [] region_nums;
    delete [] ra_h_low;
    delete [] ra_m_low;
    delete [] ra_s_low;
    delete [] ra_h_hi;
    delete [] ra_m_hi;
    delete [] ra_s_hi;
    delete [] dec_d_lo;
    delete [] dec_m_lo;
    delete [] dec_d_hi;
    delete [] dec_m_hi;
    delete [] namepool;
    delete [] decsi_lo;
    delete [] decsi_hi;

  }

  if ( fits_close_file(fptr, &status) )
    printerror( status );
}

  
/****************************************************************/
/*        usage()						*/
/****************************************************************/

void usage(void) {
  fprintf(stderr, "usage: setup_catalog: -n starname \n");
  exit(2);
}

/****************************************************************/
/*        main()						*/
/****************************************************************/

int main(int argc, char **argv) {
  int ch;			// option character
  char *starname = 0;
  double radius_minutes = 12.0;	// default
  int radius_entered = 0;	// 1 if entered on command line
  int wide = 0;
  char *output_filename = 0;

  // Command line options:
  // -w: means that we create the "widefield" HGSC catalog that
  // contains only the bright stars
  //
  // -n starname -r radius(minutes) -w
  //

  while((ch = getopt(argc, argv, "wo:r:n:")) != -1) {
    switch(ch) {
    case 'w':			// widefield version
      wide = 1;
      break;

    case 'o':
      output_filename = optarg;
      break;

    case 'n':			// starname
      starname = optarg;
      break;

    case 'r':			// radius
      radius_entered = 1;	// yes, entered on command line
      radius_minutes = atof(optarg);
      break;

    case '?':
    default:
      usage();
    }
  }

  if(output_filename == 0) output_filename = starname;

  argc -= optind;
  argv += optind;

  DEC_RA center;
  if(starname == 0) {
    if(argc != 2) {
      usage();
    } else {
      int conversion_status;
      center = DEC_RA(argv[0], argv[1], conversion_status);
      if(conversion_status != STATUS_OK) {
	fprintf(stderr, "setup_catalog: arguments wouldn't parse.\n");
	exit(2);
      }
    }
  } else {

    // for widefield catalogs, work a 30-minute radius unless operator
    // provided something different on the command line.
    if(wide && !radius_entered) radius_minutes = 30.0;

    // Read the catalog file to find the reference location for this
    // star. The reference location becomes the centerpoint for the
    // search through the HGSC as we extract stars to put into the
    // catalog file.
    NamedStar named_star(starname);
    if(!named_star.IsKnown()) {
      fprintf(stderr, "Don't know of star named '%s'\n", starname);
      exit(2);
    }
    center = named_star.Location();
  }

  fprintf(stderr, "Using radius of %f minutes\n", radius_minutes);
  const double radius_radians = (radius_minutes/60.0) * (M_PI/180.0);
  const double adj = cos(center.dec());

  // We make simple-minded, rectangular assumptions.  Should be okay
  // except really, really close to the pole.
  double NorthLimitRadians = (center.dec() + radius_radians);
  double SouthLimitRadians = (center.dec() - radius_radians);
  double EastLimitRadians = center.ra_radians() - radius_radians/adj;
  double WestLimitRadians = center.ra_radians() + radius_radians/adj;

  int wrap_occurs = 0;
  if(EastLimitRadians < 0.0) {
    EastLimitRadians += (2.0*M_PI);
    wrap_occurs = 1;
  }
  if(WestLimitRadians >= 2.0*M_PI) {
    WestLimitRadians -= (2.0*M_PI);
    wrap_occurs = 1;
  }

  // get list of all HGSC regions
  HGSC_REGIONS region_list;
  int number_regions = region_list.number_regions();
  int region_matches = 0;
  HGSCList AnswerList;

  for(int i=0; i< number_regions; i++) {
    if(region_list.north_dec(i) < SouthLimitRadians) continue;
    if(region_list.south_dec(i) > NorthLimitRadians) continue;
    if(wrap_occurs) {
      if(region_list.high_ra(i) < EastLimitRadians &&
	 region_list.low_ra(i) > WestLimitRadians) continue;
    } else {
      // no wrap
      if(region_list.high_ra(i) < EastLimitRadians) continue;
      if(region_list.low_ra(i) > WestLimitRadians) continue;
    }

    // Okay! We didn't trigger on one of those "continue" clauses, so
    // this file is valid to read in
    region_matches++;

    fitsfile *fptr;       /* pointer to the FITS file, defined in fitsio.h */
    int status = 0;
    char filename[256];
    // sprintf(filename, "/cd/gsc/%s", region_list.region_filename(i));
    sprintf(filename, HGSC_CATALOG_DIR "/%s", region_list.region_filename(i));

    /* open the file, verify we have read access to the file. */
    fprintf(stderr, "Reading %s\n", filename);
    if ( fits_open_file(&fptr, filename, READONLY, &status) ) {
      printerror( status );
      goto close_file;
    }

    do {
      int hdu_type;
      if(fits_get_hdu_type(fptr, &hdu_type, &status)) {
	printerror(status);
	goto close_file;
      }
      if(hdu_type == ASCII_TBL) break;
      if(fits_movrel_hdu(fptr, 1, &hdu_type, &status)) {
	fprintf(stderr, "Error trying to find GSC table.\n");
	printerror(status);
	goto close_file;
      }
    } while(1);
    fprintf(stderr, "found table okay.\n");

    {

      struct gsc_fields {
	int colnum;
	const char *colname;
      } gsc_col_data[] = {
	{ 0, "GSC_ID" },
	{ 0, "RA_DEG" },
	{ 0, "DEC_DEG" },
	{ 0, "MAG" },
      };

      static const int GSC_ID = 0;
      static const int RA_DEG = 1;
      static const int DEC_DEG = 2;
      static const int MAG = 3;

      static const int num_fields = sizeof(gsc_col_data)/sizeof(gsc_col_data[0]);

      // fill in the gsc_col_data[] array with column numbers
      // corresponding to the desired column names
      int k;
      for(k=0; k<num_fields; k++) {
	char colname[32];
	strcpy(colname, gsc_col_data[k].colname);
	if(fits_get_colnum(fptr, CASEINSEN,
			   colname,
			   &gsc_col_data[k].colnum,
			   &status)) {
	  fprintf(stderr, "Error finding gsc column named %s\n",
		  gsc_col_data[k].colname);
	  printerror(status);
	  goto close_file;
	}
      }

      // Now loop through all records in the table
      long num_rows;
      if(fits_get_num_rows(fptr, &num_rows, &status)) {
	printerror(status);
	goto close_file;
      }

      char **col_names = new char * [num_rows];
      double *ra_deg = new double [num_rows];
      double *dec_deg = new double [num_rows];
      double *mag = new double [num_rows];

      static const int GSC_NAME_LEN = 16;
      char *namepool = new char[num_rows * GSC_NAME_LEN];
      for(k=0; k<num_rows; k++) {
	col_names[k] = namepool + k*GSC_NAME_LEN;
      }

      fits_read_col(fptr, TSTRING, gsc_col_data[GSC_ID].colnum, 1, 0,
		    num_rows, 0, col_names, 0, &status);
      fits_read_col(fptr, TDOUBLE, gsc_col_data[RA_DEG].colnum, 1, 0,
		    num_rows, 0, ra_deg, 0, &status);
      fits_read_col(fptr, TDOUBLE, gsc_col_data[DEC_DEG].colnum, 1, 0,
		    num_rows, 0, dec_deg, 0, &status);
      fits_read_col(fptr, TDOUBLE, gsc_col_data[MAG].colnum, 1, 0,
		    num_rows, 0, mag, 0, &status);

      const char *prev_starname = "";
      for(k=0; k<num_rows; k++) {
	const double ra_radians = ra_deg[k] * M_PI/180.0;
	const double dec_radians = dec_deg[k] * M_PI/180.0;

	if(dec_radians <= NorthLimitRadians &&
	   dec_radians >= SouthLimitRadians) {
	  if((wrap_occurs && (ra_radians >= EastLimitRadians ||
			      ra_radians <= WestLimitRadians)) ||
	     ((!wrap_occurs) && (ra_radians >= EastLimitRadians &&
				 ra_radians <= WestLimitRadians))) {
	    // already get the star?
	    if(strcmp(prev_starname, col_names[k]) != 0) {
	      // good star!!
	      char starname[36];
	      sprintf(starname, "GSC%05d-%s",
		      region_list.region_number(i),
		      col_names[k]);
	      HGSC *new_hgsc = new HGSC(dec_radians,
					ra_radians,
					mag[k],
					strdup(starname));
	      AnswerList.Add(*new_hgsc);
	      prev_starname = col_names[k];
	    }
	  }
	}
      }
      fprintf(stderr, "star list now holds %d stars.\n",
	      AnswerList.length());

      delete [] col_names;
      delete [] ra_deg;
      delete [] dec_deg;
      delete [] mag;
      delete [] namepool;

    }

  close_file:
    if ( fits_close_file(fptr, &status) )
      printerror( status );
  }
  if(region_matches == 0) {
    fprintf(stderr, "Nothing found?? Try other CD??\n");
  } else {
    char HGSCfilename[132];
    sprintf(HGSCfilename, CATALOG_DIR "/%s", output_filename);

    if(wide) {
      strcat(HGSCfilename, ".wide");
      HGSCList *WideList =
	ExtractWidefield(&AnswerList,
			 NorthLimitRadians,
			 SouthLimitRadians,
			 EastLimitRadians,
			 WestLimitRadians,
			 wrap_occurs,
			 13.5 * (M_PI/(180.0*60.0*cos(SouthLimitRadians))),
			 13.5 * (M_PI/(180.0*60.0)));
      fprintf(stderr, "Widefield list holds %d stars.\n", WideList->length());
      WideList->Write(HGSCfilename);
    } else {
      // See if the file exists by trying to open the file for reading.
      FILE *fp = fopen(HGSCfilename, "r");
      if(fp) {
	fprintf(stderr,
		"setup_catalog: error: catalog file already exists.\n");
	exit(2);
      }

      AnswerList.Write(HGSCfilename);
    }
  }
}

static void printerror( int status)
{
  /*****************************************************/
  /* Print out cfitsio error messages and exit program */
  /*****************************************************/


  if (status) {
    fits_report_error(stderr, status); /* print error report */

    exit( status );    /* terminate the program, returning error status */
  }
  return;
}

struct GSC_Star {
  HGSC *hgsc_star;
  int  included;		// either 0 or 1
  int  min_frame_x;
  int  min_frame_y;
  int  max_frame_x;
  int  max_frame_y;
};

// must return -1 if d1 should come before d2
static int compare_hgsc(const void *u1, const void *u2) {
  const GSC_Star *d1 = (GSC_Star *)u1;
  const GSC_Star *d2 = (GSC_Star *)u2;
  double delta = d1->hgsc_star->magnitude - d2->hgsc_star->magnitude;
  if(delta == 0.0) return 0;
  return (delta < 0.0) ? -1 : 1;
}

HGSCList *ExtractWidefield(HGSCList *fullList,
			   double   north_lim,
			   double   south_lim,
			   double   east_lim,
			   double   west_lim,
			   int      wrap_occurs,
			   double   image_width, // radians
			   double   image_height) {
  const double frame_width = image_width;
  const double frame_height = image_height;
  const double frame_delta_width = frame_width/7.0;
  const double frame_delta_height = frame_height/7.0;

  double universe_width = west_lim - east_lim;
  if(wrap_occurs) universe_width += 2.0*M_PI;
  const double universe_height = north_lim - south_lim;

  const int num_frames_wide =
    (int) (0.5 + (universe_width - frame_width)/frame_delta_width);
  const int num_frames_high =
    (int) (0.5 + (universe_height - frame_height)/frame_delta_height);

  const int num_stars = fullList->length();
  GSC_Star *GSC_array;
  GSC_array = new GSC_Star[num_stars];
  if(!GSC_array) {
    fprintf(stderr, "cannot allocate memory for GSC_Star array.");
    return 0;
  }

  HGSCIterator i(*fullList);
  HGSC *one_star;
  int star_id = 0;
  for(one_star = i.First(); one_star; one_star = i.Next()) {
    GSC_array[star_id].hgsc_star = one_star;
    GSC_array[star_id].included = 0;

    double x = one_star->location.ra_radians() - east_lim;
    if(x < 0.0) x += 2.0*M_PI;
    const double y = one_star->location.dec() - south_lim;

    // convention: lower-left (south-east) frame is (0,0)

    int first_x = 1 + (int)((x - frame_width)/frame_delta_width);
    int last_x  = (int)(x/frame_delta_width);
    int first_y = 1 + (int)((y - frame_height)/frame_delta_height);
    int last_y  = (int)(y/frame_delta_height);

    GSC_array[star_id].min_frame_x = (first_x < 0 ? 0 : first_x);
    GSC_array[star_id].min_frame_y = (first_y < 0 ? 0 : first_y);
    GSC_array[star_id].max_frame_x =
      (last_x < num_frames_wide ? last_x : (num_frames_wide - 1));
    GSC_array[star_id].max_frame_y =
      (last_y < num_frames_high ? last_y : (num_frames_high - 1));
    
    star_id++;
  }

  qsort(GSC_array, num_stars, sizeof(GSC_Star), compare_hgsc);

  // now we have a sorted array of stars (sorted by brightness).
  int framex, framey;
  for(framex = 0; framex < num_frames_wide-1; framex++) {
    for(framey = 0; framey < num_frames_high-1; framey++) {
      // set flag on the first N in this frame
      int in_frame = 0;
      const int STARS_PER_FRAME = 10;

      for(star_id = 0; star_id < num_stars; star_id++) {
	GSC_Star *star = &GSC_array[star_id];

	if(star->min_frame_x <= framex &&
	   star->min_frame_y <= framey &&
	   star->max_frame_x >= framex &&
	   star->max_frame_y >= framey) {
	  star->included = 1;
	  if(in_frame++ >= STARS_PER_FRAME) break;
	}
      }
    }
  }

  // now build a new HGSC List
  HGSCList *CulledList = new HGSCList();

  for(star_id = 0; star_id < num_stars; star_id++) {
    GSC_Star *star = &GSC_array[star_id];

    if(star->included) {
      CulledList->Add(*(star->hgsc_star));
    }
  }

  return CulledList;
}
