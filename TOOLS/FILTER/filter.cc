/*  filter.cc -- Program to tell ccd server what filters are installed
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
#include <stdio.h>
#include <unistd.h>		// getopt()
#include <string.h>		// strlen()
#include <Filter.h>
#include <camera_api.h>

#define MAX(a,b) ((a > b) ? a : b)

// One of three conditions exists:
//    1. There are no filters anywhere. Then num_filters == 0.
//    2. There is one permanently-mounted filter. Then num_filters ==
//         1, and we behave as if we had a filter wheel with one
//         position.
//    3. There is a filter wheel, in which case num_filters == 5. Some
//         filter wheel positions may be set to "None".

static void usage(void) {
  fprintf(stderr, "filter -n <no filter present>\n");
  fprintf(stderr, "filter -0 XX <fixed filter, XX=Vc, Ic, etc>\n");
  fprintf(stderr, "filter -1 XX -2 XX -3 XX ... <filter wheel>\n");
  fprintf(stderr, "filter XX <set default filter by name>\n");
  fprintf(stderr, "filter -l <list availble filter names>\n");
  fprintf(stderr, "filter <just print current config>\n");
  exit(-2);
}

void ListDefinedFilters(void) {
  Filter f;
  printf("%s", AllDefinedFilterNames());
  int ret_val = GetDefaultFilter(f);
  if (ret_val) {
    printf("Default filter = %s\n", f.NameOf());
  } else {
    printf("No default filter.\n");
  }
}

int main(int argc, char **argv) {
  int option_char;
  int no_filter_present = 0;
  char filter[9][16] = { "", "", "", "", "", "", "", "", "" };
  int num_wheel_entries = 0;
  int just_query = 0;
  int list_filters = 0;
  int wheel_pos;
  int max_pos_provided = -1;

  while((option_char = getopt(argc, argv, "ln0:1:2:3:4:5:6:7:8:")) > 0) {
    switch (option_char) {
    case 'l':
      list_filters = 1;
      break;

    case 'n':
      no_filter_present = 1;
      break;
      
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
      num_wheel_entries++;
      // no break here -- fall through
    case '0':
      // keep track of the largest position defined
      max_pos_provided = MAX(max_pos_provided, (option_char - '0'));
      wheel_pos = (option_char - '0');
      if(strlen(optarg) > sizeof(filter[wheel_pos])) usage();
      strcpy(filter[wheel_pos], optarg);
      break;

    case '?':			// invalid argument
    default:
      fprintf(stderr, "Invalid argument.\n");
      usage();
      exit(2);
    }
  }

  if (optind < argc && argc == 2) {
    // setting default filter
    Filter f(argv[optind]);
    printf("Setting default filter to %s\n", argv[optind]);
    SetDefaultFilter(&f);
    exit(0);
  } else if (optind < argc) {
    fprintf(stderr, "filter: Extra arguments\n");
    usage();
    exit(2);
  }

  // command-line option "-l" lists defined filters
  if (list_filters) {
    ListDefinedFilters();
    exit(0);
  }

  // "no_filter_present" is set when "-n" encountered
  if(no_filter_present) {
    // illegal to also provide filter entries
    if(num_wheel_entries != 0 ||
       filter[0][0] != 0) usage();
  } else {
    // filter[0] will be set if there is a single, fixed filter
    if(filter[0][0] != 0) {
      if(num_wheel_entries != 0) usage();
    } else {
      // no args were provided; this is just a query
      if(num_wheel_entries == 0) just_query = 1;
    }
  }

  // Possibilities at this point:
  // 1. No filter present (no_filter_present is set)
  // 2. One fixed filter present (num_wheel_entries == 0 && just_query == 0)
  // 3. Five fixed filters present (num_wheel_entries > 0)
  // 4. Just a query, no change to filters (just_query == 1)

  if (!just_query) {

    if(no_filter_present) {
      SetCFWSize(0); // no filter present
    } else {

      // some validity checks
      if (num_wheel_entries > 8 || max_pos_provided > 8) {
	fprintf(stderr, "filter: error: only 8 filter positions available.\n");
	usage();
      }

      int n;
      for(n=0; n<8; n++) {
	if (filter[n+1][0] == 0) {
	  Filter blank("None");
	  SetCFWFilter(n, blank);
	} else {
	  Filter installed(filter[n+1]);
	  SetCFWFilter(n, installed);
	}
      }
    }
    fprintf(stderr, "Copying filter.info to jellybean2\n");
    int r =system("scp /home/ASTRO/CURRENT_DATA/filter.data jellybean2:/home/ASTRO/CURRENT_DATA/filter.data");
    if (r) {
      fprintf(stderr, "Warning: scp might have failed.\n");
    }
  }

  // Now print the current configuration
  
  if(FilterWheelSlots() == 0) {
    printf("No filter installed.\n");
  } else {
    int count = FilterWheelSlots();
    std::vector<Filter> &installed_filters = InstalledFilters();
    
    if(count == 1) {
      printf("Single filter present = %s.\n", installed_filters[0].CanonicalNameOf());
    } else {

      printf("Position  Filter\n");
      for(int n=0; n<count; n++) {
	printf("   %d       %s\n", n+1, installed_filters[n].CanonicalNameOf());
      }
    }
  }
}
