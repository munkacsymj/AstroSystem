/*  smart_add_archive.cc -- Searches Image directory for photometry
 *  files and adds data to the observation archive 
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
#include <string.h>		// for strdup()
#include <ctype.h>		// for isupper()
#include <unistd.h> 		// for getopt(), getpid()
#include <stdlib.h>		// for malloc(), system()
#include <dirent.h>		// for getdents()
#include <stdio.h>
#include <fcntl.h>		// for open()
#include <sys/types.h>
#include <sys/stat.h>

void usage(void) {
      fprintf(stderr,
	      "usage: smart_add_archive -h home_dir\n");
      exit(-2);
}

struct obs_summary_entry {
  char *starname;		// in standard format
  double julian_obs_time;
  obs_summary_entry *next;
} *first_obs_summary = 0;
  

/****************************************************************/
/*        FORWARD DECLARATIONS                                  */
/****************************************************************/
int ends_with(const char *pattern, const char *field);
void ProcessCSV(char *home_directory, char *filename);
void AddInfo(char *name, char *julian_day);
void AddToArchive(obs_summary_entry *obs, char *filename);

/****************************************************************/
/* main()						        */
/****************************************************************/
int main(int argc, char **argv) {
  int ch;			// option character
  char *home_directory = 0;

  // Command line options:
  // -h home_directory

  while((ch = getopt(argc, argv, "h:")) != -1) {
    switch(ch) {
    case 'h':			// image filename
      home_directory = optarg;
      break;

    case '?':
    default:
      usage();
   }
  }

  
  // must specify a home directory
  if(!home_directory) usage();

  // open the home directory.  Then scan it for *.csv files
  DIR *dir = opendir(home_directory); // POSIX
  // int dir_fd = open(home_directory, O_RDONLY); // NetBSD
  if(!dir) {
    fprintf(stderr, "Cannot open directory %s\n", home_directory);
    usage();
  }

  {
    struct dirent *d;

    while((d = readdir(dir)) != 0) {
	//	if(dir->d_type == DT_REG &&
	if(ends_with(".csv", d->d_name)) {
	  // Found one!!
	  fprintf(stderr, "Extracting observations from %s\n",
		  d->d_name);
	  ProcessCSV(home_directory, d->d_name);
	}
    }
  }
  closedir(dir);

  // Now we've built the full list of stars to look for in the
  // obs_summary_entry structures.
  obs_summary_entry *obs;

  // Loop through all stars observed, and look for a matching
  // "photometry" file with the filename of star.phot or star-b.phot
  // or star-a.phot.
  for(obs = first_obs_summary; obs; obs=obs->next) {
    char filename[128];

    sprintf(filename, "%s/%s-b.phot", home_directory, obs->starname);
    struct stat sb;
    if(stat(filename, &sb) == 0) {
      AddToArchive(obs, filename);
    } else {
      sprintf(filename, "%s/%s-a.phot", home_directory, obs->starname);
      if(stat(filename, &sb) == 0) {
	AddToArchive(obs, filename);
      } else {
	sprintf(filename, "%s/%s.phot", home_directory, obs->starname);
	if(stat(filename, &sb) == 0) {
	  AddToArchive(obs, filename);
	} else {
	  fprintf(stderr, "Couldn't find a photometry file: %s\n",
		  filename);
	}
      }
    }
  }
}
	

/****************************************************************/
/*    ends_with()
      Used to check to see if a filename ends with a given ending
      pattern. 
      */
/****************************************************************/
int ends_with(const char *pattern, const char *field) {
  const int len_field = strlen(field);
  const int len_pattern = strlen(pattern);

  // return 1 if there is a match
  return (strcmp(pattern, field + len_field - len_pattern) == 0);
}

void ProcessCSV(char *home_directory, char *filename) {

  char *full_pathname = (char *) malloc(5 + strlen(home_directory) +
					strlen(filename));
  if(!full_pathname) {
    fprintf(stderr, "Cannot malloc() for ProcessCSV()\n");
    return;
  }

  sprintf(full_pathname, "%s/%s", home_directory, filename);

  FILE *fp = fopen(full_pathname, "r");

  if(!fp) {
    fprintf(stderr, "Cannot open .csv file named %s\n", full_pathname);
    return;
  }

  char buffer[128];
  char field1[64];
  char field4[64];

  while(fgets(buffer, sizeof(buffer), fp)) {
    // Extract two fields: the first is everything up to the first
    // comma, the second is everything after the third comma.
    char *s = buffer;
    char *d = field1;

    while(*s && *s != ',') *d++ = *s++;

    *d = 0;			// terminate field1
    // verify ended with a comma
    if(*s != ',') continue;

    s++; // first char in field 2
    while(*s && *s != ',') s++;
    // verify field 2 ended with a comma
    if(*s != ',') continue;

    s++; // first char in field 3
    while(*s && *s != ',') s++;
    // verify field 3 ended with a comma
    if(*s != ',') continue;

    s++; // first char in field 4
    d = field4;
    while(*s && *s != ',' && *s != '\n') *d++ = *s++;
    *d = 0;			// terminate the field.

    // now put field1 into standard form: all lower case, with spaces
    // turned into hyphens
    for(s = field1; *s; s++) {
      if(isupper(*s)) *s = tolower(*s);
      if(*s == ' ') *s = '-';
    }

    AddInfo(field1, field4);
  }

  fclose(fp);
}

/****************************************************************/
/*    AddInfo(char *star_name, char *observation_time)          */
/****************************************************************/

void AddInfo(char *name, char *julian_day) {
  double julian_day_float = atof(julian_day);

  struct obs_summary_entry *new_entry =
    (obs_summary_entry *) malloc(sizeof(obs_summary_entry));

  new_entry->starname = strdup(name);
  new_entry->julian_obs_time = julian_day_float;
  new_entry->next = first_obs_summary;

  first_obs_summary = new_entry;

  fprintf(stderr, "Found entry for %s\n", name);
}

/****************************************************************/
/*        AddToArchive(obs, filename)                           */
/****************************************************************/

void AddToArchive(obs_summary_entry *obs, char *filename) {
  fprintf(stderr, "Adding %s at %.1f using %s\n",
	  obs->starname, obs->julian_obs_time, filename);

  char buffer[128];

  sprintf(buffer, "add_archive -f %s -t %.1f",
	  filename, obs->julian_obs_time);
  if(system(buffer) < 0) {
    fprintf(stderr, "Error: Cannot execute add_archive\n");
  }
}
