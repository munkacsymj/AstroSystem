/*  simple_stack.cc -- support functions to stack.cc
 *
 *  Copyright (C) 2007 Mark J. Munkacsy
 *
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
#include <stdio.h>
#include <Image.h>
#include <IStarList.h>
#include <ctype.h>

// image_match: returns 0 on success, 1 if failed
// puts resulting x and y offsets into *del_x and *del_y.
int simple_image_match(IStarList *i1_list, IStarList *i2_list,
		       double expected_x, double expected_y,
		       double *del_x, double *del_y) {

  const int i1_size = i1_list->NumStars;
  const int i2_size = i2_list->NumStars;
  const int poss_matches = ((i1_size < i2_size) ? i1_size : i2_size);

  if(poss_matches < 1) return 1;
  
  double *x_offsets = (double *) malloc(sizeof(double) * poss_matches);
  double *y_offsets = (double *) malloc(sizeof(double) * poss_matches);

  if(x_offsets == 0 || y_offsets == 0) {
    fprintf(stderr, "simple_stack: cannot malloc()\n");
    return 1;
  }

  fprintf(stderr, "------\n");
  int matches = 0;

  double sum_err_x = 0.0;
  double sum_err_y = 0.0;
  int j1, j2;
  for(j1 = 0; j1 < i1_size; j1++) {
    IStarList::IStarOneStar *star = i1_list->FindByIndex(j1);
    char *ref_name = star->StarName;
    if((star->validity_flags & CORRELATED) == 0) {
      continue;
    }
    
    for(j2 = 0; j2 < i2_size; j2++) {
      if(strcmp(ref_name, i2_list->FindByIndex(j2)->StarName) == 0 &&
	 !(ref_name[0] == 'S' && isdigit(ref_name[1]))) {
	// Match!!
	// fprintf(stderr, "  match: %s\n", ref_name);
	sum_err_x += (x_offsets[matches] = -(i1_list->StarCenterX(j1) -
					     i2_list->StarCenterX(j2)));
	sum_err_y += (y_offsets[matches] = -(i1_list->StarCenterY(j1) -
					     i2_list->StarCenterY(j2)));
	if (x_offsets[matches] > 30.0 || y_offsets[matches] > 30.0) {
	  fprintf(stderr, "M: %s: %.1lf, %.1lf\n",
		  ref_name, x_offsets[matches], y_offsets[matches]);
	}
	matches++;
	break;
      }  // end if
    } // end loop over i2_list
  } // end loop over i1_list

  if(matches < 1) return 1;

  *del_x = sum_err_x/matches;
  *del_y = sum_err_y/matches;

  double sum_sq_x = 0.0;
  double sum_sq_y = 0.0;
  
  for(j1 = 0; j1 < matches; j1++) {
    double err_x = *del_x - x_offsets[j1];
    double err_y = *del_y - y_offsets[j1];

    sum_sq_x  += (err_x * err_x);
    sum_sq_y  += (err_y * err_y);
  }


  fprintf(stderr, "(simple) Offset = (%f, %f), stdev = (%f, %f), %d matches\n",
	  *del_x, *del_y,
	  sqrt(sum_sq_x/matches), sqrt(sum_sq_y/matches),
	  matches);
  return 0;
}
  
  
    
