/* This may look like C code, but it is really -*-c++-*- */
#ifndef _CORRELATE_INTERNAL_H
#define _CORRELATE_INTERNAL_H
#include <dec_ra.h>
#include "TCS.h"
#include "HGSC.h"
#include "Image.h"

/****************************************************************/
/*        One FS_DATA exists for each field_star in the small   */
/* list of field_stars passed in from each of the two star-     */
/* fields.  It holds data that cannot be put into the field-    */
/* star itself because the FS_DATA changes as the trial         */
/* reference star changes.					*/
/****************************************************************/
//****************
// FLAG VALUES
//****************
#define FLAG_ON_IMAGE 0x01

class FS_DATA {
public:
  TCS tcs_loc;			// based on gross conversion
  TCS tcs_Tloc;			// transformed location (but not
				// rotated or scaled) 

  double intensity;		// (I lied; this is independent of the
				// choice of reference star)
  int match_index;		// index into FS_ARRAY of star that
				// seems to match this star
  int reasonable;
  IStarList::IStarOneStar *match_star;
  HGSC *hgsc_star;
  TCS tcs_refined_loc;		// based on specific match
  unsigned int flags;
  double match_distance_sq;     // square of the match_star's distance in radians^2
};

class FS_ARRAY {
public:
  int NumStars;
				// We keep track of the brightness of
				// the dimmest star. This is used in
				// the correlation process; if a star
				// isn't matched, but it corresponds
				// to a brightness dimmer than the
				// "dimmest star" in the catalog, then
				// we don't feel too bad about it
				// being unmatched.

  double dimmest_star;		// magnitude of dimmest star in the array
  FS_DATA *array;

  // Three constructors
  FS_ARRAY(Image &primary_image, IStarList *l, DEC_RA *RefLocation);
  FS_ARRAY(HGSCList  *l, DEC_RA *RefLocation);
  FS_ARRAY(int size_limit);

  // Destructor
  ~FS_ARRAY(void);

  int Add(FS_DATA *new_star);	// returns index number of the
				// newly-added star

private:
  int ArraySize;		// amount of space allocated

  void SortByBrightness(void);
};

#endif
