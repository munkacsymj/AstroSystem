/*  do_merge.cc -- Combine stack photometry with individ image photometry
 *
 *  Copyright (C) 2022 Mark J. Munkacsy
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

#include <list>
#include <iostream>
#include <string>
#include <unordered_map>

#include <ctype.h>
#include <string.h>
#include <unistd.h>		// getopt()

#include <astro_db.h>
#include <strategy.h>
#include <HGSC.h>
#include <Filter.h>

//****************************************************************
//        Forward Declarations
//****************************************************************
void DoMerge(juid_t source, AstroDB &astro_db);
const char *TargetName(std::list<long> &juid_list, AstroDB &db);
const char *GetColor(std::list<long> &juid_list, AstroDB &db);

void fail(void) {
  exit(-2);
}

void usage(void) {
  std::cerr << "Usage: do_merge -d /home/IMAGES/11-28-2022/astro_db.json -i 5000136\n";
  std::cerr << "    -i -- juid of the merge set\n";
    
  exit(-2);
}

//****************************************************************
//        Fetch JUID off the command line
//****************************************************************
bool fetch_juid(const char *s, unsigned long &target) {
  target = 0;
  do {
    if (*s == 0) {
      return true; // everything okay
    } else if (not isdigit(*s)) {
      return false; // bad character
    } else {
      target = (target*10)+(*s - '0');
    }
  } while(*++s);
  return true;
}

//****************************************************************
//        main()
//****************************************************************
int main(int argc, char **argv) {
  int ch;
  const char *root_dir = nullptr;
  unsigned long ref_set_juid = 0;

  // Command line options:
  // -i juid                      Specified once
  // -d /home/IMAGES/9-2-2022     Root directory

  while((ch = getopt(argc, argv, "i:d:")) != -1) {
    switch(ch) {
    case 'i':
      {
	if (fetch_juid(optarg, ref_set_juid)) {
	  ;
	} else {
	  std::cerr << "do_merge: ERROR: invalid juid: "
		    << ref_set_juid << std::endl;
	  usage();
	}
      }
      break;

    case 'd':
      root_dir = optarg;
      break;

    case '?':
    default:
      usage();
   }
  }

  if (root_dir == nullptr or ref_set_juid == 0) usage();

  AstroDB astro_db(JSON_READWRITE, root_dir);

  DoMerge(ref_set_juid, astro_db);
  astro_db.SyncAndRelease();
  return 0;
}

//****************************************************************
//        The Hard Part: DoDiffPhotometry
//****************************************************************
struct OneStar {
  const char *name;
  struct {
    bool is_avail { false };
    double mag;
    double snr_ucty;
    double scatter_ucty;
    double sequence_ucty;
    long num_vals {-1};
    AstroDB::DiffMagProfile *profile;
  } diff_data[2]; 
};

void ExtractProfiles(JSON_Expression *exp,
		     int source_tag,
		     std::list<AstroDB::DiffMagProfile *> &prof_list) {
  std::list<JSON_Expression *> &prof_exps = exp->Value("profile")->Value_list();
  for (auto p_exp : prof_exps) {
    AstroDB::DiffMagProfile *mp = new AstroDB::DiffMagProfile;
    mp->profile_name = p_exp->Value("name")->Value_char();
    mp->profile_source_tag = source_tag;
    mp->julian = p_exp->Value("julian")->Value_double();
    mp->exposure_time = p_exp->Value("exposure")->Value_double();
    mp->airmass = p_exp->Value("airmass")->Value_double();
    mp->crefmag = p_exp->Value("crefmag")->Value_double();
    mp->check_rms = p_exp->Value("check_rms")->Value_double();
    mp->target = p_exp->Value("target")->Value_char();
    mp->filter = p_exp->Value("filter")->Value_char();
    mp->technique = p_exp->Value("technique")->Value_char();
    mp->chartID = p_exp->Value("chartid")->Value_char();
    mp->comp_star_names = new std::list<std::string>;
    mp->check_star_names = new std::list<std::string>;
    for (auto s_exp : p_exp->Value("comp")->Value_list()) {
      mp->comp_star_names->push_back(s_exp->Value_string());
    }
    for (auto s_exp : p_exp->Value("checks")->Value_list()) {
      mp->check_star_names->push_back(s_exp->Value_string());
    }
    prof_list.push_back(mp);
  }
}

void ReadStars(JSON_Expression *exp,
	       int index,
	       std::list<AstroDB::DiffMagProfile *> &all_profiles,
	       std::list<OneStar *> &starlist) {
  for (auto e : exp->Value_list()) {
    // e now points to a SEQ that represents one measurement
    const char *starname = e->Value("name")->Value_char();
    // does the star already exist?
    OneStar *star = nullptr;
    for (auto s : starlist) {
      if (strcmp(s->name, starname) == 0) {
	star = s;
	break;
      }
    }
    if (star == nullptr) {
      star = new OneStar;
      star->name = starname;
    }

    const char *profile_name = e->Value("profile")->Value_char();
    AstroDB::DiffMagProfile *profile = nullptr;
    
    for (auto p : all_profiles) {
      if (strcmp(p->profile_name, profile_name) == 0 and
	  p->profile_source_tag == index) {
	profile = p;
	break;
      }
    }
    if (!profile) {
      std::cerr << "do_merge: ERROR: ReadStars(): no profile match found: "
		<< starname << std::endl;
    }
    star->diff_data[index] = {
      true,
      e->Value("mag")->Value_double(),
      e->Value("uncty/snr")->Value_double(),
      e->Value("uncty/stddev")->Value_double(),
      0.0, // sequence_ucty
      e->Value("numvals")->Value_int(),
      profile
    };
  }
}

void DoMerge(juid_t merge_set,
	     AstroDB &astro_db) {

  JSON_Expression *exp = astro_db.FindByJUID(merge_set);
  std::list<JSON_Expression *> &exp_list = exp->Value("input")->Value_list();
  std::list<juid_t> juid_list;
  for (JSON_Expression *item : exp_list) {
    juid_list.push_back(item->Value_int());
  }
  if (juid_list.size() != 2) {
    std::cerr << "DoMerge(): wrong juid list size: " << juid_list.size() << std::endl;
  }
  juid_t juid1 = juid_list.front();
  juid_t juid2 = juid_list.back();

  JSON_Expression *exp1 = astro_db.FindByJUID(juid1);
  JSON_Expression *exp2 = astro_db.FindByJUID(juid2);

  std::list<AstroDB::DiffMagProfile *> all_profiles;
  std::list<OneStar *> starlist;
  ExtractProfiles(exp1, 0, all_profiles);
  ExtractProfiles(exp2, 1, all_profiles);
  ReadStars(exp1, 0, all_profiles, starlist);
  ReadStars(exp2, 1, all_profiles, starlist);

  std::list<AstroDB::DiffMagMeasurement> diff_mags;
  // now choose
  for (auto s : starlist) {
    int index = -1; // initially, an invalid value
    if (s->diff_data[0].is_avail == false) {
      index = 1;
    } else if (s->diff_data[1].is_avail == false) {
      index = 0;
    } else {
      // otherwise, both had values
      if (s->diff_data[0].num_vals >
	  s->diff_data[1].num_vals) {
	index = 0;
      } else {
	index = 1;
      }
    }
    diff_mags.push_back( {
	s->name,
	  s->diff_data[index].mag,
	  s->diff_data[index].scatter_ucty,
	  s->diff_data[index].snr_ucty,
	  false,		// this is WRONG!!!
	  s->diff_data[index].num_vals,
	  s->diff_data[index].profile});
  }

  astro_db.AddDiffMags(merge_set,
		       diff_mags);
  
  
}
