/*  auto_sync.cc -- Automatically build a mount model
 *
 *  Copyright (C) 2017 Mark J. Munkacsy

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

/* 
   USAGE PROFILES:

Completely Raw Initial Alignment:
    This will be a two-phase alignment. Invoke as 
          ./auto_sync_gm2000 -l

Refinement of an Already-Good Alignment:
    This will be a one-phase alignment. Invoke as
          ./auto_sync_gm2000 -l -r

Refinement of a Poor Alignment:

 */
#include <unistd.h>		// getopt()
#include <string.h>		// strcmp
#include <ctype.h>		// isspace()
#include <stdio.h>
#include "bright_star.h"
#include "alt_az.h"
#include "visibility.h"
#include "scope_api.h"
#include "camera_api.h"
#include "mount_model.h"
#include <list>
#include <stdlib.h>		// random()

using namespace std;

char *last_image_filename;
bool build_syncfile = false;
bool erase_syncfile = true;
bool load_syncfile = false;

struct AlignmentStats {
  int num_stars;
  int num_align_points;
  int num_skipped;
};

enum ObsStatus {
  SKIP,
  PLANNED,
  WORKING,
  COMPLETED,
};

struct OneAlignmentStar {
  OneBrightStar *star;
  bool east_of_meridian;
  ObsStatus status;
  double dec_adjust;		// add to catalog to get observed
  double ra_adjust;		// add to RA (*not* HA) to get observed
};
  

typedef list<OneAlignmentStar *> lsl; // local_star_list
lsl stars;

void GetStats(AlignmentStats *stats) {
  stats->num_stars =
    stats->num_align_points =
    stats->num_skipped = 0;
  
  list <OneAlignmentStar *>::iterator oit;
  for (oit = stars.begin(); oit != stars.end(); oit++) {
    stats->num_stars++;
    
    if ((*oit)->status == SKIP) {
      stats->num_skipped++;
    } else if ((*oit)->status == COMPLETED) {
      stats->num_align_points++;
    }
  }
}

void list_stars(void) {
  list <OneAlignmentStar *>::iterator oit;
  int recount = 0;
  for (oit = stars.begin(); oit != stars.end(); oit++) {
    DEC_RA loc = (*oit)->star->Location(); 
    fprintf(stderr, "    [%s, %s] mag %.2lf\n",
	    loc.string_dec_of(), loc.string_ra_of(), (*oit)->star->Magnitude());
    recount++;
  }
  fprintf(stderr, "... %d stars listed above.\n", recount);
}

void MakeStarList(int target_quantity) {
  BrightStarList *l = new BrightStarList(80.0*M_PI/180,	 // max_dec
					 -35*M_PI/180,   // min_dec
					 2*M_PI,	 // biggest RA
					 0.0,		 // smallest RA
					 3.0,		 // dimmest
					 -99.9);	 // brightest
  
  BrightStarIterator it(l);
  JULIAN now(time(0));

  int star_count = 0;

  for (OneBrightStar *s = it.First(); s; s = it.Next()) {
    DEC_RA loc = s->Location();
    ALT_AZ loc_alt_az(loc, now);
    if (IsVisible(loc_alt_az, now)) {
      star_count++;
    }
  }

  double selection_fraction = 0.0;
  
  if (star_count <= target_quantity) {
    fprintf(stderr, "Using all %d stars visible.\n", star_count);
    selection_fraction = 1.0;
  } else {
    fprintf(stderr, "Selecting about %d stars from %d visible.\n",
	    target_quantity, star_count);
    selection_fraction = ((double)target_quantity/(double)star_count);
  }

  long threshold = (long) (selection_fraction * (double) 0x7fffffff);
  for (OneBrightStar *s = it.First(); s; s = it.Next()) {
    DEC_RA loc = s->Location();
    ALT_AZ loc_alt_az(loc, now);
    if (IsVisible(loc_alt_az, now)) {
      if (random() <= threshold) {
	OneAlignmentStar *as = new OneAlignmentStar;
	as->star = s;
	as->status = PLANNED;
	as->east_of_meridian = (loc_alt_az.azimuth_of() < 0.0);
	stars.push_back(as);
	fprintf(stderr, "X");
      } else {
	fprintf(stderr, "-");
      }
    }
  }
  fprintf(stderr, "\n%ld stars in final selection:\n", stars.size());
  list_stars();
}

const double exposure_time = 1.0;

OneAlignmentStar *pick_first_star(void) {
  list <OneAlignmentStar *>::iterator it;
  OneAlignmentStar *pick = 0;
  DEC_RA orig_loc = ScopePointsAt();
  JULIAN now(time(0));
  const double target_ha = orig_loc.hour_angle(now);
  const double target_dec = orig_loc.dec();
  double smallest_offset = 999999.9;

  for (it = stars.begin(); it != stars.end(); it++) {
    DEC_RA loc = (*it)->star->Location(); 
    ALT_AZ loc_alt_az(loc, now);
    const double ha = loc.hour_angle(now);
    const double dec = loc.dec();
    const double offset_dec = target_dec - dec;
    const double offset_ha = target_ha - ha;
    const double offset_sq = offset_dec*offset_dec + offset_ha*offset_ha;

    if (IsVisible(loc_alt_az, now) &&
	offset_sq < smallest_offset) {
      pick = (*it);
      smallest_offset = offset_sq;
    }
  }
  if (!pick) {
    fprintf(stderr, "error: pick_first_star() failed.\n");
    exit(-2);
  }
  return pick;
}
  
// Two flavors of pick_next_star(). One understands flips, and will
// try everything on the current side of the meridian before doing a
// flip and pursuing the other side. The other one doesn't flip; it
// just sticks to the requested side. Not sure this handles the region
// around the pole efficiently, but that's a future
// optimization. (Maybe it would be better to use Azimuth than to use
// Hour Angle??)

OneAlignmentStar *pick_next_star(DEC_RA &start_loc, bool ha_is_negative,
				 double *delta_dec, double *delta_ra);

OneAlignmentStar *pick_next_star(double *delta_dec, double *delta_ra) {
  DEC_RA start_loc = ScopePointsAt();
  bool ha_is_negative = (GetScopeHA() < 0.0);

  OneAlignmentStar *pick = pick_next_star(start_loc, ha_is_negative,
					  delta_dec, delta_ra);
  // maybe time to flip axis?
  if (pick == 0) {
    pick = pick_next_star(start_loc, !ha_is_negative,
			  delta_dec, delta_ra);
    if(pick) {
      fprintf(stdout, "Performing meridian flip.\n");
    }
  }
  return pick;
}

OneAlignmentStar *pick_next_star(DEC_RA &start_loc, bool ha_is_negative,
				 double *delta_dec, double *delta_ra) {
  //list_stars();
  list <OneAlignmentStar *>::iterator it;
  OneAlignmentStar *closest_so_far = 0;
  double smallest_distance = 999.9;
  JULIAN now(time(0));

  for (it = stars.begin(); it != stars.end(); it++) {
    DEC_RA loc = (*it)->star->Location();
    ALT_AZ loc_alt_az(loc, now);
    const double ha = loc.hour_angle(now);

    if((ha_is_negative && ha > 0.0) ||
       (ha < 0.0 && !ha_is_negative) ||
       !IsVisible(loc_alt_az, now)) {
      continue;
    }

    const double delta_dec = start_loc.dec() - (*it)->star->Location().dec();
    const double delta_ra = start_loc.ra_radians() - (*it)->star->Location().ra_radians();
    const double delta_ra_arc = delta_ra*cos(loc.dec());
    const double delta_sq = delta_dec*delta_dec + delta_ra_arc*delta_ra_arc;

    if ((*it)->status == PLANNED &&
	delta_sq < smallest_distance) {
      closest_so_far = (*it);
      smallest_distance = delta_sq;
    }
  }

  if (closest_so_far) {
    // find nearest star to grab position offsets
    DEC_RA ref_loc = closest_so_far->star->Location();
    const double ref_dec = ref_loc.dec();
    const double ref_ra = ref_loc.ra_radians();
    double smallest_dist_sq = 999.9;
    double best_delta_dec = 0.0;
    double best_delta_ra = 0.0;
    
    for (it = stars.begin(); it != stars.end(); it++) {
      if ((*it)->status == COMPLETED) {
	DEC_RA loc = (*it)->star->Location();
	const double this_dec = loc.dec();
	const double this_ra = loc.ra_radians();
	const double delta_dec1 = ref_dec - this_dec;
	const double delta_ra1 = ref_ra - this_ra;
	const double delta_sq = delta_dec1 * delta_dec1 + delta_ra1 * delta_ra1;

	if (delta_sq < smallest_dist_sq) {
	  smallest_dist_sq = delta_sq;
	  best_delta_dec = (*it)->dec_adjust;
	  best_delta_ra = (*it)->ra_adjust;
	}
      }
    }
    
    *delta_dec = best_delta_dec;
    *delta_ra = best_delta_ra;
  }
  return closest_so_far;
}
  
//****************************************************************
//        print_help()
//****************************************************************
// Commands:
//    sync: the target star is exactly centered in the frame; add a
//        point to the sync_point file and go to the next star.
//    move xxN xxE: move the specified number of arcmin and fetch an
//        image
//    next: stop processing this star and goto the next star in the
//        list. This star will not be revisited.
//    expose: make an exposure
//    blob: use the prior exposure to auto-blob sync on the bright
//        star. 
void print_help(void) {
  fprintf(stdout, "sync\nmove xx.xNS xx.xEW\nnext\nexpose\nblob\n");
}

//****************************************************************
//        base_align_stars
//****************************************************************

// "Base" stars are not really stars. They are arbitrarily-selected
// spots in the sky for which there is a "catalog" file. The center of
// the field is given the name of a "fake star" (such as "align_C).

struct BaseAlignStar {
  const char *lookup_name;
  DEC_RA location;
  int    dec_band;
  bool   visible;
  int    on_west_side;
  ALT_AZ loc_alt_az;
};

struct Triplet {
  BaseAlignStar *stars[3]; // points to three triplet stars
  double score;
};

std::list<Triplet *> all_triplets;
std::list<BaseAlignStar *> all_base_stars;

struct BaseStarlist {
  const char *lookup_name;
  DEC_RA location;
  bool visible;
  bool imaged;
} base_catalog[] = {
  // +15-deg band
  { "align_A", DEC_RA((15.0/60.0)*M_PI/180.0, (2.0/12.0)*M_PI)},
  { "align_B", DEC_RA((15.0/60.0)*M_PI/180.0, (6.0/12.0)*M_PI)},
  { "align_C", DEC_RA((15.0/60.0)*M_PI/180.0, (10.0/12.0)*M_PI)},
  { "align_D", DEC_RA((15.0/60.0)*M_PI/180.0, (14.0/12.0)*M_PI)},
  { "align_E", DEC_RA((15.0/60.0)*M_PI/180.0, (18.0/12.0)*M_PI)},
  { "align_F", DEC_RA((15.0/60.0)*M_PI/180.0, (22.0/12.0)*M_PI)},
  { "align_G", DEC_RA((15.0/60.0)*M_PI/180.0, (4.0/12.0)*M_PI)},
  { "align_H", DEC_RA((15.0/60.0)*M_PI/180.0, (8.0/12.0)*M_PI)},
  { "align_I", DEC_RA((15.0/60.0)*M_PI/180.0, (12.0/12.0)*M_PI)},
  { "align_J", DEC_RA((15.0/60.0)*M_PI/180.0, (16.0/12.0)*M_PI)},
  { "align_K", DEC_RA((15.0/60.0)*M_PI/180.0, (20.0/12.0)*M_PI)},
  { "align_L", DEC_RA((15.0/60.0)*M_PI/180.0, (0.0/12.0)*M_PI)},
  // Equator
  { "align_0a", DEC_RA(0.0, (0.0/12.0)*M_PI)},
  { "align_0b", DEC_RA(0.0, (1.33/12.0)*M_PI)},
  { "align_0c", DEC_RA(0.0, (2.67/12.0)*M_PI)},
  { "align_0d", DEC_RA(0.0, (4.0/12.0)*M_PI)},
  { "align_0e", DEC_RA(0.0, (5.33/12.0)*M_PI)},
  { "align_0f", DEC_RA(0.0, (6.67/12.0)*M_PI)},
  { "align_0g", DEC_RA(0.0, (8.0/12.0)*M_PI)},
  { "align_0h", DEC_RA(0.0, (9.33/12.0)*M_PI)},
  { "align_0i", DEC_RA(0.0, (10.67/12.0)*M_PI)},
  { "align_0j", DEC_RA(0.0, (12.0/12.0)*M_PI)},
  { "align_0k", DEC_RA(0.0, (13.33/12.0)*M_PI)},
  { "align_0l", DEC_RA(0.0, (14.67/12.0)*M_PI)},
  { "align_0m", DEC_RA(0.0, (16.0/12.0)*M_PI)},
  { "align_0n", DEC_RA(0.0, (17.33/12.0)*M_PI)},
  { "align_0o", DEC_RA(0.0, (18.67/12.0)*M_PI)},
  { "align_0p", DEC_RA(0.0, (20.0/12.0)*M_PI)},
  { "align_0q", DEC_RA(0.0, (21.33/12.0)*M_PI)},
  { "align_0r", DEC_RA(0.0, (22.67/12.0)*M_PI)},
  // Dec -15 deg
  { "align-15a", DEC_RA((-15.0/60.0)*M_PI/180.0, (0.0/12.0)*M_PI)},
  { "align-15b", DEC_RA((-15.0/60.0)*M_PI/180.0, (4.0/12.0)*M_PI)},
  { "align-15c", DEC_RA((-15.0/60.0)*M_PI/180.0, (8.0/12.0)*M_PI)},
  { "align-15d", DEC_RA((-15.0/60.0)*M_PI/180.0, (12.0/12.0)*M_PI)},
  { "align-15e", DEC_RA((-15.0/60.0)*M_PI/180.0, (16.0/12.0)*M_PI)},
  { "align-15f", DEC_RA((-15.0/60.0)*M_PI/180.0, (20.0/12.0)*M_PI)},
  // Dec +30
  { "align+30a", DEC_RA((30.0/60.0)*M_PI/180.0, (0.0/12.0)*M_PI)},
  { "align+30b", DEC_RA((30.0/60.0)*M_PI/180.0, (1.6/12.0)*M_PI)},
  { "align+30c", DEC_RA((30.0/60.0)*M_PI/180.0, (3.2/12.0)*M_PI)},
  { "align+30d", DEC_RA((30.0/60.0)*M_PI/180.0, (4.8/12.0)*M_PI)},
  { "align+30e", DEC_RA((30.0/60.0)*M_PI/180.0, (6.4/12.0)*M_PI)},
  { "align+30f", DEC_RA((30.0/60.0)*M_PI/180.0, (8.0/12.0)*M_PI)},
  { "align+30g", DEC_RA((30.0/60.0)*M_PI/180.0, (9.6/12.0)*M_PI)},
  { "align+30h", DEC_RA((30.0/60.0)*M_PI/180.0, (11.2/12.0)*M_PI)},
  { "align+30i", DEC_RA((30.0/60.0)*M_PI/180.0, (12.8/12.0)*M_PI)},
  { "align+30j", DEC_RA((30.0/60.0)*M_PI/180.0, (14.4/12.0)*M_PI)},
  { "align+30k", DEC_RA((30.0/60.0)*M_PI/180.0, (16.0/12.0)*M_PI)},
  { "align+30l", DEC_RA((30.0/60.0)*M_PI/180.0, (17.6/12.0)*M_PI)},
  { "align+30m", DEC_RA((30.0/60.0)*M_PI/180.0, (19.2/12.0)*M_PI)},
  { "align+30n", DEC_RA((30.0/60.0)*M_PI/180.0, (20.8/12.0)*M_PI)},
  { "align+30o", DEC_RA((30.0/60.0)*M_PI/180.0, (22.4/12.0)*M_PI)},
  // Dec +70
  { "align_N1", DEC_RA(70*M_PI/180.0, (4.0/12.0)*M_PI)},
  { "align_N2", DEC_RA(70*M_PI/180.0, (12.0/12.0)*M_PI)},
  { "align_N3", DEC_RA(70*M_PI/180.0, (20.0/12.0)*M_PI)},
};
#define num_base_catalog (int)(sizeof(base_catalog)/sizeof(base_catalog[0]))

Triplet *PickBestTriplet(void) {
  JULIAN now(time(0));
  const double sidereal_time_radians = GetSiderealTime();
  
  for (int i=0; i<num_base_catalog; i++) {
    BaseStarlist *b = &base_catalog[i];
    BaseAlignStar *bas = new BaseAlignStar;
    bas->lookup_name = b->lookup_name;
    bas->location = b->location;
    bas->dec_band = (b->location.dec() < 0.0);
    bas->loc_alt_az = ALT_AZ(b->location, now);
    bas->visible = IsVisible(bas->loc_alt_az, now);

    double hour_angle = sidereal_time_radians - bas->location.ra_radians();
    if (hour_angle < -M_PI) hour_angle += (M_PI*2.0);
    bas->on_west_side = dec_axis_likely_flipped(hour_angle);

    all_base_stars.push_back(bas);
  }

  double best_score = -1.0;
  Triplet *best = 0;

  // Build candidate triplets
  std::list<BaseAlignStar *>::iterator i;
  std::list<BaseAlignStar *>::iterator ii;
  std::list<BaseAlignStar *>::iterator iii;
  for (i=all_base_stars.begin(); i!=all_base_stars.end(); i++) {
    if (!(*i)->visible) continue;
    ii = i;
    for (ii++; ii != all_base_stars.end(); ii++) {
      if (!(*ii)->visible) continue;
      iii = ii;
      for (iii++; iii != all_base_stars.end(); iii++) {
	if (!(*iii)->visible) continue;

	Triplet *t = new Triplet;
	t->stars[0] = *i;
	t->stars[1] = *ii;
	t->stars[2] = *iii;
	
	int dec_band_sum = (*i)->dec_band + (*ii)->dec_band + (*iii)->dec_band;
	if (dec_band_sum == 0 || dec_band_sum == 3) {
	  t->score = 0.0;
	} else if(dec_band_sum == 1) {
	  t->score = 1.0;
	} else {
	  t->score = 0.5;
	}

	// multiple by elevation diversity
	double el_max = std::max((*i)->loc_alt_az.altitude_of(),
				 std::max((*ii)->loc_alt_az.altitude_of(),
					  (*iii)->loc_alt_az.altitude_of()));
	double el_min = std::min((*i)->loc_alt_az.altitude_of(),
				 std::min((*ii)->loc_alt_az.altitude_of(),
					  (*iii)->loc_alt_az.altitude_of()));
	t->score *= (el_max - el_min)/M_PI;

	// multipy by meridian flip diversity
	int side_count = (*i)->on_west_side +
	  (*ii)->on_west_side + (*iii)->on_west_side;
	if (side_count == 3 || side_count == 0) {
	  t->score = 0.0; // 0.0 score if no flip involved
	}

	if (t->score > best_score) {
	  best_score = t->score;
	  best = t;
	}

	if (t->score > 0.0) {
	  all_triplets.push_back(t);
	}
      }
    }
  }

  return best;
}

//****************************************************************
//        do_exposure()
// Local invocation of expose_image()
//****************************************************************
void do_exposure(void) {
  exposure_flags flags;
  flags.SetFilter("Vc");
  flags.SetDoNotTrack();
  fprintf(stdout, "Starting exposure."); fflush(stdout);
  last_image_filename = expose_image(exposure_time, flags);
  fprintf(stdout, " (Done: %s.)\n", last_image_filename); fflush(stdout);
}

//****************************************************************
//        auto_blob()
//****************************************************************
// returns true if was able to handle okay

bool auto_blob(OneAlignmentStar *this_star) {
  const char *script_out_file = "/tmp/blob_out.txt";
  const char *answer_line = "/tmp/blob_out.summary";
  const char *command = "/home/mark/ASTRO/BIN/find_blob";
  DEC_RA loc(this_star->star->Location().dec(),
	     this_star->star->Location().ra_radians());
  char cmd_buffer[400];

  sprintf(cmd_buffer, "%s -i %s > %s 2>&1; fgrep RESULT %s > %s",
	  command, last_image_filename, script_out_file,
	  script_out_file, answer_line);
  if (system(cmd_buffer)) {
    fprintf(stderr, "find_blob returned with error code.\n");
  }

  FILE *fp_answer = fopen(answer_line, "r");
  char answer_1[80];
  char answer_2[80];
  char answer_3[80];
  if (!fgets(cmd_buffer, sizeof(cmd_buffer), fp_answer)) {
    fprintf(stderr, "Cannot fetch output from find_blob.\n");
  }
  int num_converted = sscanf(cmd_buffer, "%s %s %s",
			     answer_1, answer_2, answer_3);
  if (num_converted == 2) {
    if (strcmp(answer_1, "RESULT") == 0 &&
	strcmp(answer_2, "INVALID") == 0) {
      fprintf(stderr, "Cannot identify valid blob.\n");
      return false; // no valid blob found
    } else {
      fprintf(stderr, "Invalid answer from find_blob: %s\n", cmd_buffer);
      return false;
    }
  } else if (num_converted == 3) {
    if (strcmp(answer_1, "RESULT") == 0) {
      const bool flipped = dec_axis_is_flipped();
      const int flipper = (flipped ? +1 : -1);
      double x, y;
      sscanf(answer_2, "%lf", &x);
      sscanf(answer_3, "%lf", &y);
      const double radians_per_pixel = (1.52/3600.0)*(M_PI/180.0);
      // offset_x and offset_y are in arc-radians
      const double offset_x = flipper * (x-256.0)*radians_per_pixel; // in radians
      const double offset_y = flipper * (y-256.0)*radians_per_pixel; // in radians

      // true_center is J2000 epoch
      DEC_RA true_center(loc.dec()-offset_y,
			 loc.ra_radians()-(offset_x/cos(loc.dec())));

      fprintf(stderr, "Syncing to image center [%s, %s]\n",
	      true_center.string_dec_of(), true_center.string_ra_of());
      // either add this point to the SyncSession or send it straight
      // to the mount.
      if (build_syncfile) {
	add_session_point(true_center);
      } else {
	GM2000AddSyncPoint(true_center);
      }
      
      DEC_RA mount_belief = ScopePointsAt(); // J2000 epoch
      fprintf(stderr, "Mount believes it's at [%s, %s] J2000\n",
	      mount_belief.string_dec_of(), mount_belief.string_ra_of());
      this_star->dec_adjust = -(true_center.dec() - mount_belief.dec());
      this_star->ra_adjust = -(true_center.ra_radians() -
			       mount_belief.ra_radians());
      fprintf(stdout, "Session point added.\nDelta dec = %lf (arcmin), delta RA = %lf (min)\n",
		this_star->dec_adjust*60*180/M_PI,
		this_star->ra_adjust*60*180/M_PI);
      this_star->status = COMPLETED;
      return true;
    }
  }
  fprintf(stderr, "Invalid answer from find_blob: %s\n", cmd_buffer);
  return false;
}

double get_move(char *dir, char *string) {
  char last_letter;
  const int len = strlen(string);
  char *last_letter_ptr = string + (len-1);
  double value;
  last_letter = *last_letter_ptr;
  *last_letter_ptr = 0;

  if (sscanf(string, "%lf", &value) != 1) {
    fprintf(stderr, "Illegal move value: %s\n", string);
    value = 0.0;
  }
  *dir = last_letter;

  if (last_letter == 'N' || last_letter == 'n' ||
      last_letter == 'E' || last_letter == 'e') {
    return value;
  } else if (last_letter == 'S' || last_letter == 's' ||
	     last_letter == 'W' || last_letter == 'w') {
    return -value;
  } else {
    fprintf(stderr, "Invalid direction: %c\n", last_letter);
  }
  return 0.0;
}
  
//****************************************************************
//        handle_user_input()
//****************************************************************

// return true if done
bool handle_user_input(OneAlignmentStar *star) {
  ObsStatus ob_status = WORKING;
  star->status = WORKING;
  bool finished = false;
  bool user_exit = false;

  do {
    // prompt the user
    fprintf(stdout, "go: "); fflush(stdout);

    char buffer[132];
    char *ret_val = fgets(buffer, sizeof(buffer), stdin);
    if (ret_val == 0) {
      // end of user input. Need to return and perform exit()
      finished = true;
      break;
    }

    char *s = buffer;
    while(isspace(*s)) s++;

    char command[132];
    if(sscanf(s, "%s", command) == 0) {
      // blank line
      continue;
    } else {
      // what is the keyword?
      if (strcmp(command, "sync") == 0) {
	//****************
	//    SYNC
	//****************
	DEC_RA raw_loc = ScopePointsAt();
	star->dec_adjust = raw_loc.dec() - star->star->Location().dec();
	star->ra_adjust  = raw_loc.ra_radians() -
	  star->star->Location().ra_radians();

	if (build_syncfile) {
	  add_session_point(star->star->Location());
	} else {
	  GM2000AddSyncPoint(star->star->Location());
	}

	fprintf(stdout, "Session point added.\nDelta dec = %lf (arcmin), delta RA = %lf (min)\n",
		star->dec_adjust*60*180/M_PI,
		star->ra_adjust*60*180/M_PI);
	
	ob_status = COMPLETED;
	finished = true;
      } else if(strcmp(command, "move") == 0) {
	//****************
	//    MOVE
	//****************
	char m_part1[80], m_part2[80];
	double arcmin1, arcmin2;
	char   dir1, dir2;
	bool   do_expose = true;
	
	const int num_scan = sscanf(s+5, "%s %s", m_part1, m_part2);
	if (num_scan == 1) {
	  arcmin1 = get_move(&dir1, m_part1);
	  if (dir1 == 'N' || dir1 == 'n' || dir1 == 'S' || dir1 == 's') {
	    SmallMove(0.0, arcmin1);
	  } else if (dir1 == 'E' || dir1 == 'e' || dir1 == 'W' || dir1 == 'w') {
	    SmallMove(arcmin1/cos(star->star->Location().dec()), 0.0);
	  } else {
	    fprintf(stdout, "Invalid move direction: %c\n", dir1);
	    do_expose = false;
	  }
	} else if (num_scan == 2) {
	  arcmin1 = get_move(&dir1, m_part1);
	  arcmin2 = get_move(&dir2, m_part2);
	  double delta_dec;
	  double delta_ra;
	  if (dir1 == 'N' || dir1 == 'n' || dir1 == 'S' || dir1 == 's') {
	    delta_dec = arcmin1;
	    delta_ra = arcmin2;
	  } else {
	    delta_dec = arcmin2;
	    delta_ra = arcmin1;
	  }
	  SmallMove(delta_ra/cos(star->star->Location().dec()), delta_dec);
	} else {
	  fprintf(stdout, "Invalid move command.\n");
	  do_expose = false;
	}
	if (do_expose) do_exposure();
	
      } else if(strcmp(command, "blob") == 0) {
	//****************
	//    BLOB
	//****************
	if(auto_blob(star)) {
	  finished = true;
	  ob_status = COMPLETED;
	} else {
	  fprintf(stdout, "No blob found.\n");
	}
	
      } else if(strcmp(command, "next") == 0) {
	//****************
	//    NEXT
	//****************
	fprintf(stdout, "Skipping to next star...\n");
	
	ob_status = SKIP;
	finished = true;
      } else if(strcmp(command, "help") == 0 ||
		strcmp(command, "?") == 0) {
	//****************
	//    HELP
	//****************
	print_help();
      } else if (strcmp(command, "expose") == 0) {
	//****************
	//    EXPOSE
	//****************
	do_exposure();
      } else if (strcmp(command, "quit") == 0 ||
		 strcmp(command, "exit") == 0) {
	//****************
	//    QUIT/EXIT
	//****************
	user_exit = true;
	finished = true;
      } else {
	fprintf(stdout, "Unrecognized command: %s\n",
		command);
	print_help();
      }
    }
  } while(!finished);
  star->status = ob_status;
  return user_exit;
}

//****************************************************************
//        PerformBaseAlignment()
//****************************************************************
void PerformBaseAlignment(void) {
  ClearMountModel();

  Triplet *triplet = PickBestTriplet();
  fprintf(stderr, "Using alignment points: %s, %s, and %s.\n",
	  triplet->stars[0]->lookup_name,
	  triplet->stars[1]->lookup_name,
	  triplet->stars[2]->lookup_name);

  exposure_flags flags;
  flags.SetFilter(Filter("Vc"));
  flags.SetShutterShut();
  fprintf(stderr, "Making dark exposure (60 sec).\n");
  char *dark = strdup(expose_image(60.0, flags));

  for (int i=0; i< 3; i++) {
    fprintf(stderr, "Slewing to Base star # %d\n", i+1);
    MoveTo(&triplet->stars[i]->location);
    WaitForGoToDone();
    sleep(5);

    flags.SetShutterOpen();
    fprintf(stderr, "Getting field exposure (60 sec).\n");
    char *good = expose_image(60.0, flags);

    {
      char command_buffer[256];
      sprintf(command_buffer, COMMAND_DIR "/find_stars -d %s -i %s > /tmp/find.txt 2>&1",
	      dark, good);
      fprintf(stderr, "executing %s\n", command_buffer);
      if(system(command_buffer) == -1) {
	perror("Unable to execute find_stars command");
      } else {
	sprintf(command_buffer,
		COMMAND_DIR "/star_match -e -f -d %s -n %s -i %s -b > /tmp/match.txt 2>&1",
		dark, triplet->stars[i]->lookup_name,
		good);
      fprintf(stderr, "executing %s\n", command_buffer);
	if(system(command_buffer) == -1) {
	  perror("Unable to execute star_match command");
	}
      }
    }

    Image *finder = new Image(good); // pick up new starlist
    int status = 0;
    DEC_RA current_center = finder->ImageCenter(status);
    if(status == STATUS_OK) {
      // We add this location as the "true" spot that the scope is
      // pointing at into the mount error file being maintained for
      // this session.
      fprintf(stderr, "Finder match successful.");
      GM2000AddSyncPoint(current_center);
      delete finder;		// delete the image used for the finder
    } else {

      // didn't work. Any stars seen?
      IStarList *list = finder->GetIStarList();
      if(list->NumStars == 0) {
	fprintf(stderr, "No stars found in image.\n");
	exit(-2);
      } else if(list->NumStars <= 2) {
	// too few stars
	fprintf(stderr, "Finder for %s: only %d stars seen.",
		triplet->stars[i]->lookup_name, list->NumStars);
	exit(-2);
      } else {
	// otherwise, it just couldn't find a match. Tough.
	fprintf(stderr, "Finder for %s: couldn't match.",
		triplet->stars[i]->lookup_name);
	exit(-2);
      }
    }
  } // end loop over all three points
  fprintf(stderr, "Base alignment complete.\n");
}
    


//****************************************************************
//        main()
//****************************************************************

// Options:
//    -r        [refine only] - Skip the "base alignment" step, which
//              is normally used for the first three stars using
//              star_match to match the field.
//
//    -n        Inhibit clearing any existing syncfile. Use this if
//              the previous syncfile session was aborted for some
//              reason and you want to add more blobs.
//    
//    -s        Build a syncfile. Clear any existing points in the
//              syncfile.  When
//              you're done, you have a full syncfile, but it
//              won't have been loaded into the mount.
//
//    -l        Load the syncfile into the mount.
//
// [For more info, see REMOTE_LIB/mount_model_gm2000]

void usage(void) {
  fprintf(stderr, "usage: auto_sync [-r] [-n] [-s] [-l] [-c number_refinement_stars]\n");
  exit(-2);
}

int main(int argc, char **argv) {
  int option_char;
  bool enable_blobs = true;
  bool refine_only = false;
  int sync_count = 80; // default count
  
  while((option_char = getopt(argc, argv, "slnrc:")) > 0) {
    switch (option_char) {
    case 's':
      build_syncfile = true;
      break;

    case 'l':
      load_syncfile = true;
      break;
      
    case 'n':
      erase_syncfile = false;
      break;

    case 'r':
      refine_only = true;
      break;
      
    case 'c':
      sync_count = atoi(optarg);
      break;

    case '?':
    default:
      usage();
    }
  }
  
  connect_to_scope();
  // Loading a syncfile doesn't require the use of a camera
  if (!load_syncfile) {
    connect_to_camera();
  }

  // Check for illegal combinations . . .
  if (build_syncfile && load_syncfile) {
    fprintf(stderr, "Error: -s and -l are mutually exclusive.\n");
    usage();
  }

  if (build_syncfile && erase_syncfile) {
    start_new_session(0);
  }

  // perform initial three alignment stars
  if ((!build_syncfile) && (!refine_only) && (!load_syncfile)) {
    PerformBaseAlignment();
  }

  if (!load_syncfile) {
  JULIAN now(time(0));
  MakeStarList(sync_count);

  OneAlignmentStar *this_star = pick_first_star();
  double delta_dec = 0.0;
  double delta_ra = 0.0;

  fprintf(stderr, "calculated Sidereal Time = %lf\n",
	  SiderealTime(now));

  do {
    DEC_RA star_location = this_star->star->Location();
    
    AlignmentStats stats;

    GetStats(&stats);
    fprintf(stdout, "[%d sync points, %d skipped, %d total.]\n",
	    stats.num_align_points, stats.num_skipped, stats.num_stars);
  
    fprintf(stdout, "Starting slew to mag %.1lf star.\n",
	    this_star->star->Magnitude());
    fflush(stdout);

    MoveTo(&star_location);
    WaitForGoToDone();
    const bool flipped = dec_axis_is_flipped();
    fprintf(stdout, "Mount is%s flipped.\n",
	    (flipped ? "" : " not"));

    do_exposure();

    if ((!enable_blobs) || !auto_blob(this_star)) {
      if(handle_user_input(this_star)) break;
    }

    this_star = pick_next_star(&delta_dec, &delta_ra);
  } while (this_star);
  }
  if (load_syncfile) {
    recalculate_model(0);
  }
}
