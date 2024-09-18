/*  StrategyDatabase.cc -- operations to maintain & query the strategy
 *       database 
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
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <StrategyDatabase.h>
#include <gendefs.h>

/*
 * Okay, so what *is* a Strategy Database and why do we need one?
 * It is used during post-analysis, when we have an aavso.csv file and
 * we need to discover information about a star that will be included
 * in a report. At that point, all we have is this text file, and no
 * easy way to discover pertinent information about the stars listed in
 * the file. In particular, we need to find their Harvard designation,
 * their "official" AAVSO name (from the validation file), and so
 * forth.  We pull that information out of the strategy database.
 * In order to have confidence that the strategy database is up to date,
 * we frequently rebuild the database from scratch.
 */

////////////////////////////////////////////////////////////////
//        Format of the StrategyDatabase:
//   1. Local_Name		(what I call it, in *.phot files)
//   2. Reporting_Name		(Must match validation file)
//   3. Strategy_filename	(not currently used: blank)
//   4. Designation		(From validation file, 9999+99 otherwise)
//   5. Chartname
//   6. AAVSO UID               (e.g., "000-BBL-715")
////////////////////////////////////////////////////////////////

static int array_size = 0;
static int number_entries = 0;
static struct StrategyDatabaseEntry *main_array = 0;

const struct StrategyDatabaseEntry *AMBIGUOUS =
     (struct StrategyDatabaseEntry *) (-1);

static int
sloppy_cmp(const char *name1, const char *name2) {
  while(*name1 && *name2) {
    if(*name1 == ' ') {
      if(*name2 != ' ' && *name2 != '-') return 1;
    } else if(*name1 == '-') {
      if(*name2 != ' ' && *name2 != '-') return 1;
    } else if(tolower(*name1) != tolower(*name2)) return 1; // no match
    name1++;
    name2++;
  }
  return (*name1 != *name2);	// match
}

StrategyDatabaseEntry *
CreateBlankEntryInDatabase(void) {
  if(array_size == number_entries) {
    static const int INCREMENT_SIZE = 100;

    struct StrategyDatabaseEntry *new_array = new struct StrategyDatabaseEntry [array_size + INCREMENT_SIZE];
    array_size += INCREMENT_SIZE;
    int j;
    for(j=0; j<number_entries; j++) {
      new_array[j] = main_array[j]; // structure assignment
    }
    if(main_array) delete [] main_array;
    main_array = new_array;
  }

  return &main_array[number_entries++];
}

const
StrategyDatabaseEntry *LookupByDesignation(char *designation) {
  int j;
  for(j=0; j<number_entries; j++) {
    if(sloppy_cmp(main_array[j].designation, designation) == 0)
      return &main_array[j];
  }
  return 0;
}

const StrategyDatabaseEntry *
LookupByReportingName(char *name) {
  int j;
  for(j=0; j<number_entries; j++) {
    if(sloppy_cmp(main_array[j].reporting_name, name) == 0)
      return &main_array[j];
  }
  return 0;
}

const StrategyDatabaseEntry *
LookupByAUID(char *name) {
  int j;
  for(j=0; j<number_entries; j++) {
    if(sloppy_cmp(main_array[j].AAVSO_UID, name) == 0)
      return &main_array[j];
  }
  return 0;
}

StrategyDatabaseEntry *
LookupByLocalName(char *local_name) {
  int j;
  for(j=0; j<number_entries; j++) {
    if(sloppy_cmp(main_array[j].local_name, local_name) == 0)
      return &main_array[j];
  }
  return 0;
}

static const char *StrategyDatabaseFilename = STRATEGY_DIR "/StrategyDatabase";

void SetupStrategyDatabase(void) {
  char buffer[256];

  // if already setup, just quietly return
  if(number_entries) return;
  
  FILE *fp = fopen(StrategyDatabaseFilename, "r");
  if(!fp) {
    fprintf(stderr, "StrategyDatabase cannot be opened.\n");
    return;
  }

  while(fgets(buffer, sizeof(buffer), fp)) {
    // break into words at tabs
    const char *words[12];
    int num_words = 0;

    for(int i=0; i<12; i++) words[i] = "";

    char *s = buffer;
    words[num_words++] = buffer;
    while(*s && (*s != '\n')) {
      if(*s == '\t') {
	*s = 0;
	words[num_words++] = s+1;
      }
      s++;
    }
    *s = 0;

    // line has been broken. Build a StrategyDatabaseEntry
    struct StrategyDatabaseEntry *entry = CreateBlankEntryInDatabase();
    entry->local_name        = strdup(words[0]);
    entry->strategy_filename = strdup(words[2]);
    entry->designation       = strdup(words[3]);
    entry->chartname         = strdup(words[4]);
    entry->reporting_name    = strdup(words[1]);
    strcpy(entry->AAVSO_UID, words[5]);
  }
  fclose(fp);
}
  
void ClearStrategyDatabase(void) {
  if(array_size) {
    array_size = number_entries = 0;
    delete [] main_array;
    main_array = 0;
  }
}

void AddStrategyToDatabase(Strategy *strategy, const char *strategy_filename) {
    struct StrategyDatabaseEntry *entry = CreateBlankEntryInDatabase();

    entry->local_name        = strdup(strategy->object());
    entry->strategy_filename = strdup(strategy_filename);
    entry->designation       = strdup(strategy->Designation());
    entry->chartname         = strdup(strategy->ObjectChart());
    entry->reporting_name    = strdup(strategy->ReportName());
    entry->AAVSO_UID[0]      = 0;
}
  
void SaveStrategyDatabase(void) {
  FILE *fp = fopen(StrategyDatabaseFilename, "w");

  if(!fp) {
    fprintf(stderr, "StrategyDatabase cannot be written.\n");
    return;
  }

  int j;
  for(j=0; j<number_entries; j++) {
    fprintf(fp, "%s\t%s\t%s\t%s\t%s\t%s\n",
	    main_array[j].local_name,
	    main_array[j].reporting_name,
	    main_array[j].strategy_filename,
	    main_array[j].designation,
	    main_array[j].chartname,
	    main_array[j].AAVSO_UID);
  }
  fclose(fp);
}
