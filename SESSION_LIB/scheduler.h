// This may look like C code, but it is really -*- C++ -*-
/*  scheduler.h -- implements genetic optimization algorithm to find a
 *  really good schedule
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
#ifndef _SIM_H
#define _SIM_H

#include "strategy.h"
#include "schedule.h"
#include "julian.h"
#include <stdio.h>

extern int SIZEOFCHROMOSOME;
extern std::vector<Schedule::strategy_time_pair *> stp_xref; // the input STP's
extern Session *RequestingSession;
typedef std::list<Schedule::strategy_time_pair *>::iterator stp_it;

static const int RES_OK = 0;
static const int RES_NOT_UP = 1;
static const int RES_TOO_LATE = 2;
static const int RES_USELESS = 3;

static const int TIME_INDEX_ENTRIES = 16;

class OBS_ELEMENT {
public:
  int    result;		// Observation Result Code
  double score;			// score for this observation
  Schedule::strategy_time_pair *SourceSTP(void) { return stp_xref[star_id_no]; }
  Strategy *star(void) { return SourceSTP()->strategy; }
  
  int    star_id_no;		// a small integer that uniquely
				// identifies the star
  int    time_index_no;		// a small integer that selects a
				// "time to wait" after the end of the
				// previous observation
  JULIAN when;
  OBS_ELEMENT(int star_id_number); // normal constructor
  OBS_ELEMENT(OBS_ELEMENT &oe);	// copy constructor
  OBS_ELEMENT(void) { star_id_no = 0; }
  void print_one_liner(FILE *f);
};


class TRIAL {
public:

  struct QuickPoolItem {
  public:
    Schedule::strategy_time_pair *stp; // input STP
    JULIAN last_scheduled;
  };

  void Reset(void);

  // Both of these take "input" stp's as an argument and return
  // pointers to "output" stp's.
  Schedule::strategy_time_pair *InsertInFirstGap(Schedule::strategy_time_pair *stp,
						 Schedule::strategy_time_pair *precedent = nullptr,
						 double padding_in_seconds = 0.0,
						 JULIAN insert_after_time=0.0);
  Schedule::strategy_time_pair *InsertFixedTime(Schedule::strategy_time_pair *stp);
  JULIAN TimeOfFirstGap(void);
  
  const std::list<Schedule::strategy_time_pair *> & GetTrial(void) { return trial; }
  std::list<QuickPoolItem *> quick_pool; // uses "input" stp's

private:
  std::list<Schedule::strategy_time_pair *> trial; // uses "output" stp's

  // The bool is true on success, with the stp_it pointing at the
  // entry after the gap.
  std::pair<stp_it, bool> FindFirstGap(double length_seconds);
  std::pair<stp_it, bool> FindTime(JULIAN when);
};

////////////////////////////////////////////////////////////////////
//        INDIVIDUAL
//    An individual includes an array of pointers to OBS_ELEMENTs and
//    a score.  Once an individual is created, it is never deleted
//    because it might be referenced in the hash table.
////////////////////////////////////////////////////////////////////
class INDIVIDUAL {
public:
  static JULIAN t_start, t_quit;
  
  OBS_ELEMENT *chromosome;	// points to array of OBS_ELEMENTs
  double score;			// the score for this individual
  int referenced_in_hashtable;	// set to 1 if this individual has a
				// hash table entry pointing to it.

  TRIAL trial;			// schedule that corresponds to the
				// set of chromosomes.
  int useful_length;		// number of meaningful chromosomes.

  INDIVIDUAL(void) {
    chromosome = new OBS_ELEMENT[SIZEOFCHROMOSOME];
    referenced_in_hashtable = 0;
  }
  void print_sequence(FILE *f);
};

// Here are the two random number generators that are used everywhere
int int_random(int low_limit, int high_limit);
double float_random(double low_limit, double high_limit);

// This function is called to set up the star_id array and to set the
// value of SIZEOFCHROMOSOME.
void setup_stars(Schedule *schedule);

void build_initial_population(void);
void main_loop(const char *output_filename);
  
#endif
