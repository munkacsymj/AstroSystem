/*  fetch_photometry.cc -- Grab AAVSO photometry and compare/merge
 *  with catalog file
 *
 *  Copyright (C) 2015 Mark J. Munkacsy

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
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>		// strdup()
#include <unistd.h>		// getopt()
#include <curl/curl.h>
#include "strategy.h"
#include "HGSC.h"
#include "dec_ra.h"
#include "gendefs.h"
#include "compare_photometry.h"

// fetch_one_star() will return TRUE if the fetched photometry matches
// what's currently in the catalog file. It will return FALSE if there
// are any differences.
bool fetch_one_star(const char *starname,
		    int update_flag,
		    bool force_ID_update,
		    bool distrust,
		    const char *chartID,
		    int standard_field_flag);

void update_chart_ID(const char *chart_id,
		     const char *starname) {
  char command[512];
  sprintf(command, "/home/mark/ASTRO/CURRENT/TOOLS/FETCH_PHOTOMETRY/update_chartid.py -s /home/ASTRO/STRATEGIES/%s.str -c %s",
	  starname, chart_id);
  if(system(command)) {
    fprintf(stderr, "update_chartid.py exited with error code.\n");
  }
}

const char *cleanup_name(const char *orig) {
  char *buf = (char *) malloc(strlen(orig)*3);
  char *p = buf;

  if (!buf) {
    fprintf(stderr, "out of memory: cleanup_name: malloc() failed.\n");
    return 0;
  }
  while (*orig) {
    if (*orig == ' ' || *orig == '/') {
      int ch = *orig;
      *p++ = '%';
      *p++ = (ch/16) + '0';
      *p++ = (ch % 16) + '0';
      orig++;
    } else {
      *p++ = *orig++;
    }
  }
  *p = 0;
  return buf;
}

struct MemoryStruct {
  char *memory;
  size_t size;
};
 
 
static size_t
WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
  size_t realsize = size * nmemb;
  struct MemoryStruct *mem = (struct MemoryStruct *)userp;
 
  mem->memory = (char *) realloc(mem->memory, mem->size + realsize + 1);
  if(mem->memory == NULL) {
    /* out of memory! */ 
    printf("not enough memory (realloc returned NULL)\n");
    return 0;
  }
 
  memcpy(&(mem->memory[mem->size]), contents, realsize);
  mem->size += realsize;
  mem->memory[mem->size] = 0;
 
  return realsize;
}
 

void usage(void) {
  fprintf(stderr, "usage: fetch_photometry [-f] [-u] [-s] -a [-d] | -n starname [-c chartID]\n");
  fprintf(stderr, "-f to forcibly replace the current ChartID in the strategy file\n");
  fprintf(stderr, "-u actually perform an update to the catalog file\n");
  fprintf(stderr, "-s fetch 'standard' sequence values\n");
  fprintf(stderr, "-a fetch for all catalog files\n");
  fprintf(stderr, "-d distrust existing Chart IDs in strategy file(s).\n");
  fprintf(stderr, "-n starname to be fetched\n");
  fprintf(stderr, "-c chartID to be specifically fetched instead of current sequence\n");
  exit(2);
}

int main (int argc, char **argv) {
  int ch;			// option character
  int update_flag = 0;		// performs an update of the catalog
  int standard_field_flag = 0;
  bool fetch_all = false;
  bool distrust = false;
  bool force_ID_update = false;
  const char *starname = 0;
  const char *chartID = 0;

  // Command line options:
  // -u 		Perform an update of the catalog file
  // -n starname        Local name of the star (e.g., "rr-boo")
  // -c chartID         Chart ID (e.g., "14872HWM")
  // -s                 Fetch "standards" photometry instead of
  //                    orginary stuff

  while((ch = getopt(argc, argv, "fadsun:c:")) != -1) {
    switch(ch) {
    case 'a':
      fetch_all = true;
      break;

    case 'd':
      distrust = true;
      break;

    case 'f':
      force_ID_update = true;
      break;

    case 's':
      standard_field_flag = 1;
      break;
      
    case 'u':
      update_flag = 1;
      break;

    case 'n':
      if (strlen(optarg) > 32) {
	fprintf(stderr, "starname too long\n");
	usage();
      }
      starname = strdup(optarg);
      break;

    case 'c':
      if (strlen(optarg) > 32) {
	fprintf(stderr, "chartname too long\n");
	usage();
      }
      chartID = strdup(optarg);
      break;

    case '?':
    default:
      usage();
      /*NOTREACHED*/
    }
  }

  if (fetch_all == false && (starname == 0 || *starname == 0)) usage();
  if (fetch_all && starname) usage();

  if (!fetch_all) {
    fetch_one_star(starname, update_flag, force_ID_update, distrust, chartID, standard_field_flag);
  } else {
    // We are fetching photometry for all files. Go through the
    // strategy directory one file at a time and pick out all the
    // *.str files.
    static const char *strategy_directory = STRATEGY_DIR;
    DIR *strat_dir = opendir(strategy_directory);
    if(!strat_dir) {
      fprintf(stderr, "strategy: FindAllStrategies: cannot opendir() in %s\n",
	      strategy_directory);
      usage();
      /*NOTREACHED*/
    }

    struct dirent *dp;
    while((dp = readdir(strat_dir)) != NULL) {
      int len = strlen(dp->d_name);
      if(len >= 4 &&
	 (strcmp(dp->d_name + (len - 4), ".str") == 0)) {
	// got one!
	char strategy_name[64];
	strcpy(strategy_name, dp->d_name);
	strategy_name[len-4] = 0;

	Strategy *new_strategy = new Strategy(strategy_name, 0);

	if (new_strategy->AutoUpdatePhotometry()) {
	  fetch_one_star(new_strategy->object(),
			 update_flag,
			 force_ID_update,
			 distrust,
			 0,      // chartID is never used when iterating
			 standard_field_flag);
	}

	delete new_strategy;
      }
    }
  }
}

bool fetch_one_star(const char *starname,
		    int update_flag,
		    bool force_ID_update,
		    bool distrust,
		    const char *chartID,
		    int standard_field_flag) {
  char catalog_filename[128];
  CURL *curl_handle;
  CURLcode res;

  struct MemoryStruct chunk;

  chunk.memory = (char *) malloc(1); // will grow as needed
  chunk.size = 0; // no data held

  curl_global_init(CURL_GLOBAL_ALL); // initialize curl
  char url[256];

  Strategy strategy(starname, 0);

  // get the name used as an index into the AAVSO VSP tool
  const char *human_aavso_name = strategy.AAVSOName();
  const char *aavso_name = "<not avail>";
  
  if (!strategy.IsStandardField()) {
    if (!human_aavso_name || *human_aavso_name == 0) {
      if (chartID == 0) {
	fprintf(stderr, "fetch_photometry: cannot tie %s to proper AAVSO name for VSP.\n",
		starname);
	return true;
      }
    } else {
      aavso_name = cleanup_name(human_aavso_name);
    }
  }
  
  if (strategy.IsStandardField() && !standard_field_flag) {
    standard_field_flag = true;
    fprintf(stderr, "Treating %s as a Standard Field.\n",
	    starname);
  }

  const char *standards = (standard_field_flag ? "&special=std_field" : "");

  // Now calculate the URL to be fetched using one of two versions,
  // depending on whether user provided just a star name or a chart ID
  if (chartID != 0 && *chartID != 0) {
    sprintf(url, "https://app.aavso.org/vsp/api/chart/%s/?format=json",chartID);
  } else if (standard_field_flag) {
    // Query by RA/Dec for standard fields
    DEC_RA loc = strategy.GetObjectLocation();
    sprintf(url,
	    "https://app.aavso.org/vsp/api/chart/?format=json&ra=%s&dec=%s&fov=30&maglimit=16.5%s",
	    loc.string_ra_of(), loc.string_fulldec_of(), standards);
  } else {
    sprintf(url,
	    "https://app.aavso.org/vsp/api/chart/?format=json&star=%s&charttitle=%s&fov=30&maglimit=16.5",
	    /*"https://www.aavso.org/apps/vsp/api/chart/?format=json&star=%s&charttitle=%s&fov=30&maglimit=16.5",*/
	    aavso_name, aavso_name);
    /*sprintf(url, "http://www.aavso.org/cgi-bin/vsp.pl?name=%s&charttitle=%s&fov=30&maglimit=16.5&ccdtable=on&delimited=yes%s",
      aavso_name, aavso_name, standards);*/
    /*sprintf(url, "http://www.aavso.org/cgi-bin/vsp.pl?chartid=%s&fov=30&maglimit=16.5&ccdtable=on&delimited=yes%s",
      chartID, standards);*/
  }
  
  fprintf(stderr, "URL query string:\n%s\n", url);
  curl_handle = curl_easy_init();
  curl_easy_setopt(curl_handle, CURLOPT_URL, url);
  curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);

  // tell curl where to put the data
  curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *) &chunk);

  // provide a default user-agent
  curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "fetch_photometry/2.0");

  // and get it.
  fprintf(stderr, "Fetching photometry data for %s ... ", human_aavso_name);
  res = curl_easy_perform(curl_handle);
  if (res != CURLE_OK) {
    fprintf(stderr, "\ncurl fetch failed: %s\n", curl_easy_strerror(res));
  } else {
    fprintf(stderr, "done.\n");
    int fd = open("/tmp/fetch_photometry.data", O_CREAT | O_WRONLY, 0666);
    if (fd >= 0) {
      unsigned int num_written = write(fd, chunk.memory, chunk.size);
      if (num_written != chunk.size) {
	fprintf(stderr, "Problem writing /tmp/fetch_photometry.data\n");
      }
      close(fd);
      fprintf(stderr, "AAVSO response stored in /tmp/fetch_photometry.data\n");
    } else {
      fprintf(stderr, "Failed trying to create /tmp/fetch_photometry.data\n");
    }
  }

  sprintf(catalog_filename, "%s/%s", CATALOG_DIR, starname);
  FILE *catalog_fp = fopen(catalog_filename, "r");
  if (!catalog_fp) {
    fprintf(stderr, "Unable to open catalog file for %s\n", starname);
    exit(2);
  }
  HGSCList catalog(catalog_fp);
  fclose(catalog_fp);

  char *end_of_buffer = ((char *) chunk.memory) + chunk.size;
  *end_of_buffer = 0;
  const char *buffer = (const char *) chunk.memory;
  int files_match = 1; // assume yes to start

  PhotometryRecordSet *phot_list = ParseAAVSOResponse(buffer);
  if (phot_list == 0) {
    fprintf(stderr, "Skipping %s\n", starname);
    return false;
  }
  for (auto phot : *phot_list) {
    const int star_mismatch = compare_photometry(phot, &catalog, update_flag);
    if (star_mismatch) {
      files_match = 0;
      fprintf(stderr, "mismatch: %s\n", phot->PR_AUID);
    }
  }

  // now things get interesting...
  // If things didn't match, then we need to make a change and need to
  // update the Chart ID in the strategy. However, 
  if (force_ID_update) {
    fprintf(stderr, "Updating strategy file with sequence ID: %s\n",
	    sequence_name());
    update_chart_ID(sequence_name(), starname);
  }

  if (files_match) {
    if (distrust) {
      char current_seq_name[32];
      strcpy(current_seq_name, sequence_name()); // remember this for
						 // later
      
      const char *strategy_chart = strategy.ObjectChart();
      fprintf(stderr, "checking photometry for chart ID: %s\n",  strategy_chart);
      if (fetch_one_star(starname, false, false, false, strategy_chart, standard_field_flag)) {
	// current photometry matches what the strategy file says. We
	// be happy.
	fprintf(stderr, "Photometry matched for %s.\n", starname);
	return true;
      } else {
	// the Chart ID is wrong in the strategy file.
	if (update_flag) {
	  catalog.Write(catalog_filename);
	  update_chart_ID(current_seq_name, starname);
	}
	return false;
      }
    } else {
      // no distrust, but files match. Do nothing.
      fprintf(stderr, "Answer: Photometry matched for %s.\n", starname);
      return true;
    }
  } else { // mismatch
    fprintf(stderr, "Answer: New photometry found for %s. %s\n",
	    starname,
	    update_flag ? "Catalog updated." : "");
    if (update_flag) {
      catalog.Write(catalog_filename);
      update_chart_ID(sequence_name(), starname);
    }
    return false;
  }
}
