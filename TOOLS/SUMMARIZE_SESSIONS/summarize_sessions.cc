/*  summarize_sessions.cc -- creates a report, one line per day, that
 *  indicates what photometry reporting steps have been completed for
 *  each day
 *
 *  Copyright (C) 2017 Mark J. Munkacsy

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
#include <unistd.h>		// unlink()
#include <stdlib.h>		// system(), mkstemp()
#include <string.h>		// strdup, strtok()
#include <ctype.h>		// toupper, isdigit, isspace
#include <sys/types.h>		// (DIR *)
#include <sys/stat.h>		// struct stat
#include <dirent.h>		// opendir(), ...
#include <assert.h>
#include <stdio.h>
#include <list>			// std::list
#include <obs_record.h>

//****************************************************************
//        Class OneDay
//****************************************************************
class OneDay {
public:
  char *dir_path;
  char *folder_shortname;
  int year;
  int month;
  int day;

  bool has_aavso_csv_file;
  bool has_bvri_db_file;
  bool has_aavso_report_file;
  bool photometry_imported;
  bool has_aavso_sent_file;

  OneDay(void);
  ~OneDay(void);
};

OneDay::OneDay(void) {
  has_aavso_csv_file =
    has_bvri_db_file =
    has_aavso_sent_file =
    photometry_imported =
    has_aavso_report_file = false;
}

OneDay::~OneDay(void) {
  free(dir_path);
  free(folder_shortname);
}

bool compareDay(const OneDay *a, const OneDay *b) {
  if (a->year != b->year) {
    return a->year > b->year;
  }
  if (a->month != b->month) {
    return a->month > b->month;
  }
  return a->day > b->day;
}

//****************************************************************

ObsRecord *all_observations = 0;

typedef std::list<OneDay *> DayList;

DayList all_days;

const static char *image_directory = "/home/IMAGES";

void CheckObservations(const char *aavso_filename, OneDay *d) {
  // if at least one entry in the aavso.csv file has had photometry
  // data entered into the observations database, then this check
  // succeeds, and we set the photometry_imported flag in d.
  FILE *fp = fopen(aavso_filename, "r");
  if (!fp) return;
  char buffer[132];
  int lookups_found = 0;
  int photometry_found = 0;

  while(fgets(buffer, sizeof(buffer), fp)) {
    // use strtok to split up the .csv file line; we want to grab the
    // first four words (starname, designation, filenumbers, julian day)
    char *tok_p = buffer;
    const char *delim = ",";
    char *words[8];

    for (int i=0; i<4; i++) {
      words[i] = strtok(tok_p, delim);
      tok_p = 0; // ...because this is how strtok() works.
    }

    // need to have at least starname and julian day
    if (words[0] == 0 || words[3] == 0) continue;

    // do a validity check on the julian day
    const double julian_double = atof(words[3]);
    if (julian_double < 2400000.0 || julian_double > 4400000.0) {
      fprintf(stderr, "summarize_sessions: can't grok JD = %s\n",
	      words[3]);
      continue;
    }

    // find the observation that matches in the "observations.txt" file
    ObsRecord::Observation *obs =
      all_observations->FindObservation(words[0], JULIAN(julian_double));
    if (!obs) {
      //fprintf(stderr, "summarize_sessions: can't find obs for: %s,...,%s\n",
      //      words[0], words[3]);
      continue;
    }

    // This counts as a successful lookup!
    lookups_found++;
    // Is photometry available?
    if (isnormal(obs->V_mag) ||
	isnormal(obs->B_mag) ||
	isnormal(obs->R_mag) ||
	isnormal(obs->I_mag)) photometry_found++;
  }

  fclose(fp);

  d->photometry_imported = (photometry_found > 0);
  fprintf(stderr, "%s: %d of %d photometry found.\n",
	  d->folder_shortname, photometry_found, lookups_found);
}
  

void GetData(OneDay *d, ObsRecord *all_obs) {
  char *temp_filename = (char *) malloc(strlen(d->dir_path) + 255);
  struct stat f_stat;
  if (!temp_filename) {
    fprintf(stderr, "summarize_sessions: error allocating string memory.\n");
    return;
  }

  // Is there an aavso.csv file??
  sprintf(temp_filename, "%s/aavso.csv", d->dir_path);
  if (stat(temp_filename, &f_stat)) {
    // error occured during stat()
    d->has_aavso_csv_file = false;
    return;
  } else {
    d->has_aavso_csv_file = true;
    CheckObservations(temp_filename, d);
  }

  // Is there a bvri.db file??
  sprintf(temp_filename, "%s/bvri.db", d->dir_path);
  if (stat(temp_filename, &f_stat)) {
    // error occured during stat()
    d->has_bvri_db_file = false;
    return;
  } else {
    d->has_bvri_db_file = true;
  }
  
  // Is there an aavso.report file??
  sprintf(temp_filename, "%s/aavso.report", d->dir_path);
  if (stat(temp_filename, &f_stat)) {
    // error occured during stat()
    // check alternate filename
    sprintf(temp_filename, "%s/aavso.report.txt", d->dir_path);
    if (stat(temp_filename, &f_stat)) {
      d->has_aavso_report_file = false;
      return;
    }
  }
  d->has_aavso_report_file = true;

  // Is there an aavso.sent file??
  sprintf(temp_filename, "%s/aavso.sent", d->dir_path);
  if (stat(temp_filename, &f_stat)) {
    // error occured during stat()
    d->has_aavso_sent_file = false;
    fprintf(stderr, "%s: sent false\n", temp_filename);
    return;
  } else {
    d->has_aavso_sent_file = true;
    fprintf(stderr, "%s: sent true\n", temp_filename);
  }
}
  

void InitDayList(ObsRecord *all_obs) {

  DIR *image_dir = opendir(image_directory);
  if(!image_dir) {
    fprintf(stderr, "summarize_sessions: cannot opendir() in %s\n",
	    image_directory);
    return;
  }

  struct dirent *dp;
  while((dp = readdir(image_dir)) != NULL) {
    // Build a string that holds the full pathname
    const int pathlen = strlen(dp->d_name) + strlen(image_directory) + 5;
    char *full_path = (char *) malloc(pathlen);
    if (!full_path) {
      perror("summarize_sessions: unable to malloc filename space:");
      return;
    }
    sprintf(full_path, "%s/%s", image_directory, dp->d_name);

    // Now see if that full pathname points to a directory
    struct stat dir_stat;
    if (stat(full_path, &dir_stat)) {
      // error occured during stat()
      perror("summarize_sessions: stat() failed:");
      continue; // go to next folder in directory
    }
    // is it a directory?
    if ((dir_stat.st_mode & S_IFDIR) == 0) continue; // no: skip entry
    
    // is it a valid "date" directory name?
    int d_year;
    int d_month;
    int d_day;
    int num_converted;

    num_converted = sscanf(dp->d_name, "%u-%u-%u", &d_month, &d_day, &d_year);
    if (num_converted != 3) continue;
    
    // got one!

    OneDay *new_day = new OneDay;
    
    if(!new_day) {
      fprintf(stderr, "Unknown error reading directory: %s\n",
	      full_path);
    } else {
      new_day->dir_path = full_path;
      new_day->folder_shortname = strdup(dp->d_name);
      new_day->year = d_year;
      new_day->month = d_month;
      new_day->day = d_day;
      all_days.push_back(new_day);
    }
  }
  closedir(image_dir);

  // now sort the list into the order desired for display (most recent
  // entries are first)
  all_days.sort(compareDay);

  DayList::iterator it;
  for (it=all_days.begin(); it != all_days.end(); it++) {
    OneDay *d = (*it);
    GetData(d, all_obs);
  }
}

void usage(void) {
  fprintf(stderr, "Usage: summarize_sessions -o filename.txt\n");
  exit(-1);
}

void PrintSummary(FILE *fp) {
  fprintf(fp, "Date        Obs Analyzed Report Imported Uploaded\n");

  DayList::iterator it;
  for (it = all_days.begin(); it != all_days.end(); it++) {
    OneDay *d = (*it);

    fprintf(fp, "%-12s  %c     %c       %c       %c      %c\n",
	    d->folder_shortname,
	    (d->has_aavso_csv_file ? 'X' : ' '),
	    (d->has_bvri_db_file ? 'X' : ' '),
	    (d->has_aavso_report_file ? 'X' : ' '),
	    (d->photometry_imported ? 'X' : ' '),
	    (d->has_aavso_sent_file ? 'X' : ' '));
  }
}
	  

//****************************************************************
//        main()
//****************************************************************
int main(int argc, char **argv) {
  int option_char;
  char *output_filename = 0;

  while((option_char = getopt(argc, argv, "o:")) > 0) {
    switch (option_char) {
    case 'o':
      output_filename = optarg;
      break;

    case '?':			// invalid argument
    default:
      fprintf(stderr, "Invalid argument.\n");
      usage();
    }
  }

  FILE *fp_out = fopen(output_filename, "w");
  if (!fp_out) {
    fprintf(stderr, "summarize_sessions: cannot open output file: %s\n",
	    output_filename);
    perror("Cannot create:");
    usage();
    /*NOTREACHED*/
  }
  
  all_observations = new ObsRecord;
  
  InitDayList(all_observations);

  PrintSummary(fp_out);

  if(fclose(fp_out)) {
    fprintf(stderr,
	    "summarize_sessions: ERROR: Cannot close output filename %s\n",
	    output_filename);
    perror("Cannot close file after writing:");
    exit(-2);
    /*NOTREACHED*/
  }
  
  return 0;
}
