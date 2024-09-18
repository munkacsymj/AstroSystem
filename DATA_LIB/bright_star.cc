/*  bright_star.cc -- manage a database of bright stars
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
#include <sys/types.h>		// stat(); write();
#include <sys/stat.h>		// stat();
#include <sys/uio.h>		// write();
#include <unistd.h>		// write();
#include <stdio.h>
#include <fcntl.h>		// open();
#include <stdlib.h>		// malloc();
#include <string.h>		// strdup();
#include "bright_star.h"
#include <gendefs.h>

// We maintain both an ascii database and a binary database file. They
// should match. The binary is normally used (for speed).  We watch
// the modification dates of both files, and if the ascii file is
// newer than the binary, the binary will be rebuilt from the ascii
// file. 

static const char *
BrightStarFileNameBin = BRIGHT_STAR_DIR "/BrightStars.bin";
static const char *
BrightStarFileNameAscii = BRIGHT_STAR_DIR "/BrightStars.ascii";

struct BSFformat {
  long          StarCount;
  unsigned long StringHeapOffset; // from beginning of file
};

// Flags:
#define FLAG_DONT_USE 0x01	// don't use this star (binary, etc.)

struct BSFstar {
  long NameOffset;		// from start of StringHeap
  float DecRadians;
  float RARadians;
  float Magnitude;
  unsigned char Flags;
};

static const int BSFstarsize = sizeof(BSFstar);
static const int BSFheadersize = sizeof(BSFformat);

  // Convert the ASCII file into the binary file if the ASCII file is
  // newer. 
static void
ConversionFailed(const char *msg) {
  perror(msg);
  fprintf(stderr, "BrightStar: Unable to create binary file.\n");
}

static void
CreateBrightStarFile(void) {
  struct stat BinStat;
  struct stat AsciiStat;

  if(stat(BrightStarFileNameAscii, &AsciiStat) < 0) {
    ConversionFailed("Cannot find ASCII Bright Star File");
    return;
  }
  if(stat(BrightStarFileNameBin, &BinStat) < 0) {
    goto convert_file;
  }

  if((BinStat.st_mode & S_IFREG) == 0 ||
     BinStat.st_mtime < AsciiStat.st_mtime) goto convert_file;

  // Otherwise, all is well. No conversion needed.
  return;

convert_file:

  fprintf(stderr, "Updating Bright Star binary file. . . ");

  char buffer_in[256];
  int fd_out = open(BrightStarFileNameBin,
		    O_WRONLY | O_CREAT | O_TRUNC,
		    0666);
  FILE *fp_in = fopen(BrightStarFileNameAscii, "r");

  static const int HEAP_INCREMENT_SIZE = 1024;
  char *heap = (char *) malloc(HEAP_INCREMENT_SIZE);
  long heap_offset = 0;
  long heap_allocated_so_far = HEAP_INCREMENT_SIZE;
  int stars_written = 0;

  // write a placeholder at the beginning.  Will come back and rewrite
  // it at the very end (when we know how many stars are present).
  BSFformat header;
  header.StarCount = 0;
  header.StringHeapOffset = 0;
  if(write(fd_out, &header, BSFheadersize) != BSFheadersize) {
    ConversionFailed("Error writing brightstar file header:");
    return;
  }

  while(fgets(buffer_in, sizeof(buffer_in), fp_in)) {
    // First 16 char of buffer_in hold the star name, if any.
    long this_name_offset;
    if(buffer_in[0] == '\n') break;
    if(buffer_in[0] != ' ') {
      // Yes, there is a name!
      char *p = buffer_in+16;
      while(*p == ' ') p--;
      // Now "p" points to last significant char.
      p++;
      *p = 0; // terminate the string
      if(p - buffer_in >= (heap_allocated_so_far - heap_offset)) {
	// Need more memory for the string heap
	heap_allocated_so_far += HEAP_INCREMENT_SIZE;
	heap = (char *) realloc(heap, heap_allocated_so_far);
      }
      this_name_offset = heap_offset;
      p = heap+heap_offset;
      char *s = buffer_in;
      do {
	*p++ = *s;
      } while(*s++);
      heap_offset = (p - heap);
    } else {
      this_name_offset = -1;
    }

    int num_converted;
    int hours;
    int degrees;
    double minutes_ra;
    double minutes_dec;
    float mag;
    int flags;

    num_converted = sscanf(buffer_in+17, "%d %lf %d %lf %*d %f %d",
			   &hours,
			   &minutes_ra,
			   &degrees,
			   &minutes_dec,
			   &mag,
			   &flags);
    if(num_converted == 5) flags = 0;
    if(num_converted == 0 || num_converted == EOF) break;
    if(num_converted < 5) {
      fprintf(stderr, "Bright Star ASCII file bad conversion: %s\n",
	      buffer_in);
    }

    BSFstar this_star;
    this_star.NameOffset = this_name_offset;
    this_star.RARadians = (hours + minutes_ra/60.0) * (M_PI/12.0);
    this_star.Magnitude = mag;
    this_star.Flags = flags;
    // Dec conversion tricky because of sign. Following are possible:
    //     10 58.61666
    //     -10 58.61666
    //     0 58.51555
    //     0 -58.51555
    if (degrees < 0) {
      this_star.DecRadians = (degrees * 60 - minutes_dec) * (M_PI/(60*180.0));
    } else {
      this_star.DecRadians = (degrees * 60 + minutes_dec) * (M_PI/(60*180.0));
    }

    if(write(fd_out, &this_star, BSFstarsize) != BSFstarsize) {
      ConversionFailed("Error writing to brightstar file:");
      return;
    }
    stars_written++;
  }
      
  // finished writing stars.  Write the correct header.
  header.StarCount = stars_written;
  header.StringHeapOffset = BSFheadersize + stars_written * BSFstarsize;
  lseek(fd_out, 0, SEEK_SET);
  if(write(fd_out, &header, BSFheadersize) != BSFheadersize) {
    ConversionFailed("Error re-writing brightstar file header:");
    return;
  }
  lseek(fd_out, header.StringHeapOffset, SEEK_SET);
  if(write(fd_out, heap, heap_offset) != heap_offset) {
    ConversionFailed("Error writing brightstar file heap:");
    return;
  }
  close(fd_out);
  fclose(fp_in);

  fprintf(stderr, " done (%d stars).\n", stars_written);
}

BrightStarList::BrightStarList(double max_dec,        /* north-est */
			       double min_dec,        /* south-est */
			       double east_ra,        /* biggest */
			       double west_ra,        /* smallest */
			       double max_magnitude,  /* dimmest */
			       double min_magnitude) {	/* brightest */
  head = 0;
  // Convert the ASCII file into the binary file if the ASCII file is
  // newer. 
  CreateBrightStarFile();

  int fd = open(BrightStarFileNameBin, O_RDONLY, 0);
  if(fd < 0) {
    perror("Cannot open BrightStar binary file");
    return;
  }

  BSFformat header;
  if(read(fd, &header, BSFheadersize) != BSFheadersize) {
    perror("Cannot read header of Bright Star binary file");
    return;
  }

  for(int star_counter=0; star_counter < header.StarCount; star_counter++) {
    BSFstar this_star;

    if(read(fd, &this_star, BSFstarsize) != BSFstarsize) {
      perror("Cannot read a star from Bright Star binary file");
      return;
    }

    if(this_star.Magnitude <= max_magnitude &&
       this_star.Magnitude >= min_magnitude &&
       this_star.DecRadians <= max_dec &&
       this_star.DecRadians >= min_dec &&
       (this_star.Flags & FLAG_DONT_USE) == 0) {
      // so far, so good.  Now check RA.
      // Do we wrap around 00h ??
      int good_star;
      if(east_ra > west_ra) {
	// no wrap: easy case
	good_star = (this_star.RARadians <= east_ra &&
		     this_star.RARadians >= west_ra);
      } else {
	// wrap case. Careful.
	good_star = (this_star.RARadians >= west_ra ||
		     this_star.RARadians <= east_ra);
      }
      if(good_star) {
	OneBrightStar *new_one = new OneBrightStar;
	new_one->OBSLocation = DEC_RA(this_star.DecRadians,
				      this_star.RARadians);
	new_one->OBSMagnitude = this_star.Magnitude;

	// Now fetch the name
	if(this_star.NameOffset == -1) {
	  // no name (anonymous?)
	  new_one->OBSName = 0;
	} else {
	  // seek into the namespace at the end of the file, get the
	  // name, then seek back to where we started.
	  off_t OriginalPosition = lseek(fd, 0, SEEK_CUR);
	  lseek(fd, header.StringHeapOffset + this_star.NameOffset, SEEK_SET);
	  char buffer[80];
	  if(read(fd, buffer, sizeof(buffer)) != sizeof(buffer)) {
	    perror("Unable to read from newly-created BrightStarFile");
	    return;
	  }
	  new_one->OBSName = strdup(buffer);
	  lseek(fd, OriginalPosition, SEEK_SET);
	}

	new_one->next = head;
	head = new_one;
      }
    }
  }
  close(fd);
}

BrightStarList::~BrightStarList(void) {
  OneBrightStar *one_star;
  OneBrightStar *next_star;

  for(one_star = head; one_star; one_star = next_star) {
    if(one_star->OBSName) free(one_star->OBSName);

    next_star = one_star->next;
    delete one_star;
  }
}

int
BrightStarList::NumberOfStars(void) {
  OneBrightStar *one_star;
  int count=0;

  for(one_star = head; one_star; one_star = one_star->next) {
    count++;
  }
  return count;
}
