/*  obs_matrix.cc -- Unknown purpose
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
#include <libgen.h>		// for basename()
#include <string.h>
#include <stdio.h>
#include <unistd.h> 		// for getopt()
#include <stdlib.h>		// for atof()
#include <julian.h>
#include <IStarList.h>
#include <fitsio.h>

class DynamicMatrix {
public:
  DynamicMatrix(int NumRows) {
    num_columns = 0;
    num_rows = NumRows;
    matrix_data = 0;
  }
    
  ~DynamicMatrix(void) {
    for(int i = 0; i<num_columns; i++) {
      free(matrix_data[i]);
    }
    free(matrix_data);
  }

  int AddColumn(void) {
    double **new_m = (double ** ) malloc(++num_columns * sizeof(double *));
    int i;
    for(i=0; i< (num_columns-1); i++) {
      new_m[i] = matrix_data[i];
    }
    new_m[num_columns-1] = (double *) malloc(num_rows * sizeof(double));
    for(i=0; i<num_rows; i++) new_m[num_columns-1][i] = 0.0;

    if(matrix_data) {
      free(matrix_data);
    }
    matrix_data = new_m;
    return num_columns-1;
  }
  
  double &Value(int x, int y) { return matrix_data[y][x]; }
  int num_columns;
private:
  double **matrix_data;
  int num_rows;
} *matrix = 0;;

class ColumnNames {
public:
  ColumnNames *next;
  char *Name;
  int column_number;
};
  
class ExposureData {
public:
  // exposure time??
  char filename[32];		// trailing filename
};

ExposureData *ExposureArray;
ColumnNames *head = 0;
//static int NumColumns = 0;

const char *ColumnNumberToColumnName(int ColumnNumber) {
  ColumnNames *c;

  for(c=head; c; c=c->next) {
    if(c->column_number == ColumnNumber) return c->Name;
  }

  return "";
}

int ColumnNameToColumnNumber(const char *ColumnName) {
  ColumnNames *c;

  for(c=head; c; c=c->next) {
    if(strcmp(c->Name, ColumnName) == 0) return c->column_number;
  }
  // not found. Create new column
  c                = new ColumnNames;
  c->next          = head;
  head             = c;
  c->Name          = strdup(ColumnName);
  c->column_number = matrix->AddColumn();

  return c->column_number;
}

int main(int argc, char **argv) {
  int ch;			// option character
  char *output_filename = 0;	// filename of the output file

  // Command line options:
  //
  // -o outputfilename.csv   Imagefile1 Imagefile2 Imagefile3 ...
  //

  while((ch = getopt(argc, argv, "o:")) != -1) {
    switch(ch) {
    case 'o':			// image filename
      output_filename = optarg;
      break;

    case '?':
    default:
      fprintf(stderr, "usage: %s -o output_filename.csv inputfiles... \n",
	      argv[0]);
      return 2;			// error return
    }
  }

  argc -= optind;
  argv += optind;
  
  if(output_filename == 0 || argc < 1) {
    fprintf(stderr, "usage: %s -o output_filename.csv inputfiles... \n",
	    argv[0]);
    return 2;			// error return
  }

  matrix = new DynamicMatrix(argc);	// one row for each file
  int f;
  for(f=0; f<argc; f++) {
    char *input_file = argv[f];
    
    fprintf(stderr, "Processing %s\n", input_file);
    // Pull the list from the file
    IStarList List(input_file);

    // Refine the stars and update in the List.
    int i;
    for(i=0; i < List.NumStars; i++) {
      IStarList::IStarOneStar *oneStar = List.FindByIndex(i);

      int star_column = ColumnNameToColumnNumber(oneStar->StarName);
      if(oneStar->validity_flags & COUNTS_VALID) {
	// matrix->Value(f, star_column) = oneStar->nlls_counts;
	matrix->Value(f, star_column) = oneStar->pixel_sum;
      }
    }
  }

  // now print out the entire matrix
  FILE *output_file = fopen(output_filename, "w");
  fprintf(output_file, "\t");
  int i;
  for(i=0; i<matrix->num_columns; i++) {
    fprintf(output_file, "%s\t", ColumnNumberToColumnName(i));
  }
  fprintf(output_file, "\n");
  
  for(f=0; f<argc; f++) {
    char *input_file = argv[f];

    fprintf(output_file, "%s\t", basename(input_file));
    
    for(i=0; i<matrix->num_columns; i++) {
      if(matrix->Value(f, i) == 0.0) {
	fprintf(output_file, "\t");
      } else {
	fprintf(output_file, "%.0f\t", matrix->Value(f, i));
      }
    }
    fprintf(output_file, "\n");
  }
}
