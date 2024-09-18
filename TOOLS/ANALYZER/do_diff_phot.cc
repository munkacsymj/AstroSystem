/*  do_diff_phot.cc -- Perform differential photometry
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
void DoDiffPhot(juid_t source);
const char *TargetName(std::list<long> &juid_list, AstroDB &db);
std::list<HGSC *> &
GetComps(std::list<std::string> &comp_names, HGSCList &catalog);
const char *GetColor(std::list<long> &juid_list, AstroDB &db);
void DoDiffPhotometry(const char *target,
		      std::list<long> &juid_list,
		      HGSCList &catalog,
		      char technique, // one of 'S', 'E', 'C'
		      long ref_set_juid,
		      Filter &filter,
		      std::list<HGSC *> &comps,
		      AstroDB &astro_db);

void fail(void) {
  exit(-2);
}

void usage(void) {
  std::cerr << "Usage: do_diff_phot -d /home/IMAGES/11-28-2022/astro_db.json -i 6000136 -i ... -c PG0918-C -c ... \n";
  std::cerr << "    -i -- image juid\n"
	    << "    -c -- comparison starname\n"
	    << "    -d -- root directory\n"
	    << "    -s -- the set the diff phot will attach to\n"
	    << "    -t -- the technique: E, C, or S (ensemble, single-comp, standard field)\n";
    
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
  std::list<long> juid_input_list;
  std::list<std::string> comp_star_list;
  long ref_set_juid = 0;
  int technique = ' '; // space triggers default
  //const char *analysis_technique = nullptr;

  // Command line options:
  // -i juid                      Specified multiple times
  // -s juid                      The relevant set
  // -c GSC00231-01015            Specified multiple times
  // -d /home/IMAGES/9-2-2022     Root directory
  // -t analysis_technique        E, C, or S

  while((ch = getopt(argc, argv, "t:s:i:c:d:")) != -1) {
    switch(ch) {
      //case 'a':
      //analysis_technique = optarg;
      //break;
      
    case 'i':
      {
	unsigned long juid;
	if (fetch_juid(optarg, juid)) {
	  juid_input_list.push_back(juid);
	} else {
	  std::cerr << "do_diff_phot: ERROR: invalid image juid: "
		    << juid << std::endl;
	  usage();
	}
      }
      break;

    case 't':
      if (strlen(optarg) != 1) {
	std::cerr << "do_diff_phot: technique must be single letter, not "
		  << optarg << std::endl;
	usage();
      }
      if (optarg[0] == 'C' or
	  optarg[0] == 'E' or
	  optarg[0] == 'S') {
	technique = optarg[0];
      } else {
	std::cerr << "do_diff_phot: invalid technique: " << optarg << std::endl;
	usage();
      }
      break;

    case 'c':
      comp_star_list.push_back(std::string(optarg));
      break;

    case 's':
      {
	unsigned long juid;
	if (fetch_juid(optarg, juid)) {
	  ref_set_juid = juid;
	} else {
	  std::cerr << "do_diff_phot: ERROR: invalid set juid: "
		    << juid << std::endl;
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
  // Need the target object name. Get this from the images
  const char *target = TargetName(juid_input_list, astro_db);
  std::cerr << "target = " << target << std::endl;
  // Need the catalog. (The -c options force an override of anything
  // found in the catalog.)
  HGSCList catalog(target);
  Strategy strategy(target); // need is_standard_field
  if (not catalog.NameOK()) {
    std::cerr << "do_diff_phot: ERROR target "
	      << target
	      << ": unable to open corresponding catalog\n";
    fail();
  }

  // Now let's straighten out technique. If specified on the command
  // line, that overrides everything else
  if (technique == ' ') {
    if (strategy.IsStandardField()) {
      technique = 'S';
    } else {
      technique = 'C';
    }
  }
  
  std::list<HGSC *> comps = GetComps(comp_star_list, catalog);
  std::cout << "Using comp star(s):\n";
  for (auto comp : comps) {
    std::cout << "    " << comp->label << std::endl;
  }
  Filter filter(GetColor(juid_input_list, astro_db));

  DoDiffPhotometry(target,
		   juid_input_list,
		   catalog,
		   technique,
		   ref_set_juid,
		   filter,
		   comps,
		   astro_db);
  
  astro_db.SyncAndRelease();
  return 0;
}

//****************************************************************
//        Helper: TargetName()
//****************************************************************
const char *TargetName(std::list<long> &juid_list, AstroDB &db) {
  const char *target = nullptr;

  for (long juid : juid_list) {
    std::cerr << "Lookup juid " << juid << std::endl;
    JSON_Expression *exp = db.FindByJUID(juid);
    const char *name = exp->GetValue("target")->Value_char();
    if (target == nullptr) {
      target = name;
    } else {
      if (strcmp(target, name) != 0) {
	std::cerr << "do_diff_phot.TargetName() inconsistency: "
		  << target << " vs. " << name << std::endl;
	fail();
	/*NOTREACHED*/
      }
    }
  }
  return target;
}
  
//****************************************************************
//        Helper: Get List of HGSC stars to be comp
//****************************************************************
std::list<HGSC *> &
GetComps(std::list<std::string> &comp_names,
	 HGSCList &catalog) {
  std::list<HGSC *> *answer = new std::list<HGSC *>;
  if (comp_names.size() > 0) {
    // Use the specified ensemble
    for (std::string name : comp_names) {
      HGSC *comp = catalog.FindByLabel(name.c_str());
      if (comp) {
	answer->push_back(comp);
      } else {
	std::cerr << "do_diff_phot: ERROR: specified comp, "
		  << name << " not found in catalog\n";
	fail();
      }
    }
  } else { // Otherwise, using comp star found in catalog
    HGSC *comp = nullptr;
    
    HGSCIterator it(catalog);
    for(HGSC *star = it.First(); star; star = it.Next()) {
      if (star->is_comp) {
	if (comp != nullptr) {
	  std::cerr << "do_diff_phot: ERROR: multiple comps in catalog\n";
	  fail();
	} else {
	  comp = star;
	}
      }
    }
    answer->push_back(comp);
  }
  return *answer;
}

//****************************************************************
//        Helper: Get color
//****************************************************************
const char *GetColor(std::list<long> &juid_list, AstroDB &db) {
  const char *color_name = nullptr;

  for (long juid : juid_list) {
    JSON_Expression *exp = db.FindByJUID(juid);
    const char *name = exp->GetValue("filter")->Value_char();
    if (color_name == nullptr) {
      color_name = name;
    } else {
      if (strcmp(color_name, name) != 0) {
	std::cerr << "do_diff_phot.GetColor() inconsistency: "
		  << color_name << " vs. " << name << std::endl;
	fail();
	/*NOTREACHED*/
      }
    }
  }
  return color_name;
}
  
//****************************************************************
//        The Hard Part: DoDiffPhotometry
//****************************************************************
struct OneStar {
  const char *name;
  double mag_sum {0.0};
  double mag_sum_sq {0.0};
  double snr_sum_sq {0.0};
  int num_measures {0};		// this is also # images
  HGSC *hgsc {nullptr};
  bool is_comp {false};
  bool is_check {false};
  bool is_in_list_of_comps {false};
  double diff_mag;
  double diff_ucty_snr;
  double diff_ucty_scatter;
  double residual_err;
};

std::unordered_map<std::string, OneStar *> all_stars;
  
HGSC *IsACompStar(const char *starname, std::list<HGSC *> &comps) {
  for (auto comp : comps) {
    if (strcmp(starname, comp->label) == 0) return comp;
  }
  return nullptr;
}

void DoDiffPhotometry(const char *target,
		      std::list<long> &juid_list,
		      HGSCList &catalog,
		      char technique,
		      long ref_set_juid,
		      Filter &filter,
		      std::list<HGSC *> &comps, // remember, might be empty
		      AstroDB &astro_db) {

  auto filter_color = FilterToColor(filter);
  // things that will be averaged across all images...
  double jd_midpoint_sum {0.0};
  double airmass_sum {0.0};
  double exposure_time_sum {0.0};
  const int num_images = juid_list.size();
  double CMAG = NAN;
  juid_t directive = -1;

  // loop through the images...
  for (auto juid : juid_list) {
    //********************************
    // Find instrumental mags for
    // this image.
    //********************************
    juid_t inst_juid = astro_db.InstMagsForJUID(juid);
    JSON_Expression *exp = astro_db.FindByJUID(inst_juid);

    // At this point, "juid" is juid of the underlying image and
    // "inst_juid" is the juid of that image's instrumental mags.
    JSON_Expression *image_exp = astro_db.FindByJUID(juid);
    if (directive < 0) {
      directive = image_exp->Value("directive")->Value_int();
    }

    jd_midpoint_sum += image_exp->Value("julian")->Value_double();
    airmass_sum += image_exp->Value("airmass")->Value_double();
    exposure_time_sum += image_exp->Value("exposure")->Value_double();

    if (exp == 0) {
      std::cerr << "do_diff_phot: ERROR: no instrumental mags for "
		<< inst_juid << std::endl;
      fail();
    }

    double sum_comp_truth = 0.0;
    double sum_comp_inst_mags = 0.0;
    //double sum_comp_ucty_sq = 0.0;
    int num_comps_found = 0;

    std::list<JSON_Expression *> measurements = exp->Value("measurements")->Value_list();
    for (auto measurement : measurements) {
      const char *this_starname = measurement->Value("name")->Value_char();
      std::string this_starname_s {this_starname};
      const double inst_mag = measurement->Value("imag")->Value_double();
      auto star_iterator = all_stars.find(this_starname_s);
      OneStar *star = nullptr;
      if (star_iterator == all_stars.end()) {
	star = new OneStar;
	star->name = this_starname;
	star->hgsc = catalog.FindByLabel(this_starname);
	if (star->hgsc) {
	  star->is_comp = star->hgsc->is_comp;
	  star->is_check = star->hgsc->is_check;
	  star->is_in_list_of_comps = IsACompStar(this_starname, comps) ? true : false;
	}
	all_stars[this_starname_s] = star;
      } else {
	star = star_iterator->second;
      }

      if (star->hgsc->multicolor_data.IsAvailable(filter_color) and
	  ((star->is_comp and (technique == 'C' or technique == 'E')) or
	   (star->is_check and (technique == 'S')))) {
	sum_comp_inst_mags += inst_mag;
	sum_comp_truth += star->hgsc->multicolor_data.Get(filter_color);
	num_comps_found++;
      }
    }

    //********************************
    //    Calculate Zero Point
    //********************************
    const double zero = (sum_comp_inst_mags - sum_comp_truth)/num_comps_found;
    if (technique == 'C') CMAG = sum_comp_inst_mags/num_comps_found;

    //********************************
    //  Second pass: zero-adjust and remember the result
    //********************************
    for (auto measurement : measurements) {
      std::string this_starname {measurement->Value("name")->Value_char()};
      OneStar *star = all_stars[this_starname];
      double inst_mag = measurement->Value("imag")->Value_double();
      JSON_Expression *uncty_exp = measurement->Value("uncty");
      double uncty_snr = -1.0; // <0 => invalid
      if (uncty_exp) uncty_snr = uncty_exp->Value_double();
      double adj_mag = inst_mag - zero;

      star->mag_sum += adj_mag;
      star->mag_sum_sq += (adj_mag*adj_mag);
      star->snr_sum_sq += (uncty_snr*uncty_snr);
      star->num_measures++;
    }
  } // end loop over all images

  //********************************
  // Calculate check star stats
  //********************************
  double check_sum_sq = 0.0;
  int num_checks = 0;
  std::list<std::string> *check_star_names = new std::list<std::string>;
  for (auto starpair : all_stars) {
    OneStar *star = starpair.second;
    double mag = star->mag_sum/star->num_measures;
    double uncty = sqrt(star->mag_sum_sq/star->num_measures - (mag*mag));
    double uncty_snr = sqrt(star->snr_sum_sq/star->num_measures);
    star->diff_mag = mag;
    star->diff_ucty_scatter = uncty;
    star->diff_ucty_snr = uncty_snr;
    if (star->is_check and star->hgsc and star->hgsc->multicolor_data.IsAvailable(filter_color)) {
      star->residual_err = star->hgsc->multicolor_data.Get(filter_color) - star->diff_mag;
      check_sum_sq += star->residual_err*star->residual_err;
      check_star_names->push_back(star->name);
      num_checks++;
    }
  }
  const double check_rms = (num_checks > 0 ? sqrt(check_sum_sq/num_checks) : 0.0);

  // If the diff phot entry already exists, delete it
  // Need to turn ref_set_juid (a set juid) into a diff phot juid
  juid_t existing_diff_phot = astro_db.DiffPhotForJUID(ref_set_juid);
  if (existing_diff_phot) {
    astro_db.DeleteEntryForJUID(existing_diff_phot);
  }

  bool is_stack = (GetJUIDType(ref_set_juid) == DB_STACKS);
  // Create a list of comp star names that can be passed to
  // AddDiffMags()
  std::list<std::string> *comp_star_names = new std::list<std::string>;
  for (auto comp : comps) {
    comp_star_names->push_back(comp->label);
  }
  Strategy strategy(target);

  AstroDB::DiffMagProfile *profile = new AstroDB::DiffMagProfile;;

  profile->profile_name = "profile1";
  profile->julian = jd_midpoint_sum/num_images;
  profile->exposure_time = exposure_time_sum/num_images;
  profile->airmass = airmass_sum/num_images;
  profile->target = target;
  profile->filter = strdup(filter.NameOf());
  profile->technique = (comps.size() > 1 ? "ENSEMBLE" : "SINGLE_COMP");
  profile->crefmag = CMAG;
  profile->comp_star_names = comp_star_names;
  profile->check_star_names = check_star_names;
  profile->check_rms = check_rms;
  profile->chartID = strdup(strategy.ObjectChart());

  std::list<AstroDB::DiffMagMeasurement> diff_mags;
  for (auto starpair : all_stars) {
    OneStar *star = starpair.second;
    double mag = star->mag_sum/star->num_measures;
    double uncty = star->mag_sum_sq/star->num_measures - (mag*mag);
    double uncty_snr = sqrt(star->snr_sum_sq/star->num_measures);
    star->diff_mag = mag;
    star->diff_ucty_scatter = uncty;
    star->diff_ucty_snr = uncty_snr;
    diff_mags.push_back(AstroDB::DiffMagMeasurement { star->name,
						     star->diff_mag,
						     star->diff_ucty_scatter,
						     star->diff_ucty_snr,
						     is_stack,
						     star->num_measures,
						     profile});
  }

  astro_db.AddDiffMags(ref_set_juid, directive, diff_mags);
}
