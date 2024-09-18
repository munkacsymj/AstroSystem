#include <stdlib.h>
#include <stdio.h>
#include <string.h>		// strdup()
#include "HGSC.h"
#include "compare_photometry.h"
#include "aavso_photometry.h"

// This returns the string giving the name of the photometry sequence
// that is being used 
char last_sequence_name[32] = "";

const char *sequence_name(void) {
  return last_sequence_name;
}

void get_color_data(PhotometryRecord &pr, 
		    PhotometryColor color,
		    const char *field) {
  if (field && *field) {
    pr.PR_colordata.Add(color, atof(field));
  }
}

PhotometryColor valid_colors[] = { PHOT_V, PHOT_B, PHOT_R, PHOT_I };

// return true if a complete match
bool all_colors_match(MultiColorData *mcd1, MultiColorData *mcd2) {
  bool mismatch = false;
  
  for (unsigned int i=0; i< (sizeof(valid_colors)/sizeof(valid_colors[0])); i++) {
    const PhotometryColor color = valid_colors[i];
    if (mcd1->IsAvailable(color)) {
      if (!mcd2->IsAvailable(color)) {
	mismatch = true;
      } else {
	if (fabs(mcd1->Get(color) - mcd2->Get(color)) >= 0.001 or
	    fabs(mcd1->GetUncertainty(color) - mcd2->GetUncertainty(color)) >= 0.001) {
	  mismatch = true;
	}
      }
    } else {
      if (mcd2->IsAvailable(color)) {
	mismatch = true;
      }
    }
    fprintf(stderr, "        color index %d: mismatch = %d [%d,%d]\n",
	    i, mismatch, mcd1->IsAvailable(color), mcd2->IsAvailable(color));
  }
  return !mismatch;
}

// If update_flag is true, will actually modify the HGSC in-memory
// data to match the AAVSO photometry. 
int compare_photometry(PhotometryRecord *pr, // one line of AAVSO photometry
		       HGSCList *catalog,
		       int update_flag) {
  int num_stars_different = 0;
  
  // Find the corresponding catalog star in our catalog
  HGSCIterator it(*catalog);
  HGSC *one_star;
  double closest = 999.9;
  HGSC *closest_star = 0;

  // find the star in the catalog that is closest to this photometry
  // reference 
  for (one_star = it.First(); one_star; one_star = it.Next()) {
    double delta_ra; // arc-radians
    double delta_dec; // arc-radians

    delta_dec = one_star->location.dec() -
      pr->PR_location.dec();
    delta_ra = cos(one_star->location.dec()) *
      (one_star->location.ra_radians() - pr->PR_location.ra_radians());

    double delta = sqrt(delta_dec*delta_dec + delta_ra*delta_ra);
    if (delta < closest) {
      closest = delta;
      closest_star = one_star;
    }
  }

  // "closest" must be less than 2 arcsec
  if (closest > M_PI / (90.0*3600.0)) {
    fprintf(stderr, "photometry star %s > 2arcsec from any catalog star\n",
	    pr->PR_AUID);
    if (update_flag) {
      // add a new star
      char label_text[64];
      sprintf(label_text, "AAVSO_%s", pr->PR_chart_label);
      HGSC *new_star = new HGSC(pr->PR_location.dec(),
				pr->PR_location.ra_radians(),
				pr->PR_V_mag,
				label_text);
      new_star->is_check = 1;
      new_star->photometry = pr->PR_V_mag;
      new_star->photometry_valid = 1;
      new_star->A_unique_ID = strdup(pr->PR_AUID);
      new_star->multicolor_data = pr->PR_colordata;
      catalog->Add(*new_star);
    }
    num_stars_different++;
  } else {
    fprintf(stderr, "checking %s: ", closest_star->A_unique_ID);
    // we have our match!
    if ((!closest_star->photometry_valid) ||
	(closest_star->photometry_valid && fabs(closest_star->photometry -
						pr->PR_V_mag) > 0.0001) ||
	!all_colors_match(&closest_star->multicolor_data,
			  &pr->PR_colordata)) {
      fprintf(stderr, " mismatch.\n");
      num_stars_different++;
    } else {
      fprintf(stderr, " good.\n");
    }
    if (update_flag) {
      fprintf(stderr, "    Updating %s\n", closest_star->A_unique_ID);
      closest_star->photometry = pr->PR_V_mag;
      closest_star->is_check = 1;
      closest_star->photometry_valid = 1;
      if (closest_star->A_unique_ID) free(closest_star->A_unique_ID);
      closest_star->A_unique_ID = strdup(pr->PR_AUID);
      if (closest_star->report_ID) free(closest_star->report_ID);
      closest_star->report_ID = strdup(pr->PR_chart_label);
      closest_star->multicolor_data = pr->PR_colordata;
    }
  }

  return num_stars_different ? COMPARE_MISMATCH : COMPARE_MATCHES;
}    
      

      
	

