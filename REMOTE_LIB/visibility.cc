/*  visibility.cc -- Local horizon definition (trees, structures, etc)
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
#include <stdio.h>
#include "visibility.h"

// return 1 if object is in visible part of the sky; return 0 if
// below observing horizon
//visibility table:
#if 0
// This first table is the horizon map built for the original
// observatory position next to the fence with the LX200 in it on a
// tripod. 
static double vis_table[][2] = { { -180.0, 45.0 },
			  { -131.0, 5.0 }, // ???
			  { -97.0, 5.0 },
			  { -89.0, 5.5 },
			  { -76.0, 9.0 },
			  { -60.0, 18.0 },
			  { -39.0, 23.0 },
			  { -17.0, 24.0 },
			  { -3.0, 24.0 },
			  { 2.0, 11.0 },
			  { 6.0, 11.0 },
			  { 8.0, 18.0 },
			  { 14.0, 24.0 },
			  { 19.0, 24.0 },
			  { 25.0, 20.0 },
			  { 33.0, 22.0 },
			  { 39.0, 18.0 },
			  { 44.0, 23.0 },
			  { 56.0, 29.0 },
			  { 66.0, 28.0 },
			  { 71.0, 19.0 },
			  { 77.0, 19.0 },
			  { 86.0, 19.0 },
			  { 91.0, 23.0 },
			  { 102.0, 23.0 },
			  { 106.0, 19.0 },
			  { 110.0, 20.0 },
			  { 117.0, 25.0 },
			  { 129.0, 22.0 },
			  { 136.0, 21.0 },
			  { 145.0, 33.0 },
			  { 160.0, 34.0 },
			  { 170.0, 34.0 },
			  { 171.0, 45.0 },
			  { 180.0, 45.0 }};
#endif
#if 0
// This second table is for the MI-250 in the larger Sherwood observatory a
// full 3 feet from the property line prior to any renovation of the
// house.
// Note that +-180 is North and 0 is South, with angles increasing
// clockwise.
static double vis_table[][2] = { { -180.0, 35.0 }, // North
			  { -174.2, 27.7 },
			  { -168.9, 12.0 },
			  { -165.8, 3.0 },
			  {  -97.0, 3.0 },
			  {  -92.5, 4.8 },
			  {  -83.7, 8.4 },
			  {  -74.8, 12.3 },
			  {  -66.2, 17.9 },
			  {  -57.1, 20.4 },
			  {  -37.9, 22.7 },
			  {  -20.4, 25.9 },
			  {   -9.5, 18.6 },
			  {   -2.9, 13.4 },
			  {    4.1, 13.3 },
			  {    7.7, 9.0 },
			  {    9.5, 18.7 },
			  {   15.9, 23.1 },
			  {   25.1, 19.6 },
			  {   34.5, 17.8 }, // roof (garage)
			  {   53.5, 18.5 },
			  {   68.3, 19.7 },
			  {   98.4, 18.8 },
			  {  114.3, 20.2 },
			  {  122.5, 25.2 },
			  {  132.1, 23.2 },
			  {  139.6, 19.5 },
			  {  146.2, 22.4 },
			  {  156.7, 34.5 },
			  {  172.1, 34.9 },
			  {  180.0, 35.0 }};
			  
#endif
#if 1
// This third table is the horizon map built for the Vanderbilt
// observatory position 

static double vis_table[][2] = { { (0.0-180.0), 0.0 },
				 { (13.5-180.0), 1.0 },
				 { (21.0-180.0), 4.0 },
				 { (30.0-180.0), 6.0 },
				 { (36.0-180.0), 6.0 },
				 { (43.5-180.0), 2.0 },
				 { (66.0-180.0), 0.0 },
				 { (67.5-180.0), 3.0 },
				 { (75.5-180.0), 3.5 },
				 { (82.0-180.0), 9.0 },
				 { (89.7-180.0), 8.7 },
				 { (101.5-180.0), 4.0 },
				 { (106.5-180.0), 0.0 },
				 { (125.5-180.0), 0.0 },
				 { (131.0-180.0), 6.2 },
				 { (137.6-180.0), 8.2 },
				 { (161.3-180.0), 8.0 },
				 { (168.3-180.0), 0.5 },
				 { (177.0-180.0), 0.5 },
				 { (215.0-180.0), 7.5 },
				 { (229.0-180.0), 8.5 },
				 { (271.0-180.0), 15.0 },
				 { (296.0-180.0), 14.0 },
				 { (315.0-180.0), 10.0 },
				 { (328.0-180.0), 9.0 },
				 { (343.0-180.0), 7.0 },
				 { (348.0-180.0), 9.0 },
				 { (356.0-180.0), 9.5 },
				 { (360.0-180.0), 0.0 }};
#endif

int IsVisible(ALT_AZ alt_az, JULIAN when) {
  double alt = alt_az.altitude_of()*180.0/M_PI;
  double az  = alt_az.azimuth_of()*180.0/M_PI;

  if(alt > 45.0) return 1; // speed
  if(alt < 5.0) return 0;  // speed

  const int index_limit = sizeof(vis_table)/sizeof(vis_table[0]) - 1;
  for(int j = 0; j < index_limit; j++) {
    if(az >= vis_table[j][0] &&
       az <= vis_table[j+1][0]) {
      double fraction = (az - vis_table[j][0])/
	(vis_table[j+1][0] - vis_table[j][0]);
      double adder = fraction * (vis_table[j+1][1] - vis_table[j][1]);
      double el_limit = adder + vis_table[j][1];

      return alt >= el_limit;
    }
  }
  fprintf(stderr, "Strategy:IsVisible: lookup failure for az = %f\n", az);
  return 0;
}
