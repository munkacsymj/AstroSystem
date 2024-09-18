// This may look like C code, but it is really -*- C++ -*-
/*  validation.h -- services to validate star info against the AAVSO
 *  validation file 
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

#ifndef VALIDATION_H
#define VALIDATION_H

// We will verify that all the stars in our "stars" directory have
// entries in the AAVSO validation file found in that directory.

void initialize_validation_file(const char *validation_directory);

/* validate_star() returns zero if the validation was successful; */
/* non-zero is returned if the validation failed.  If it fails and */
/* suppress_messages is zero, then a diagnostic message will be put */
/* onto stderr. */
int validate_star(const char *designation,
		  const char *full_name,
		  int suppress_messages);

void validation_finished(void);

#endif /* VALIDATION_H */
