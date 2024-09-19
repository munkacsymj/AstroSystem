/*  iexpand.cc -- Used to expand a filename with an embedded hyphen
 *  into a sequence of filenames
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
#include <string.h>		// strcat()
#include <ctype.h>		// isdigit()
#include <stdlib.h>
#include <libgen.h>
#include <stdio.h>
#include <gendefs.h>

void usage(void) {
  fprintf(stderr, "usage: iexpand %s/dir/imagexxx-yyy.fits\n", IMAGE_DIR);
  exit(-1);
}

int main(int argc, char **argv) {

  if(argc != 2) {
    usage();
  }

  char *string_in = argv[1];

  int in_length = strlen(string_in);

  char *base_string = (char *) malloc(in_length + 5);
  char *filemung_string = (char *) malloc(in_length + 5);

  strcpy(base_string, string_in);
  strcpy(filemung_string, string_in);

  char *root_dir = dirname(base_string);
  char *root_filemung = basename(filemung_string);

  if(root_dir == 0 || root_filemung == 0) usage();

  int root_mung_len = strlen(root_filemung);

  if(root_filemung[0] != 'i' ||
     root_filemung[1] != 'm' ||
     root_filemung[2] != 'a' ||
     root_filemung[3] != 'g' ||
     root_filemung[4] != 'e') usage();

  if(root_filemung[root_mung_len-1] != 's' ||
     root_filemung[root_mung_len-2] != 't' ||
     root_filemung[root_mung_len-3] != 'i' ||
     root_filemung[root_mung_len-4] != 'f' ||
     root_filemung[root_mung_len-5] != '.') usage();

  char *dig1 = root_filemung+5;
  char *dig2;

  char *s = dig1;
  while(*s && isdigit(*s)) s++;
  if(*s) *s = 0;
  else usage();

  s++;
  dig2 = s;
  while(*s && isdigit(*s)) s++;
  if(*s != '.') usage();

  *s = 0;

  int int_dig1, int_dig2;
  sscanf(dig1, "%d", &int_dig1);
  sscanf(dig2, "%d", &int_dig2);

  int num_files = int_dig2 - int_dig1 + 1;
  if(num_files < 0 || num_files > 1000) {
    fprintf(stderr, "iexpand: %d is illogical # of files\n", num_files);
    usage();
  }

  const unsigned int str_len = num_files * (5 + strlen(root_dir) + 14);
  char *answer = (char *) malloc(str_len);
  if(!answer) {
    perror("iexpand: cannot allocate memory for answer");
    exit(-2);
  }

  *answer = 0;

  for(int i = int_dig1; i<= int_dig2; i++) {
    char temp[128];

    sprintf(temp, "%s/image%03d.fits ", root_dir, i);
    strcat(answer, temp);
    if (strlen(answer) >= str_len) {
      fprintf(stderr, "IEXPAND: ERR: answer overflow (%ld vs %d)\n",
	      strlen(answer), str_len);
      exit(-2);
    }
  }

  printf("%s\n", answer);

  return 0;
}
