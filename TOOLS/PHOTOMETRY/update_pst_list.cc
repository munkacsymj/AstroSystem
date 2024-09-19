/*  update_pst_list.cc -- edit existing pst file by looking at results of nstar
 *
 *  Copyright (C) 2007, 2018, 2019 Mark J. Munkacsy
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
#include <sys/types.h>		// for getpid()
#include <stdio.h>
#include <unistd.h> 		// for getopt(), getpid()
#include <stdlib.h>		// for atof()
#include <gendefs.h>
#include <ctype.h>
#include <list>
#include <math.h>

//#define DELETE_TEMPS

void usage(void) {
      fprintf(stderr,
	      "usage: update_pst_list -t pstfile -p nstarfile -o messages.txt\n");
      exit(-2);
}

struct PSTStar {
  unsigned int id;
  double x_center;
  double y_center;
  double mag;
  double msky;
};

class PSTFile {
public:
  PSTFile(const char *filename);
  ~PSTFile(void);
  void ReWrite(void);
  std::list<PSTStar *> *GetStars(void);
  void MarkForDeletion(PSTStar *star_to_delete);

private:
  std::list<PSTStar *> all_stars;
  std::list<char *> header_lines;
  std::list<PSTStar *> stars_to_delete;
  const char *file_name;
};

struct NStar {
  unsigned int id;
  double x_center;
  double y_center;
  double mag;
  double merr;
  double msky;
  unsigned int niter;
  double sharpness;
  double chi;
  int pier;
  char *errors;
};

class NSTARFile {
public:
  NSTARFile(const char *filename);
  ~NSTARFile(void);
  std::list<NStar *> *GetStars(void);

private:
  std::list<NStar *> all_stars;
};

int main(int argc, char **argv) {
  int ch;			// option character
  char *pst_filename = 0;
  char *nstar_filename = 0;
  char *messages_filename = 0;

  // Command line options:
  // -t pstfile          From prior run of pstselect
  // -p nstarfile        From prior run of nstar
  // -o messages         New file with summary

  while((ch = getopt(argc, argv, "t:p:o:")) != -1) {
    switch(ch) {
    case 't':
      pst_filename = optarg;
      break;

    case 'p':
      nstar_filename = optarg;
      break;

    case 'o':
      messages_filename = optarg;
      break;

    case '?':
    default:
      usage();
   }
  }

  //  filename arguments are required
  if(pst_filename == 0 ||
     nstar_filename == 0 ||
     messages_filename == 0) {
    usage();
  }

  PSTFile p_file(pst_filename);
  NSTARFile n_file(nstar_filename);

  // simple. Look through the n_file for any entry that has a chi
  // value bigger than 2.25. Those stars get deleted from p_file.
  bool modified = false;
  int num_deleted = 0;
  std::list<NStar *>::iterator it;
  std::list<NStar *> *l = n_file.GetStars();
  for (it = l->begin(); it != l->end(); it++) {
    NStar *s = (*it);
    if (s->chi > 2.25) {
      // candidate for deletion if it's in the p_file
      std::list<PSTStar *> *t = p_file.GetStars();
      std::list<PSTStar *>::iterator it_p;
      for (it_p = t->begin(); it_p != t->end(); it_p++) {
	if ((*it_p)->id == s->id) {
	  // yes: mark for deletion
	  p_file.MarkForDeletion(*it_p);
	  num_deleted++;
	  modified = true;
	  fprintf(stderr, "Deleting star %d with chi = %.3lf\n",
		  s->id, s->chi);
	  break;
	}
      }
    }
  }

  FILE *fp = fopen(messages_filename, "w");
  if (!fp) {
    fprintf(stderr, "Cannot create messages file: %s\n", messages_filename);
  }
  if(modified) {
    p_file.ReWrite();
    fprintf(fp, "MODIFIED\n");
  } else {
    fprintf(fp, "OKAY\n");
  }

  double sum_err_sq = 0.0;
  double sum_chi = 0.0;
  int num_sums = 0;
  
  for (it = l->begin(); it != l->end(); it++) {
    NStar *s = (*it);

    sum_err_sq += (s->merr*s->merr);
    sum_chi += s->chi;
    num_sums++;
  }
  fprintf(fp, "Deleted %d stars (of %d). Avg chi = %.3lf, RMS err = %.3lf\n",
	  num_deleted, num_sums,
	  sum_chi/num_sums, sqrt(sum_err_sq/num_sums));

  fclose(fp);
  return(0);
}

//****************************************************************
//        NSTARfile
//****************************************************************

NSTARFile::NSTARFile(const char *filename) {
  FILE *fp = fopen(filename, "r");
  char buffer[132];
  NStar *this_star = 0;

  while(fgets(buffer, sizeof(buffer), fp)) {
    // classify the line using the first character
    if (buffer[0] == '#') {
      // comment line: ignore it
      continue;
    }
    if (isdigit(buffer[0])) {
      // first line of star entry
      if (this_star) {
	fprintf(stderr, "NSTARFile: illogical line: %s\n", buffer);
      } else {
	this_star = new NStar;
	int group;
	int num_scan = sscanf(buffer, "%d %d %lf %lf %lf %lf %lf",
			      &this_star->id, &group,
			      &this_star->x_center,
			      &this_star->y_center,
			      &this_star->mag,
			      &this_star->merr,
			      &this_star->msky);
	if (num_scan != 7) {
	  fprintf(stderr, "NSTARFile: bad line 1: %s\n", buffer);
	}
      }
    }
    if (buffer[0] == ' ') {
      // second line of entry
      if (this_star == 0) {
	fprintf(stderr, "NSTARFile: illogical line 2: %s\n", buffer);
      } else {
	char error_string[132];
	int num_scan = sscanf(buffer, "%d %lf %lf %d %s",
			      &this_star->niter,
			      &this_star->sharpness,
			      &this_star->chi,
			      &this_star->pier,
			      error_string);
	if (num_scan == 5) {
	  this_star->errors = strdup(error_string);
	  all_stars.push_back(this_star);
	  this_star = 0;
	} else {
	  fprintf(stderr, "NSTARFile: bad line 2: %s\n", buffer);
	  this_star = 0;
	}
      }
    }
  } // end fgets() entire file
}

NSTARFile::~NSTARFile(void) {
  std::list<NStar *>::iterator it;
  for (it = all_stars.begin(); it != all_stars.end(); it++) {
    delete (*it);
  }
  all_stars.clear();
}

std::list<NStar *> *
NSTARFile::GetStars(void) {
  return &all_stars;
}

//****************************************************************
//        PSTFile
//****************************************************************
PSTFile::PSTFile(const char *filename) {
  file_name = filename;
  FILE *fp = fopen(filename, "r");
  if (!fp) {
    fprintf(stderr, "Cannot read PSTfile: %s\n", filename);
    return;
  }

  char buffer[132];
  while(fgets(buffer, sizeof(buffer), fp)) {
    // Tell what kind of line from the first character
    if (buffer[0] == '#') {
      // comment header, capture the text for writing later
      char *this_line = strdup(buffer);
      header_lines.push_back(this_line);
    } else if (isdigit(buffer[0])) {
      // star line. Read it.
      PSTStar *p = new PSTStar;
      int num_fields = sscanf(buffer, "%d %lf %lf %lf %lf",
			      &p->id, &p->x_center, &p->y_center,
			      &p->mag, &p->msky);
      if (num_fields != 5) {
	fprintf(stderr, "PSTFile: error parsing: %s\n", buffer);
      } else {
	all_stars.push_back(p);
      }
    } else {
      fprintf(stderr, "PSTFile: illegal line type: %s\n", buffer);
    }
  } // end loop over all lines in the file.
  fclose(fp);
}

PSTFile::~PSTFile(void) {
  std::list<PSTStar *>::iterator it_p;
  for (it_p = all_stars.begin(); it_p != all_stars.end(); it_p++) {
    delete (*it_p);
  }
  all_stars.clear();

  std::list<char *>::iterator it_c;
  for (it_c = header_lines.begin(); it_c != header_lines.end(); it_c++) {
    free(*it_c);
  }
  header_lines.clear();

  stars_to_delete.clear();
}

void
PSTFile::ReWrite(void) {
  FILE *fp = fopen(file_name, "w");
  if (!fp) {
    perror("PSTFile::ReWrite(): Cannot rewrite file:");
    return;
  }
  // write all the comment lines
  std::list<char *>::iterator it_c;
  for (it_c = header_lines.begin(); it_c != header_lines.end(); it_c++) {
    fprintf(fp, "%s", (*it_c)); // string already contains \n char at end
  }
  // then write all the star lines
  std::list<PSTStar *>::iterator it;
  for (it = all_stars.begin(); it != all_stars.end(); it++) {
    PSTStar *s = (*it);
    bool marked_for_delete = false;
    
    std::list<PSTStar *>::iterator it_x;
    for (it_x = stars_to_delete.begin(); it_x != stars_to_delete.end(); it_x++) {
      if (s == (*it_x)) {
	marked_for_delete = true;
	break;
      }
    }

    if (!marked_for_delete) {
      fprintf(fp, "%-9d%-10.3lf%-10.3lf%-12.3lf%-15.7lg\n",
	      s->id, s->x_center, s->y_center, s->mag, s->msky);
    }
  }
  fclose(fp);
}

std::list<PSTStar *> *
PSTFile::GetStars(void) {
  return &all_stars;
}

void
PSTFile::MarkForDeletion(PSTStar *star_to_delete) {
  stars_to_delete.push_back(star_to_delete);
}

  
