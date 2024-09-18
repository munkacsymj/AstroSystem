/*  scheduler.cc -- implements genetic optimization algorithm to find a
 *  really good schedule
 *
 *  Copyright (C) 2007, 2017 Mark J. Munkacsy

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
#include <stdlib.h>		// pick up random()
#include <string.h>		// pick up strcpy()
#include <unistd.h>		// unlink()
#include <algorithm>		// std::max()
#include "observing_action.h"
#include "session.h"
#include "scheduler.h"
#include "scoring.h"

static const char *snap_file_name = "snapshot";

JULIAN INDIVIDUAL::t_start(0.0);
JULIAN INDIVIDUAL::t_quit(0.0);
static JULIAN t_start, t_quit;
Session *RequestingSession;
int SIZEOFCHROMOSOME;
std::vector<Schedule::strategy_time_pair *> stp_xref; // the input STP's


// local forward declarations
static void sort_population(void);
void summarize_generation(int generation_number);
void specific_neighbor_rotate(OBS_ELEMENT *src,
			      OBS_ELEMENT *tgt,
			      int chosen_element_to_move);
void write_snapshots(void);
void print_top_three(INDIVIDUAL *i1, INDIVIDUAL *i2, INDIVIDUAL *i3,
		     FILE *f);

// This is the size of the population, the total number of individuals
// that exist during a cycle
static const int POPULATION_SIZE = 70;
// This is the number of individuals in the population that are
// retained (intact) from one generation to the next.  Only the top
// scorers are kept intact.
static const int N_RETAIN = 40;
// Every individual contains every star that is known. Some may be
// scheduled after the termination time, some may be scheduled after
// sunrise. Hence, it makes no sense to try and create a "new" star as
// part of our mutation process (since everything is already there,
// the concept of something new makes no sense).  That leaves just a
// few possible things that can be done during mutations:
//    1) pick a substring of the observation list and rotate it to the
//        right some random number of chromosomes
//    2) swap two adjacent observations (kind of a special case of 1)
//    3) slice two different good elements at some random slice point
//    4) randomly change the time-delay between observations
static const double F_RANDOM_SWAP = 0.15;
static const double F_ROTATE = 0.25;
static const double F_PAIR_SWAP = 0.20;
static const double F_TIME_DELAY = 0.15;
static const int GENERATION_LIMIT = 1000;

int write_log = 40;
int write_snapshot = 100;		// used to be 100

static INDIVIDUAL *population[POPULATION_SIZE];

#ifdef DEBUG_POPULATION
void check_population(void) {
  for(int i=0; i<POPULATION_SIZE; i++) {
    for (int j=0; j<SIZEOFCHROMOSOME; j++) {
      if(population[i]->chromosome[j].star_id_no < 0) {
	fprintf(stderr, "negative star_id\n");
      }
    }
  }
}
#endif

void
TRIAL::Reset(void) {
  for (auto x:trial) {
    delete x;
  }
  trial.clear();
  for (auto z:quick_pool) {
    delete z;
  }
  quick_pool.clear();
}

////////////////////////////////////////////////////////////////////
//        cleanout_duplicates(OBS_ELEMENT *e)
////////////////////////////////////////////////////////////////////
// Duplicates: Each chromosome should contain each star_id exactly
// once. However, some operations are not guaranteed to preserve this
// "exactly once" condition. (For example, splicing two chromosomes
// can very easily create duplicates if the same star is listed in the
// first half of one chromosome's schedule and in the second half of
// the other schedule.) The function cleanout_duplicates() will fix
// this. For every star_id that's missing, there's another star_id
// that is duplicated. This will identify the duplicates, and replace
// each duplicate with one of the missing star_ids.

static int *missing_list;
static int *found_list;

void cleanout_duplicates(OBS_ELEMENT *e) {
  int i;
  int number_of_duplicates = 0;

  for(i=0; i<SIZEOFCHROMOSOME; i++) found_list[i] = 0;
  for(i=0; i<SIZEOFCHROMOSOME; i++) {
    found_list[e[i].star_id_no]++;
  }
  for(i=0; i<SIZEOFCHROMOSOME; i++) {
    if(found_list[i] == 0) {
      missing_list[number_of_duplicates++] = i;
    }
    found_list[i] = 0;
  }

  const int direction = int_random(0,1); // 0=forward/up, 1=reverse/down
  int count = SIZEOFCHROMOSOME;
  i = (direction ? SIZEOFCHROMOSOME-1 : 0);
  while(count--) {
    if(found_list[e[i].star_id_no]++) {
      // this is a duplicate
      e[i].star_id_no = missing_list[--number_of_duplicates];
    }
    i = (direction ? i-1 : i+1 );
  }

  // check the algorithm
  if(number_of_duplicates != 0) {
    fprintf(stderr, "cleanout_duplicates: number check failed: %d\n",
	    number_of_duplicates);
  }
}
      
void
time_adjust(int element,
	    OBS_ELEMENT *src,
	    OBS_ELEMENT *tgt) {
  for(int i = 0; i<SIZEOFCHROMOSOME; i++) {
    tgt[i].star_id_no = src[i].star_id_no;
    tgt[i].time_index_no = src[i].time_index_no;
  }

  tgt[element].time_index_no = int_random(0, TIME_INDEX_ENTRIES);
}

void neighbor_rotate(OBS_ELEMENT *src,
		     OBS_ELEMENT *tgt) {
  // how many in the source are not observable?
  int unobservable = 0;
  int element_being_moved;
  int i;
  for(i=0; i<SIZEOFCHROMOSOME; i++) {
    if(src[i].result != RES_OK) unobservable++;
  }
  element_being_moved = int_random(0, unobservable-1);
  for(i=0; i<SIZEOFCHROMOSOME; i++) {
    tgt[i].star_id_no = src[i].star_id_no;
    tgt[i].time_index_no = src[i].time_index_no;
  }

  tgt[element_being_moved].star_id_no =
    src[element_being_moved+1].star_id_no;
  tgt[element_being_moved].time_index_no =
    src[element_being_moved+1].time_index_no;

}

////////////////////////////////////////////////////////////////////
//        splice(OBS_ELEMENT *e1, *e2, *tgt)
//  This function will splice the front of e1 onto the aft end of e2,
//  putting the result into tgt, rewriting anything that might have
//  been in tgt previously.  One random number is used, specifying the
//  number of elements pulled from e1.
////////////////////////////////////////////////////////////////////

void splice(INDIVIDUAL *i1, INDIVIDUAL *i2, INDIVIDUAL *tgt_i) {
  const bool short_rotate = (float_random(0.0, 1.0) < 0.5);
  const int last_slot = std::max(4, (short_rotate ? i1->useful_length : SIZEOFCHROMOSOME));
  const int split = int_random(1, last_slot-2);
  OBS_ELEMENT *e1 = i1->chromosome;
  OBS_ELEMENT *e2 = i2->chromosome;
  OBS_ELEMENT *tgt = tgt_i->chromosome;
  
  for(int i = 0; i<SIZEOFCHROMOSOME; i++) {
    tgt[i].star_id_no = ((i<split)? e1[i].star_id_no : e2[i].star_id_no);
    tgt[i].time_index_no = ((i<split)? e1[i].time_index_no : e2[i].time_index_no);
  }
  cleanout_duplicates(tgt);
}

void random_swap(INDIVIDUAL *i1, INDIVIDUAL *tgt_i) {
  const bool short_rotate = (float_random(0.0, 1.0) < 0.5);
  const int last_slot = std::max(4, (short_rotate ? i1->useful_length : SIZEOFCHROMOSOME));
  const int n1 = int_random(0, last_slot-1);
  int n2;
  do {
    n2 = int_random(0, last_slot-1);
  } while(n2 == n1);	     // can't choose the same individual to be
				// swapped with itself
  OBS_ELEMENT *e1 = i1->chromosome;
  OBS_ELEMENT *tgt = tgt_i->chromosome;

  for(int i = 0; i<SIZEOFCHROMOSOME; i++) {
    tgt[i].star_id_no = e1[i].star_id_no;
    tgt[i].time_index_no = e1[i].time_index_no;
  }

  int x1 = tgt[n1].star_id_no;
  int x2 = tgt[n1].time_index_no;
  tgt[n1].star_id_no = tgt[n2].star_id_no;
  tgt[n1].time_index_no = tgt[2].time_index_no;
  tgt[n2].star_id_no = x1;
  tgt[n2].time_index_no = x2;
}
  
////////////////////////////////////////////////////////////////////
//        inner_rotate(OBS_ELEMENT *e, *tgt)
//  This function will start with e, identify a sub-string somewhere
//  inside of e, and will perform a logical rotation to the right of
//  that substring, replacing the substring.  The result is put into
//  tgt (the original e is left unchanged).  Anything previously put
//  into tgt is overwritten.  Three random numbers are used: n1
//  specifies the beginning of the substring, n2 specifies the end,
//  and "jump" specifies how many "genes" of rotation are to be
//  applied. 
////////////////////////////////////////////////////////////////////

void rotate_elements(OBS_ELEMENT *e,
		     OBS_ELEMENT *tgt,
		     int n1,	// left-most gene in rotation
		     int n2,	// right-most gene in rotation
		     int jump) { // # of places to rotate to the right
  int i;
  // copy e into tgt and then perform the rotation in place in tgt.
  for(i=0; i<SIZEOFCHROMOSOME; i++) {
    tgt[i].star_id_no = e[i].star_id_no;
    tgt[i].time_index_no = e[i].time_index_no;
  }

  // copy is finished, perform the rotation
  for(i=n1; i<=n2; i++) {
    int j = i+jump;
    if(j>n2) j = n1 + (j - n2 - 1);
    tgt[j].star_id_no = e[i].star_id_no;
    tgt[j].time_index_no = e[i].time_index_no;
  }
}

void inner_rotate(INDIVIDUAL *i1, INDIVIDUAL *tgt) {
  const bool short_rotate = (float_random(0.0, 1.0) < 0.5);
  const int last_slot = std::max(4, (short_rotate ? i1->useful_length : SIZEOFCHROMOSOME));
  const int n1   = int_random(0, last_slot-2);
  const int n2   = int_random(n1+1, last_slot-1);
  const int jump = int_random(1, n2-n1);

  //fprintf(stderr, "rotate: <%d, %d> shift %d, useful = %d\n", n1, n2, jump, i1->useful_length);

  rotate_elements(i1->chromosome, tgt->chromosome, n1, n2, jump);
}

////////////////////////////////////////////////////////////////////
//        void pair_swap()
//    This function swaps a pair of adjacent stars.
////////////////////////////////////////////////////////////////////
void pair_swap(OBS_ELEMENT *src,
	       OBS_ELEMENT *tgt,
	       int pair_bottom_index) {
  int i;

  for(i=0; i<SIZEOFCHROMOSOME; i++) {
    tgt[i].star_id_no = src[i].star_id_no;
    tgt[i].time_index_no = src[i].time_index_no;
  }

  // Perform verification check to make sure that we've been given a
  // valid value of pair_bottom_index.
  if(pair_bottom_index < 1 ||
     pair_bottom_index > SIZEOFCHROMOSOME - 1) {
    fprintf(stderr, "pair_swap: illegal pair_bottom_index = %d\n",
	    pair_bottom_index);
  } else {
    // Perform the swap.  Note that this will fail in unpleasant ways
    // if src and tgt both point to the exact same individual.  So
    // don't do it.
    tgt[pair_bottom_index - 1].star_id_no =
      src[pair_bottom_index].star_id_no;
    tgt[pair_bottom_index].star_id_no =
      src[pair_bottom_index - 1].star_id_no;

    tgt[pair_bottom_index - 1].time_index_no =
      src[pair_bottom_index].time_index_no;
    tgt[pair_bottom_index].time_index_no =
      src[pair_bottom_index - 1].time_index_no;
  }
}

////////////////////////////////////////////////////////////////////
//        main_loop()
////////////////////////////////////////////////////////////////////

void main_loop(const char *output_file) {
  int generation = 0;
  if (write_snapshot) {
    (void) unlink(snap_file_name);
  }
    
  do {
    generation++;
#ifdef DEBUG_POPULATION
    check_population();
#endif
    sort_population();		// sort based upon score
    if(write_log && ((generation % write_log) == 0 ||
		     generation == 1)) {
#ifndef NO_HASH
      int hash_tries, hash_hits, hash_size;
      get_hash_statistics(&hash_tries, &hash_hits, &hash_size);
      fprintf(stdout, "hash tries/hits/size = %d/%d/%d\n",
	      hash_tries, hash_hits, hash_size);
#endif
      summarize_generation(generation);
    }

    if(write_snapshot && (generation % write_snapshot) == 0) {
      write_snapshots();
    }

    // now that the population has been sorted by score, we leave the
    // top N_RETAIN individuals alone.
    for(int i=N_RETAIN; i<POPULATION_SIZE; i++) {
      if(population[i]->referenced_in_hashtable) {
	population[i] = new INDIVIDUAL;
      }

      // perform a random dice roll to decide whether we will create
      // this new individual by rotating an existing individual or by
      // splicing two individuals together.
      double this_random = float_random(0.0, 1.0);
      if(this_random < F_RANDOM_SWAP) {
	random_swap(population[int_random(0, N_RETAIN-1)], population[i]);
      } else if(this_random < F_ROTATE + F_RANDOM_SWAP) {
	inner_rotate(population[int_random(0, N_RETAIN-1)], population[i]);

      } else if(this_random < F_ROTATE+F_PAIR_SWAP+F_RANDOM_SWAP) {
	INDIVIDUAL *src = population[int_random(0, i-1)];
	INDIVIDUAL *tgt = population[i];
	const bool short_rotate = (float_random(0.0, 1.0) < 0.5);
	const int last_slot = std::max(4, (short_rotate ? src->useful_length : SIZEOFCHROMOSOME));
	pair_swap(src->chromosome, tgt->chromosome, int_random(1, last_slot-1));

      } else if(this_random < F_ROTATE+F_PAIR_SWAP+F_TIME_DELAY+F_RANDOM_SWAP) {
	INDIVIDUAL *src = population[int_random(0, i-1)];
	INDIVIDUAL *tgt = population[i];
	const bool short_rotate = (float_random(0.0, 1.0) < 0.5);
	const int last_slot = std::max(4, (short_rotate ? src->useful_length : SIZEOFCHROMOSOME));
	time_adjust(int_random(0, last_slot-1), src->chromosome, tgt->chromosome);

      } else {
	// otherwise do a slice
	// n1 and n2 are the indices of the two individuals we have
	// selected to slice together
	const int n1 = int_random(0, N_RETAIN-1);
	int n2;
	do {
	  n2 = int_random(0, N_RETAIN-1);
	} while(n2 == n1); // can't choose the same individual to be
			   // sliced to itself
	splice(population[n1],
	       population[n2],
	       population[i]);
      }

      // assign a score to this brand new individual.  If this
      // inidividual has never been seen before, the
      // referenced_in_hashtable flag will be set during this call.
      assign_score(population[i]);
    }
  } while(generation <= GENERATION_LIMIT);

  {
    FILE *fp_out = fopen(output_file, "w");
    if(!fp_out) {
      fprintf(stderr, "Cannot create output file %s\n", output_file);
      return;
    }
    
    fprintf(fp_out, "%f ", population[0]->score);
    for (auto stp : population[0]->trial.GetTrial()) {
      if (stp->result == RES_OK) {
	if (stp->oa->TypeOf() == AT_Dark ||
	    stp->oa->TypeOf() == AT_Flat) {
	  fprintf(fp_out, "%d %s %s %lf\n",
		  stp->oa->GetUniqueID(),
		  stp->oa->TypeString().c_str(),
		  stp->oa->TypeString().c_str(),
		  stp->scheduled_time.day());
	} else if(stp->oa->TypeOf() == AT_Time_Seq) {
	  fprintf(fp_out, "%d %s %s %lf %lf\n",
		  stp->oa->GetUniqueID(),
		  stp->oa->TypeString().c_str(),
		  stp->strategy->object(),
		  stp->scheduled_time.day(),
		  stp->scheduled_end_time.day());
	} else { // quick and script
	  fprintf(fp_out, "%d %s %s %lf\n",
		  stp->oa->GetUniqueID(),
		  stp->oa->TypeString().c_str(),
		  stp->strategy->object(),
		  stp->scheduled_time.day());
	}
      }
    }
    fclose(fp_out);
  }
}

////////////////////////////////////////////////////////////////////
//        sort_population(void)
//  Sort the entire population so that the individuals with the
//  highest scores come first.  This is important because we retain
//  the first "n" individuals of each population.  Duplicates are
//  given scores of zero.
////////////////////////////////////////////////////////////////////
static void simple_sort_population(void) {
  int i = 1;

  while(i < POPULATION_SIZE) {
    if(population[i-1]->score < population[i]->score) {
      // out of order
      INDIVIDUAL *temp = population[i];
      population[i] = population[i-1];
      population[i-1] = temp;
      if(i>1) i--;
    } else {
      i++;
    }
  }
}

/****************************************************************/
// The function sort_population() is straightforward except for its
// handling of "duplicates". (Note that these "identical individuals"
// (duplicates) are different from duplicated star_ids *within* a
// single individual.) It's important that we get rid of identical
// individuals so that duplicates of the best individual don't "push
// out" weaker individuals. If this is allowed to proceed unchecked,
// the entire population can become filled with identical individuals,
// which severely impairs the benefits of splicing to find potentially
// better solutions.
//
// Other than the handling of duplicates, sort_population() just sorts
// all the individuals within the population so that the one with the
// highest score is at the beginning of the list.
/****************************************************************/
static void sort_population(void) {
  simple_sort_population();
  // now find duplicates and give the second a score of zero
  int i;
  for(i=1; i<POPULATION_SIZE; i++) {
    int j = i+1;
    while(j < POPULATION_SIZE &&
	  population[i]->score == population[j]->score) {
      // now compare population[i] against population[j]
      int m = 0;		// chromosome index for population[i]
      int n = 0;		// chromosome index for population[j]
      do {
	while(m < SIZEOFCHROMOSOME &&
	      population[i]->chromosome[m].result != RES_OK) m++;
	while(n < SIZEOFCHROMOSOME &&
	      population[j]->chromosome[n].result != RES_OK) n++;
	// here's the only way out of this loop: reach the end of one
	// of the individuals.
	if(m == SIZEOFCHROMOSOME || n == SIZEOFCHROMOSOME) break;
	// Do they refer to the same star?
	if(population[i]->chromosome[m].star_id_no !=
	   population[j]->chromosome[n].star_id_no ||
	   population[i]->chromosome[m].time_index_no !=
	   population[j]->chromosome[n].time_index_no) goto next_star;
	m++;
	n++;
      } while(1);
      // fall out of loop means they're identical!
      population[i]->score = 0.0;
      
	  next_star:
      j++;
    } // end while all j values
  } // end for all i values
  
  // now re-sort to move the duplicates to the end with their "low"
  // scores of zero
  simple_sort_population();
}

////////////////////////////////////////////////////////////////////
//        UTILITIES
////////////////////////////////////////////////////////////////////

// Here are the two random number generators that are used everywhere
int int_random(int low_limit, int high_limit) {
  return low_limit + (int) ((high_limit-low_limit+1)*float_random(0.0, 1.0));
}

double float_random(double low_limit, double high_limit) {
  double factor =
    ((double)random())/(double)(unsigned long)0x7fffffff;
  return low_limit + (high_limit-low_limit)*factor;
}

void summarize_generation(int generation_number) {
  fprintf(stdout, "%6d ", generation_number);
  for(int i=0; i<12; i++) fprintf(stdout, "%5.1f ", population[i]->score);
  fprintf(stdout, "\n");
}

void write_snapshots(void) {
  FILE *snap_file = fopen(snap_file_name, "a");
  if(snap_file == 0) {
    fprintf(stderr, "unable to append to %s\n", snap_file_name);
  } else {
    print_top_three(population[0],
		    population[1],
		    population[2],
		    snap_file);
    // population[0]->print_sequence(snap_file);
    // calculate_score(population[0]->chromosome, snap_file);
    fclose(snap_file);
  }
}

/****************************************************************/
//        setup_stars()
// Perform initialization. Build data structures that will be needed
// elsewhere.
// Things to set up:
//    - stp_xref[]
//    - start and quit times
//    - create the strategy_time_pairs
//    - allocate the missing and found lists (for duplicate detection)
/****************************************************************/
void setup_stars(Schedule *schedule) {
  
  RequestingSession = schedule->executing_session();
  t_start = RequestingSession->SchedulingStartTime();
  t_quit = RequestingSession->SchedulingEndTime();
  INDIVIDUAL::t_start = t_start;
  INDIVIDUAL::t_quit = t_quit;

  SIZEOFCHROMOSOME = schedule->all_strategies.size();

  for (auto stp : schedule->all_strategies) {
    stp->needs_execution = 1;
    stp_xref.push_back(stp);
  }

  missing_list = (int *) malloc(SIZEOFCHROMOSOME*sizeof(int));
  found_list   = (int *) malloc(SIZEOFCHROMOSOME*sizeof(int));
}

/****************************************************************/
// The initial population is created by setting all individuals to an
// identical schedule that puts all the stars in order (arranged by
// star_id) with zero time delay. Then each individual is created by
// cloning the original individual but with a random rotation within
// the schedule. All the time delays are kept zero to start.
/****************************************************************/
void build_initial_population(void) {

  int i;

  population[0] = new INDIVIDUAL;
  for(i=0; i<SIZEOFCHROMOSOME; i++) {
    population[0]->chromosome[i].star_id_no = i;
    population[0]->chromosome[i].time_index_no = 0;
  }
  population[0]->useful_length = SIZEOFCHROMOSOME;

  for(i=1; i<POPULATION_SIZE; i++) {
    population[i] = new INDIVIDUAL;
    inner_rotate(population[0], population[i]);
  }

  for(i=0; i<POPULATION_SIZE; i++) {
    population[i]->useful_length = SIZEOFCHROMOSOME;
    assign_score(population[i]);
  }
}

void INDIVIDUAL::print_sequence(FILE *f) {
  int i;
  fprintf(f, "score = %f, sequence follows:\n", score);
  for(i=0; i<SIZEOFCHROMOSOME; i++) {
    chromosome[i].print_one_liner(f);
    fputs("\n", f);
  }
  fprintf(f, "\n");
}

void OBS_ELEMENT::print_one_liner(FILE *f) {
  char time_buffer[32];
  const char *result_buffer;
  char answer[64];

  switch(result) {
  case RES_OK: result_buffer = "OK     "; break;
  case RES_NOT_UP: result_buffer = "TOO LOW"; break;
  case RES_TOO_LATE: result_buffer = "TOOLATE"; break;
  case RES_USELESS:  result_buffer = "USELESS"; break;
  default: result_buffer = "<nil>  "; break;
  }

  strcpy(time_buffer, SourceSTP()->scheduled_time.to_string());
  
  answer[0] = time_buffer[11];
  answer[1] = time_buffer[12];
  answer[2] = time_buffer[13];
  answer[3] = time_buffer[14];
  answer[4] = time_buffer[15];

  const char *who = "";
  if (SourceSTP()->oa->TypeOf() == AT_Dark) who = "<Dark>";
  else if (SourceSTP()->oa->TypeOf() == AT_Flat) who = "<Flat>";
  else {
    who = star()->object();
  }
  sprintf(answer+5, " %7s %s", who, result_buffer);

  fputs(answer, f);
}

void build_summary_list(INDIVIDUAL *i, std::vector<std::string> &target) {
  for (auto t : i->trial.GetTrial()) { // "t" is an <output> STP
    ObservingAction *oa = t->oa;
    char time_buffer[32];
    const char *result_buffer;
    char answer[64];

    switch(t->result) {
    case RES_OK: result_buffer = "OK     "; break;
    case RES_NOT_UP: result_buffer = "TOO LOW"; break;
    case RES_TOO_LATE: result_buffer = "TOOLATE"; break;
    case RES_USELESS:  result_buffer = "USELESS"; break;
    default: result_buffer = "<nil>  "; break;
    }

    strcpy(time_buffer, t->scheduled_time.to_string());
  
    answer[0] = time_buffer[11];
    answer[1] = time_buffer[12];
    answer[2] = time_buffer[13];
    answer[3] = time_buffer[14];
    answer[4] = time_buffer[15];

    const char *who = "";
    if (oa->TypeOf() == AT_Dark) who = "<Dark>";
    else if (oa->TypeOf() == AT_Flat) who = "<Flat>";
    else {
      who = oa->strategy()->object();
    }
    sprintf(answer+5, " %12s %s: %7.3lf  ", who, result_buffer, t->score);
    target.push_back(std::string(answer));
  }
}
    

void print_top_three(INDIVIDUAL *i1, INDIVIDUAL *i2, INDIVIDUAL *i3,
		     FILE *f) {
  std::vector<std::string> list1, list2, list3;

  build_summary_list(i1, list1);
  build_summary_list(i2, list2);
  build_summary_list(i3, list3);

  unsigned int max_i = list1.size();
  if (max_i < list2.size()) max_i = list2.size();
  if (max_i < list3.size()) max_i = list3.size();

  fprintf(f, "\nscore = %f, score = %f, score = %f\n",
	  i1->score,
	  i2->score,
	  i3->score);

  for (unsigned int i=0; i<max_i; i++) {
    const char *string1 = "";
    const char *string2 = "";
    const char *string3 = "";

    if (i < list1.size()) string1 = list1[i].c_str();
    if (i < list2.size()) string2 = list2[i].c_str();
    if (i < list3.size()) string3 = list3[i].c_str();

    fprintf(f, "%s%s%s\n", string1, string2, string3);
  }
  fprintf(f, "\n");
}

//****************************************************************
//        TRIAL
//****************************************************************

// return an iterator to the first item that starts *after* the
// specified time 
std::pair<stp_it, bool>
TRIAL::FindTime(JULIAN when) {
  if (when < t_start) return {trial.end(), false};
  
  for (stp_it item = trial.begin(); item != trial.end(); item++) {
    if (when < (*item)->scheduled_time) return {item, true};
  }

  return {trial.end(), when < t_quit};
}

std::pair<stp_it, bool>
TRIAL::FindFirstGap(double length_seconds) {
  JULIAN prior_t = t_start;
  stp_it item;
  for (item = trial.begin(); item != trial.end(); item++) {
    if ((*item)->scheduled_time - prior_t >= length_seconds) {
      return {item, true};
    }
    prior_t = (*item)->scheduled_end_time;
  }
  return {trial.end(), t_quit-prior_t >= length_seconds};
}
  
// Both of these take "input" stp's as an argument and return
// pointers to "output" stp's.
// This is used for everything *except* AT_Time_Seq. (so, AT_Quick,
// AT_Script, AT_Dark, AT_Flat)
Schedule::strategy_time_pair *
TRIAL::InsertInFirstGap(Schedule::strategy_time_pair *stp,
			Schedule::strategy_time_pair *precedent,
			double padding_in_seconds, // only applies if
						   // immediately
						   // follows precedent
			JULIAN insert_after_time) {
  
  const double duration_days = ((padding_in_seconds + stp->oa->execution_time_prediction())
				/(24.0*3600.0));
  bool precedent_found = (precedent == nullptr ? true : false);

  stp_it item;
  JULIAN prior_end = t_start;
  // this loop will be terminated when "item" points to the first item
  // *after* a gap where this can be inserted.
  for (item = trial.begin(); item != trial.end(); item++) {
    if (precedent_found &&
	prior_end + duration_days <= (*item)->scheduled_time) {
      if (insert_after_time > 0.0 and prior_end > insert_after_time) {
	break;
      }
    }
    if (*item == precedent) precedent_found = true;
    prior_end = (*item)->scheduled_end_time;
  }
  if (item == trial.end()) {
    // does it fit at the end?
    if (INDIVIDUAL::t_quit >= prior_end + duration_days) {
      // Yes!
      ; // fall through
    } else {
      // Nope.
      return nullptr;
    }
  }
  Schedule::strategy_time_pair *new_stp = new Schedule::strategy_time_pair;
  *new_stp = *stp;
  trial.insert(item, new_stp);
  new_stp->scheduled_time = prior_end + padding_in_seconds/(24.0*3600.0);
  new_stp->scheduled_end_time = (new_stp->scheduled_time + stp->oa->execution_time_prediction()/
				 (24.0*3600.0));
  return new_stp;
}

// This is only used with AT_Time_Seq
Schedule::strategy_time_pair *
TRIAL::InsertFixedTime(Schedule::strategy_time_pair *stp) {
  JULIAN when = stp->scheduled_time;

  if (when < INDIVIDUAL::t_start or
      stp->scheduled_end_time > INDIVIDUAL::t_quit) return nullptr;

  stp_it item;
  for (item = trial.begin(); item != trial.end(); item++) {
    if (when < (*item)->scheduled_time) break;
  }
  // Should (maybe) get inserted prior to "item".
  if (item != trial.end() and (*item)->scheduled_time < stp->scheduled_end_time) {
    // Nope, doesn't fit
    return nullptr;
  }
  // Yes, it does fit.
  Schedule::strategy_time_pair *new_stp = new Schedule::strategy_time_pair;
  *new_stp = *stp;
  trial.insert(item, new_stp);
  return new_stp;
}

