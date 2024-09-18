/*  scoring.cc -- calculates the "value function" for a candidate schedule
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
#include "session.h"
#include "observing_action.h"
#include "scoring.h"

double calculate_score(INDIVIDUAL *indiv);

static const int minutes = 60;

double time_delay_table[TIME_INDEX_ENTRIES] = {
  0.0,
  0.0,
  0.0,
  0.0,
  0.0,
  0.0,
  0.0,
  0.0,
  1.0 * minutes,
  2.0 * minutes,
  5.0 * minutes,
  10.0 * minutes,
  15.0 * minutes,
  30.0 * minutes,
  60.0 * minutes,
  120.0 * minutes,
};

// Scoring results are saved in "hash nodes" that make it easy for us
// to find an individual that we've previously scored.
struct HASH_NODE {
  struct INDIVIDUAL *individual; // points to the individual being
				 // scored
  struct HASH_NODE *next_hash;	 // points to the next individual with
				 // the same hash function
};

static int hash_tries = 0;
static int hash_hits  = 0;
static int hash_size  = 0;

void get_hash_statistics(int *tries, int *hits, int *total_size) {
  *tries = hash_tries;
  *hits  = hash_hits;
  *total_size = hash_size;
}

////////////////////////////////////////////////////////////////////
//     int hash_of(INDIVIDUAL *w)
//  This function provides the hashing function that identifies
//  individuals so that they can be found in the hash tables.
////////////////////////////////////////////////////////////////////
#define HASHSIZE 205

int hash_of(INDIVIDUAL *w) {
  int hash = 0;
  for(int i = 0; i<SIZEOFCHROMOSOME; i++) {
    hash = (hash * 203 + w->chromosome[i].star_id_no) % HASHSIZE;
  }
  return hash;
}

// Here are the heads of all the linked hash chains. There is one head
// for each possible hash value.
HASH_NODE *hash_heads[HASHSIZE];

////////////////////////////////////////////////////////////////////
//        void assign_score(INDIVIDUAL *x)
//   This function checks the hash table to see if we have previously
//   scored an identical individual.  If so, the score from that
//   previously-evaluated individual will be copied into this new
//   individual.  If not, the new one will be evaluated and will be
//   put into the hash table.  The function calculate_score() is used
//   to actually compute the score.
////////////////////////////////////////////////////////////////////
#ifdef NO_HASH
void assign_score(INDIVIDUAL *x) {
  x->score = calculate_score(x);
}
#else
void assign_score(INDIVIDUAL *x) {
  // calculate the hash value for this new individual
  const int hash = hash_of(x);
  // now walk down the hash chain for this hash value, checking to see
  // if any individuals have an identical set of chromosomes.
  hash_tries++;
  HASH_NODE *h = hash_heads[hash];
  if(h) {
    do { // start the loop over all individuals with this hash
      OBS_ELEMENT *j = x->chromosome;
      OBS_ELEMENT *k = h->individual->chromosome;

      // now see if "j" and "k" match (do an element-by-element check)
      for(int c = 0; c<SIZEOFCHROMOSOME; c++) {
	if(j[c].star_id_no != k[c].star_id_no ||
	   j[c].time_index_no != k[c].time_index_no) goto next_hash_jmp;
      }
      // looped through all elements without finding a mismatch
      x->score = h->individual->score; // copy the score
      for(int c = 0; c<SIZEOFCHROMOSOME; c++) {
	x->chromosome[c].result = h->individual->chromosome[c].result;
	x->chromosome[c].score  = h->individual->chromosome[c].score;
	x->chromosome[c].when   = h->individual->chromosome[c].when;
      }
      hash_hits++;
      return;

    next_hash_jmp:
      ; // null, just lets us place the label
    } while((h=h->next_hash));
  }

  // no match was found; we will create a new hash node
  hash_size++;
  h = new HASH_NODE;
				// put this element in at the head of
				// the hash chain for this hash value
  h->next_hash = hash_heads[hash];
  hash_heads[hash] = h;
  h->individual = x;
				// then calculate the actual score
  x->score = calculate_score(x);
  x->referenced_in_hashtable = 1;
}
#endif

double
calculate_score(INDIVIDUAL *indiv) {
  double cum_score = 0.0;

  indiv->trial.Reset();

  // Build the quick pool and schedule the time sequences
  for(int element_index=0; element_index < SIZEOFCHROMOSOME; element_index++) {
    OBS_ELEMENT &e = indiv->chromosome[element_index];
    Schedule::strategy_time_pair *e_info = e.SourceSTP(); // an input STP
    ObservingAction &oa = *(e_info->oa);

    if (oa.TypeOf() == AT_Time_Seq && element_index < SIZEOFCHROMOSOME/2) {
      indiv->trial.InsertFixedTime(e_info);
    }
    else if (oa.TypeOf() == AT_Quick && element_index < SIZEOFCHROMOSOME/2) {
      indiv->trial.quick_pool.push_back(new TRIAL::QuickPoolItem{e_info,0.0});
    }
  }

  // Now add all the other ObservingActions
  JULIAN scheduling_time = indiv->t_start;
  Schedule::strategy_time_pair *prior_entry = nullptr;
  int last_useful_element = -1;
  
  for(int element_index=0; element_index < SIZEOFCHROMOSOME; element_index++) {
    OBS_ELEMENT &e = indiv->chromosome[element_index];
    Schedule::strategy_time_pair *e_info = e.SourceSTP();
    ObservingAction &oa = *(e_info->oa);

    if (oa.TypeOf() == AT_Time_Seq || oa.TypeOf() == AT_Quick) continue;

    // Check the quick pool and put anything in that makes sense
    for (auto qpi : indiv->trial.quick_pool) {
      if (scheduling_time - qpi->last_scheduled >= qpi->stp->oa->CadenceDays() and
	  qpi->stp->strategy->IsVisible(scheduling_time)) {
	auto x = indiv->trial.InsertInFirstGap(qpi->stp,
					       nullptr,
					       0.0,
					       qpi->last_scheduled+qpi->stp->oa->CadenceDays());
	if (x != nullptr) {
	  x->prior_observation = qpi->last_scheduled;
	  qpi->stp->oa->SetInterval(ObsInterval{x->scheduled_time.day(),
					       x->scheduled_end_time.day(),
					       1.0});
	  qpi->last_scheduled = x->scheduled_time;
	  scheduling_time = x->scheduled_end_time;
	  last_useful_element = element_index;
	}
      }
    }

    // Insert this observation
    if (oa.TypeOf() == AT_Script) {
      prior_entry = indiv->trial.InsertInFirstGap(e_info,
						  prior_entry,
						  time_delay_table[e.time_index_no]);
      if (prior_entry != nullptr) {
	scheduling_time = prior_entry->scheduled_end_time;
	oa.SetInterval(ObsInterval{prior_entry->scheduled_time.day(),
				   prior_entry->scheduled_end_time.day(),
				   1.0});
	last_useful_element = element_index;
      }
    } else {
      // Dark and Flat
      auto x = indiv->trial.InsertInFirstGap(e_info);
      if (x != nullptr) {
	scheduling_time = x->scheduled_end_time;
	last_useful_element = element_index;
      }
    }
  }
  indiv->useful_length = last_useful_element;

  // Now perform the score calculation
  int trial_index = 0;

  for (auto t : indiv->trial.GetTrial()) { // "t" is an <output> STP
    double this_score = 0.0;
    Strategy *strategy = t->strategy;
    ObservingAction *oa __attribute__((unused)) = t->oa;
    if(t->needs_execution == 0) {
      // this thing makes no contribution
      t->result = RES_USELESS;
      // check to see if object is visible
    } else if (oa->TypeOf() == AT_Dark ||
	       oa->TypeOf() == AT_Flat) {
      this_score = oa->GetPriority();
      t->result = RES_OK;
    } else if(!strategy->IsVisible(t->scheduled_time)) {
      t->result = RES_NOT_UP;
      // check to see if observation goes beyond shutdown time
    } else {
      if(RequestingSession->SchedulingEndTime() < t->scheduled_end_time) {
	t->result = RES_TOO_LATE;
      } else {
	t->result = RES_OK;
	if (oa->TypeOf() == AT_Quick) {
	  JULIAN last_obs_time = strategy->GetLastObservationTime();
	  if (t->prior_observation > last_obs_time) last_obs_time = t->prior_observation;
	  this_score = t->score = oa->score(last_obs_time,
					    t->scheduled_time,
					    t->scheduled_end_time);
	  //fprintf(stderr, "quick score = %.3lf\n", this_score);
	} else {
	  this_score = t->score = oa->score(strategy->GetLastObservationTime(),
					    t->scheduled_time,
					    t->scheduled_end_time);
	}

	if(this_score == 0.0) t->result = RES_USELESS;
      }
    }
    t->score = this_score;
    cum_score += this_score;
    trial_index++;
  }
  //if(cum_score > 100.0) {
  //fprintf(stderr, "scoring: too high: %f\n", cum_score);
  //  }

  indiv->score = cum_score;
  return cum_score;
}

void print_trial(std::list<Schedule::strategy_time_pair *> trial) {
  for (auto x : trial) {
    fprintf(stdout, "%s %s %lf - %lf\n",
	    x->oa->strategy()->object(),
	    x->oa->TypeString().c_str(),
	    x->scheduled_time.day(),
	    x->scheduled_end_time.day());
  }
}
