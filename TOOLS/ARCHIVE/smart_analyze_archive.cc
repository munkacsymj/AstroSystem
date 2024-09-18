/*  smart_analyze_archive.cc -- Pulls data out of the archive for
 *  analysis looking for possible variability
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
#include <string.h>
#include <stdlib.h>		// exit()
#include <math.h>
#include <stdio.h>
#include <ctype.h>

struct star_info {
  star_info *next;
  char name[32];
  double sum_mv;
  double sum_mv_sq;
  int    num_obs;

  double avg;
  double sigma;
  double brightest;
  double dimmest;
} *head;

int num_stars = 0;

star_info *
FindByName(const char *starname) {
  star_info *p;

  for(p=head; p; p=p->next) {
    if(strcmp(starname, p->name) == 0) return p;
  }

  p = new star_info;
  p->next = head;
  head = p;
  strcpy(p->name, starname);
  p->sum_mv =
    p->sum_mv_sq = 0.0;
  p->brightest = 99.0;
  p->dimmest   = -99.0;
  p->num_obs   = 0;

  num_stars++;

  return p;
}
    
int
main(int argc, char **argv) {
  FILE *fp_in = fopen("/usr/local/ASTRO/ARCHIVE/archive.dat", "r");
  if(!fp_in) {
    fprintf(stderr, "Cannot open archive file.\n");
    exit(-2);
  }

  FILE *fp_out = fopen("/tmp/analyze.out", "w");
  if(!fp_out) {
    fprintf(stderr, "Cannot open output file in /tmp\n");
    exit(-2);
  }

  char orig_line[132];
  int num_obs = 0;

  while(fgets(orig_line, sizeof(orig_line), fp_in)) {
    if(orig_line[0] != 'S' ||
       orig_line[1] != '=') continue;

    char *s = orig_line;
    while(*s != ' ') s++;
    *s = 0;

    if(*(s+1) != 'M' || *(s+2) != 'V' || *(s+3) != '=' || !isdigit(*(s+4)))
      continue;

    double mag;
    sscanf(s+4, "%lf", &mag);

    if(mag < 0.0 || mag > 20.0) {
      fprintf(stderr, "err: invalid magnitude of %f for %s\n",
	      mag, orig_line+2);
      continue;
    }

    star_info *star = FindByName(orig_line+2);
    
    star->sum_mv += mag;
    star->sum_mv_sq += (mag*mag);
    star->num_obs++;

    if(mag < star->brightest) star->brightest = mag;
    if(mag > star->dimmest)   star->dimmest   = mag;

    if((num_obs++ % 1000) == 0)
      fprintf(stderr, "%d obs so far.\n", num_obs);
  }

  fprintf(stdout, "Processed %d observations on %d different stars.\n",
	  num_obs, num_stars);

  star_info *star;

  for(star=head; star; star=star->next) {
    star->avg = (star->sum_mv/star->num_obs);
    if(star->num_obs > 1)
      star->sigma = sqrt((star->sum_mv_sq - star->num_obs*star->avg*star->avg)/(star->num_obs - 1));
    else
      star->sigma = 0.0;

    char flag = ' ';
    if((star->dimmest - star->brightest) < 3*star->sigma &&
       star->num_obs > 15 &&
       fabs(star->avg - (star->dimmest + star->brightest)/2.0) < 0.2*(star->dimmest - star->brightest)) flag = '&';

    fprintf(fp_out, "%32s %.3f %.2f %.2f %.3f %d %c\n",
	    star->name,
	    star->avg,
	    star->brightest,
	    star->dimmest,
	    star->sigma,
	    star->num_obs,
	    flag);
  }

  fprintf(stdout, "Answer put into /tmp/analyze.out\n");
}
