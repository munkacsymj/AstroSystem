/* This may look like C code, but it is really -*-c++-*- */
/*  astro_db.h -- manages the JSON file holding session info
 *
 *  Copyright (C) 2022 Mark J. Munkacsy

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
#ifndef _ASTRO_DB_H
#define _ASTRO_DB_H

#include <list>
#include <string>

#include "json.h"
#include "julian.h"
#include <stdio.h>

//****************************************************************
//        Helper class: JUID (JASON Unique ID)
//****************************************************************

typedef long juid_t;

enum DB_Entry_t {
		 DB_SESSION,
		 DB_IMAGE,
		 DB_SET,
		 DB_ANALYSIS,
		 DB_INST_MAGS,
		 DB_DIRECTIVE,
		 DB_SUBMISSION,
		 DB_STACKS,
		 // special, final entry
		 DB_NUM_JUID_TYPES,
};

DB_Entry_t GetJUIDType(juid_t juid);

class JUID {
public:
  JUID(void) {;}
  ~JUID(void) {;}

  void Initialize(JSON_Expression &exp);
  long GetNextJUID(DB_Entry_t which_JUID_type);

private:
  juid_t next_juid[DB_NUM_JUID_TYPES];
};

class AstroDB;
class DB_Measurement {
public:
  DB_Measurement(AstroDB &astro_db,
		 const char *target); // must match strategy name
  ~DB_Measurement(void) {;}
  void AddExposure(const char *fits_filename,
		   const char *filter,
		   JULIAN midpoint,
		   double exposure_time,
		   double airmass,
		   const char *chartname,
		   bool needs_dark = true,
		   bool needs_flat = true);
  juid_t Close(bool include_stack);
private:
  const char *target_name;
  AstroDB &host_db;
  juid_t this_directive;
  std::list<std::pair<const char *,juid_t>> exposure_list;
};

class AstroDB {
 public:
  struct InstMagMeasurement {
    const char *star_id;
    double inst_mag;
    double uncertainty;
    double airmass;
  };

  struct DiffMagProfile {
    const char *profile_name;
    int profile_source_tag; // used w/profile_name for uniqueness
    double julian;
    double exposure_time;
    double airmass;
    const char *target;
    const char *filter;
    const char *technique;
    double crefmag;
    std::list<std::string> *comp_star_names;
    std::list<std::string> *check_star_names;
    double check_rms;
    const char *chartID;
  };

  class ADirective {
  public:
    ADirective(AstroDB &host, JSON_Expression *root_exp=nullptr);
    ~ADirective(void);
    bool ImageExcludedFromStack(juid_t image_juid);
    bool ImageExcludedFromAnaly(juid_t image_juid);
    bool StarExcludedFromEnsemble(const char *name, const char *filter=nullptr);
    bool StarExcludedFromChecks(const char *name, const char *filter=nullptr);
    bool UseEnsembles(void) const { return use_ensembles; }
    bool ZeroPointTransforms(void) const { return zero_point_transforms; }
    bool DoTransforms(void) const { return do_transforms; }
  private:
    std::list<juid_t> stack_exclusions;
    std::list<juid_t> analy_exclusions;
    std::list<std::pair<const char *,const char *>> ensemble_exclusions;
    std::list<std::pair<const char *,const char *>> check_exclusions;
    bool do_transforms;
    bool zero_point_transforms;
    bool use_ensembles;
    AstroDB &&parent;
  };

  struct DiffMagMeasurement {
    const char *star_id;
    double diff_mag;
    double uncertainty;
    double uncty_snr;
    bool from_stacked_image; // else averaged from images
    long num_vals;
    DiffMagProfile *profile;
  };
  
  // "date" can either be a date in the form of "6-12-2020" or can be
  // a pathname in the form of "/home/IMAGES/6-12-2020".
  AstroDB(int mode = JSON_READONLY,
	  const char *date = nullptr); // default is today
  ~AstroDB(void) {;}

  // This returns the new integer sequence number for this session
  int NewSession(const char *type);
  const char *SessionLogfile(void);
  const char *SessionShellfile(void);
  void SetShellFile(void);

  void SyncAndRelease(void);
  void Reactivate(bool *anything_changed=nullptr);

  std::list<JSON_Expression *> &FetchAllOfType(DB_Entry_t which_type);

  // If the specified image has instrumental mags, the juid of the
  // instrumental mags will be returned, otherwise, 0 is returned.
  juid_t InstMagsForJUID(juid_t image_juid);
  juid_t DiffPhotForJUID(juid_t instmags_juid);
  void DeleteEntryForJUID(juid_t item_to_delete);

  juid_t LookupExposure(const char *filename, const char *section=nullptr);

  juid_t CreateEmptyDirective(juid_t new_juid=-1);

  juid_t AddExposure(const char *fits_filename,
		     const char *target,
		     const char *filter,
		     juid_t directive,
		     JULIAN midpoint,
		     double exposure_time,
		     double airmass,
		     const char *chartname,
		     bool needs_dark = true,
		     bool needs_flat = true);

  juid_t AddSubexpSet(const char *filter,
		      juid_t directive,
		      std::list<juid_t> &input_exposures);

  juid_t AddMergeSet(juid_t input_stack,
		     juid_t directive,
		     juid_t input_subexp);

  juid_t AddRefreshStack(const char *filter,
			 juid_t directive,
			 const char *target_object,
			 const char *stack_filename,
			 std::list<const char *> &constituent_filenames,
			 bool filenames_are_actual);
  juid_t AddRefreshStack(const char *filter,
			 juid_t directive,
			 const char *target_object,
			 const char *stack_filename,
			 std::list<juid_t> &constituent_juids,
			 bool filenames_are_actual);

  juid_t AddBVRISet(std::list<juid_t> &input, juid_t directive);

  juid_t AddDiffMags(juid_t source_set,
		     juid_t directive,
		     std::list<DiffMagMeasurement> &mags);

  juid_t AddInstMags(juid_t source_exposure,
		     const char *filter,
		     juid_t directive,
		     const char *method,	   // "aperture"
		     const char *uncty_technique,  // "snr"
		     std::list<InstMagMeasurement> &mags);
  void AddPSF(juid_t inst_mags_juid,
	      double par1,
	      double par2);

  juid_t CreateNewTarget(const char *target_name);
  void AddJUIDToTarget(juid_t target_set, juid_t new_member);

  void Print(void) { al_exp.Print(stderr); }

  JSON_Expression *FindByJUID(juid_t juid);
  char *BaseDirectory(void);
  const char *AstroDBPathname(void) const { return sync_filename; }

  // Locking
  int BeginLockRegion(void);
  void EndLockRegion(int id);
  int BeginReleaseRegion(void);
  void EndReleaseRegion(int id);

 private:
  JSON_Expression al_exp;
  const char *sync_filename {nullptr};
  const char *working_date {nullptr};
  int file_mode;

  JUID juid;

  enum LockState {DB_LOCK, DB_RELEASE};
  LockState current_state { DB_LOCK };
  std::list<std::pair<int,LockState>> lock_stack;

  JSON_Expression *FindMaybeDeleteByJUID(juid_t juid, bool do_delete);
};

// ... an interesting helper function. This is normally invoked by
// passing in the filename of an image file. It will return a string
// that can be passed as an argument to the AstroDB constructor if an
// AstroDB file is in the same directory as the image file. Otherwise
// it will return nullptr.
const char *HasAstroDBInDirectory(const char *image_filename);


#endif
