/* This may look like C code, but it is really -*-c++-*- */
#ifndef _CORRELATE_INTERNAL2_H
#define _CORRELATE_INTERNAL2_H
#include <dec_ra.h>
#include "TCS.h"
#include "HGSC.h"
#include "Image.h"
#include "correlate2.h"
#include <wcs.h>

/****************************************************************/
/*        One FS_DATA exists for each field_star in the small   */
/* list of field_stars passed in from each of the two star-     */
/* fields.  It holds data that cannot be put into the field-    */
/* star itself because the FS_DATA changes as the trial         */
/* reference star changes.					*/
/****************************************************************/

struct IMG_DATA;

struct CAT_DATA {
public:
  CAT_DATA(HGSC &hgsc_entry) : hgsc_star(hgsc_entry) {;}
  ~CAT_DATA(void);

  int index;
  bool is_wide;
  double residual2;		// square of the residual distance
  HGSC &hgsc_star;
  std::list<IMG_DATA *> matches;
};

struct IMG_DATA {
public:
  IMG_DATA(IStarList::IStarOneStar &starlist_entry);
  IMG_DATA(const IMG_DATA *clone);
  ~IMG_DATA(void);

  int index;
  DEC_RA trial_loc;
  std::list<CAT_DATA *> matches;
  IStarList::IStarOneStar &star;
  double residual2;		// square of the residual distance
  double intensity;
  bool is_bright_star;
};

struct Solution {
  const WCS *solution_wcs {nullptr};
  int num_img_matches;
  int num_cat_matches;
};

// return true if s1 is better than s2
bool BetterThan(Solution &s1, Solution &s2);

class Truth;

struct ThreadTask {
  std::list<IMG_DATA *> star_assignments;
  Solution best_solution;
  int task_number;
  Context *context;
  std::vector<IMG_DATA *> all_image_stars;
  std::vector<CAT_DATA *> all_cat_stars;
  int num_imagestars_to_use;
  int num_catstars_to_use;
  unsigned int thread_id;
  Truth *truth;
  std::vector<int> histogram;
};

struct OneMatch {
public:
  IMG_DATA *img_star;
  CAT_DATA *cat_star;
  double distance;
};

struct AllMatches {
public:
  std::list<OneMatch> match_list;
  double avg_distance;
};


struct ResidualStatistics {
  double average;
  double median;
  double stddev;
};

//****************************************************************
//        Class: Grid
//****************************************************************
typedef std::list<CAT_DATA *> Cell;

class Grid {
public:
  Grid(Context &context,
       std::vector<CAT_DATA *> all_cat,
       double max_tolerance); // in arcradians
  ~Grid(void);

  // returns -1 if off-grid
  int LocToGridNum(const DEC_RA &loc) const;

  // "residual" holds the SQUARE of the residual distance in radians^2
  CAT_DATA *FindNearest(const DEC_RA &loc,
			double tolerance,
			double &residual,
			int max_index) const;

  DEC_RA Normalize(const DEC_RA &loc) const {
    if(!wraparound) return loc;
    return DEC_RA(loc.dec(), Normalize(loc.ra_radians()));
  }

  double Distance2(const DEC_RA &t1, const DEC_RA &t2) const;
					
private:
  bool wraparound;
  double cos_dec;
  double dec_ref;		// smallest dec
  double dec_incr;		// dec increment per grid cell
  double ra_ref;		// smallest ra
  double ra_incr;		// ra increment per grid cell
  int num_dec_cells;
  int num_ra_cells;
  int num_cells_total;
  double single_cell_tolerance;

  int matrix_index(int dec_index, int ra_index) const { return ra_index + dec_index*num_ra_cells; }

  double Normalize(double ra) const { return ((wraparound and
					 ra > M_PI) ? (ra-2*M_PI) : ra); }
  Cell **grid;
};


#endif
