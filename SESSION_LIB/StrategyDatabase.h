// This may look like C code, but it is really -*- C++ -*-
/*  StrategyDatabase.h -- operations to maintain & query the strategy
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
#ifndef _STRATEGYDATABSE_H
#define _STRATEGYDATABSE_H
#include <strategy.h>

extern const struct StrategyDatabaseEntry *AMBIGUOUS;

struct StrategyDatabaseEntry {
  const char *local_name;
  const char *strategy_filename;
  const char *designation;
  const char *chartname;
  const char *reporting_name;
  char AAVSO_UID[12];		// e.g., "000-BBL-715"
};

const StrategyDatabaseEntry *LookupByDesignation(char *designation);
const StrategyDatabaseEntry *LookupByReportingName(char *name);
const StrategyDatabaseEntry *LookupByAUID(char *name);
StrategyDatabaseEntry *LookupByLocalName(char *local_name);

void SetupStrategyDatabase(void);
void ClearStrategyDatabase(void);
void AddStrategyToDatabase(Strategy *strategy,
			   const char *strategy_filename);
StrategyDatabaseEntry *
CreateBlankEntryInDatabase(void);

void SaveStrategyDatabase(void);

#endif
