/*  Statistics.cc -- pixel statistics in an image, including
 *  histogram-based statistics 
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
#include "Statistics.h"
#include <stdlib.h>		// for qsort()
#include <string.h>
#include <stdio.h>

static const int ThresholdSize = 50;

static void *
MedianSmall(void *base,
	    int nelements,
	    int selectedElement,
	    int elementSize,
	    int (*Compare)(const void *, const void *)) {
  qsort(base, nelements, elementSize, Compare);
  return (char *) base + selectedElement*elementSize;
}

static void *
MedianSelect(void *base,
	     int nelements,
	     int selectedElement,
	     int elementSize,
	     int (*Compare)(const void *, const void *)) {
  if(nelements <= ThresholdSize)
    return MedianSmall(base,
		       nelements,
		       selectedElement,
		       elementSize,
		       Compare);

  // It's not a small set. Find the median of a randomly-selected chunk.
  int sample1 = (nelements - ThresholdSize)/2;

  void *trial = MedianSmall((char *) base + sample1*elementSize,
			    ThresholdSize,
			    ThresholdSize/2,
			    elementSize,
			    Compare);

  // Now count the number less than (L), equal to(E), and more than
  // (M) the "trial" value
  int L = 0;
  int E = 0;
  int M = 0;

  for(int i=0; i<nelements; i++) {
    int comp = (*Compare)(trial, (char *) base + i*elementSize);
    if(comp == 0) E++;
    if(comp < 0) M++;
    if(comp > 0) L++;
  }

  if(L <= selectedElement && (L+E) >= selectedElement)
    return trial;

  if(L > selectedElement) {
    void *newbase = malloc(L*elementSize);
    void *nextbase = newbase;
    if(!newbase) {
      perror("Statistics: median: cannot allocate memory");
      return trial;		// wrong, but anything better available?
    }
    for(int j=0; j<nelements; j++) {
      int comp = (*Compare)(trial, (char *) base + j*elementSize);
      if(comp > 0) {
	memcpy(nextbase, (char *) base + j*elementSize, elementSize);
	nextbase = (void *) ((char *)nextbase + elementSize);
      }
    }
    void *answer = MedianSelect(newbase,
				L,
				selectedElement,
				elementSize,
				Compare);
    for(int j=0; j<nelements;j++) {
      const int comp = (*Compare)(answer, (char *) base + j*elementSize);
      if(comp == 0) {
	answer = (char *) base + j*elementSize;
	break;
      }
    }
				    
    free(newbase);
    return answer;
  }

  if((L+E) < selectedElement) {
    void *newbase = malloc(M*elementSize);
    void *nextbase = newbase;
    if(!newbase) {
      perror("Statistics: median: cannot allocate memory");
      return trial;		// wrong, but anything better available?
    }
    for(int j=0; j<nelements; j++) {
      int comp = (*Compare)(trial, (char *) base + j*elementSize);
      if(comp < 0) {
	memcpy(nextbase, (char *) base + j*elementSize, elementSize);
	nextbase = (void *) ((char *)nextbase + elementSize);
      }
    }
    void *answer = MedianSelect(newbase,
				M,
				selectedElement - L - E,
				elementSize,
				Compare);
    for(int j=0; j<nelements;j++) {
      const int comp = (*Compare)(answer, (char *) base + j*elementSize);
      if(comp == 0) {
	answer = (char *) base + j*elementSize;
	break;
      }
    }
    free(newbase);
    return answer;
  }

  fprintf(stderr, "Statistics: median: logic flaw\n");
  fprintf(stderr, "Statistics: L = %d\n", L);
  fprintf(stderr, "Statistics: E = %d\n", E);
  fprintf(stderr, "Statistics: M = %d\n", M);
  fprintf(stderr, "Statistics: selectedElement = %d\n", selectedElement);
  
  return 0;
}
  
void *
Median(void *base,
       int nelements,
       int elementSize,
       int (*Compare)(const void *, const void *)) {
  return MedianSelect(base,
		      nelements,
		      nelements/2,
		      elementSize,
		      Compare);
}

// This is a more generic form of Median(). Instead of finding the
// median (the point with 50% of the sample at a lower value). In this
// form, we specify "lower_limit", which must be in the range of
// 0..nelements. A pointer to the element at that point is returned.
void *HistogramPoint(void *base,
		     int nelements,
		     int elementSize,
		     int (*Compare)(const void *, const void *),
		     int lower_limit) {
  return MedianSelect(base,
		      nelements,
		      lower_limit,
		      elementSize,
		      Compare);
}
  
