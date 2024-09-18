// This may look like C code, but it is really -*- C++ -*-
/*  mount_model.h -- Implements mount pointing model (obsolete)
 *
 *  Copyright (C) 2007 Mark J. Munkacsy

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

#ifndef _MOUNT_MODEL_H
#define _MOUNT_MODEL_H

#include <stdio.h>
#include "dec_ra.h"
#include "julian.h"
#include "gendefs.h"

// Convert back and forth between raw scope position and corrected
// (modeled) position. mount_coords() returns the "raw" scope position
// that corresponds to a desired true location. true_coords() takes a
// "raw" scope position and returns the actual sky location when the
// scope claims to be at that "raw" position.
//
// These come in two variants. The first pair assume a "likely"
// meridian flip if necessary. The second pair accept a boolean to
// indicate whether a flip has *actually* occurred.
#ifdef INTERNAL_MOUNT_MODEL
DEC_RA mount_coords(DEC_RA catalog_position, JULIAN when);
DEC_RA true_coords(DEC_RA scope_position, JULIAN when);
DEC_RA mount_coords(DEC_RA catalog_position, JULIAN when, bool flipped);
DEC_RA true_coords(DEC_RA scope_position, JULIAN when, bool flipped);
#endif

// returns 0 on success, errno otherwise
int start_new_session(const char *session_filename);

// implicit "now" and "current scope position". 
void add_session_point(DEC_RA catalog_position);

void zero_mount_model(void);

// just adjust H0 and D0; leave all else the same
void quick_sync_model(DEC_RA catalog_position);

// Use current set of sync points to calculate new model from a set of
// session points
int recalculate_model(const char *session_file_name);
		      
// Print the current model parameters onto the specified file
void print_mount_model(FILE *fp);

#ifndef GM2000
void get_mount_model_control(bool &enable_mount_modeling,
			     bool &enable_epoch_adjust);

void control_mount_model(bool enable_mount_modeling, bool enable_epoch_adjust);
#endif
#endif
