// This may look like C code, but it is really -*- C++ -*-
/*  mount_model_int.cc -- Implements mount pointing model
 *
 *  Copyright (C) 2015 Mark J. Munkacsy

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

#include "dec_ra.h"
#include "julian.h"
#include <list>
#include <stdio.h>
#include "sync_session.h"

//
// Above all else, remember the following convention:
// {residual error} = {obs position} - ({catalog pos} + {model adj})
//
// A negative value of Mel means the mount's axis points at a spot in
// the sky *above* the pole.
// A positive value of Maz means the mount's axis points at a spot to
// the east of the pole.

// Definitions:
//    - The mount knows only the coordinate system called {scope}
//    - True (catalog) positions are called {catalog}
//    - The difference between {scope} and {catalog} are called {delta}
//    - Imperfections in the mount model show up as {error}, which can
//    be measured in either the {scope} or {catalog} systems.
//    - The {mount parameters} describe how to calculate {delta}.
//    - Sync points capture pairs of {scope},{catalog} and are used to
//    calculate the {mount parameters}.

//****************************************************************
//        MountModel
//****************************************************************
class MountModel {
public:
  // make available to everyone
  void Publish(void); // *always* publishes to the same place
  void Refresh(void); // pick up anything published by anyone else

  MountModel(void); // creates a zero'd mount model
  MountModel(const char *filename);
  ~MountModel(void) {;} // nothing to do
  
  // a chance to save things without publishing them
  void Write(const char *filename);

  void Build(SyncSession *s); // build a model from sync points

  DEC_RA RawToTrue(DEC_RA raw_location);
  DEC_RA TrueToRaw(DEC_RA desired_true_location);

  // delta_D0 and delta_H0 will be added to D0, H0
  // Both delta_D0 and delta_H0 are in radians
  void AdjustD0H0(double delta_D0, double delta_H0);

  bool MountModelInUse(void); // is model to be used?
  void UseMountModel(bool use_mount_model); // sticky (preserved
					    // across sessions)

  const double *GetParams(void) const { return params; }

  void Print(FILE *fp); // print a human-readable summary
  void Zero(void); // zero the model's parameters

  bool UsingEpochJ2000(void) { return use_epoch_J2000; }
  void UseEpochJ2000(bool use_J2000) { use_epoch_J2000 = use_J2000; }
  bool ModelFrozen(void) { return model_is_frozen; }
  void FreezeModel(bool freeze_model) { model_is_frozen = freeze_model; }
  
  void calculateDelta(double declination, double hour_angle,
		      double *delta_dec, double *delta_ha,
		      bool flipped);
public:
  static const int PARAM_D0     = 0;		// 
  static const int PARAM_H0     = 1;		// 
  static const int PARAM_Mel    = 2;
  static const int PARAM_Maz    = 3;
  static const int PARAM_ch     = 4; // collimation error
  static const int PARAM_np     = 5; // non-perpendicularity
  static const int PARAM_flex   = 6; // dec axis flexure
  static const int PARAM_Hslap  = 5; // HA axis slop around meridian

  static const int NUM_PARAM = 6; //(leave out flex for now)

private:
  double params[NUM_PARAM];

  bool use_epoch_J2000; // if false, use today's epoch
  bool model_is_frozen;
  bool model_is_enabled;

  time_t last_refresh_time;

};
  
  
