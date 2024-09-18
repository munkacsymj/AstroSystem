// This may look like C code, but it is really -*- C++ -*-
/*  obs_spreadsheet.h -- maintains the aavso.csv spreadsheet that
 *  lists all observations made during the session
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

#ifndef _OBS_SPREADSHEET_H
#define _OBS_SPREADSHEET_H
#include <julian.h>

// SpreadSheet_Filelist is a list of all the image files used to
// support a particular entry in the spreadsheet.
class SpreadSheet_Filelist {
public:
  SpreadSheet_Filelist(void);
  ~SpreadSheet_Filelist(void);

  void Add_Filename(const char *filename);

  char *GetImageList(void);
private:
  int current_allocation_size;
  int current_size;
  int *NumberList;
};

void
Initialize_Spreadsheet(const char * spreadsheet_name);

// The star_name being provided here is the star name in its
// "common" form as we would list it in a photometry file. The
// Add_Spreadsheet_Entry method will convert it to AAVSO form.
void
Add_Spreadsheet_Entry(const char           * star_name,
		      const char           * star_designation,
		      SpreadSheet_Filelist * filelist,
		      JULIAN                 obs_time);

void
Finalize_Spreadsheet(void);

#endif
