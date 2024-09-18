/*  set_directive.cc -- set parameters for an analysis
 *
 *  Copyright (C) 2023 Mark J. Munkacsy
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

#include <iostream>
#include <string>
#include <list>

#include <unistd.h>		// getopt()
#include <stdlib.h>		// strtol()

#include <astro_db.h>

//****************************************************************
//        Usage()
//****************************************************************

void usage(void) {
  std::cerr << "Usage: set_directive -d /home/IMAGES/11-28-2022/astro_db.json [-i \n";
  std::cerr << "    -i filename.fits -- images to exclude from analysis\n"
	    << "    -s filename.fits -- images to exclude from stack\n"
	    << "    -d               -- root directory\n"
	    << "    -D filename.fits -- use the filename to find the correct directive\n"
	    << "    -e [f,]GSC...    -- exclude this star from the ensemble [for this filter]\n"
	    << "    -c [f,]GSC...    -- exclude this star from the check star set [filter]\n"
	    << "    -C Vc            -- exclude this color from analysis\n"
	    << "    -t               -- do transforms\n"
	    << "    -E               -- use ensembles\n"
	    << "    -z               -- color-correct ensemble during zero-point calcs\n"
	    << " (Note: -D can provide a directive JUID instead of an imagename.)\n";
    
  exit(-2);
}

//****************************************************************
//        Utility: SplitIntoFilterStarPair(const char *)
//****************************************************************
std::pair<std::string, std::string>
SplitIntoFilterStarPair(const char *s) {
  std::string w(s);
  const unsigned int comma = w.find(',', 0);
  if (comma == std::string::npos) {
    return std::pair<std::string, std::string> ("",w);
  } else {
    return std::pair<std::string, std::string>
      ( w.substr(0, comma), w.substr(comma+1));
  }
  /*NOTREACHED*/
}

//****************************************************************
//        Utility: ConvertFilenameListIntoJUIDList()
//****************************************************************
std::list<juid_t>
*ConvertFilenameListIntoJUIDList(AstroDB &astro_db, std::list<const char *> filelist) {
  std::list<juid_t> *answer = new std::list<juid_t>;
  bool fault = false;
  for (const char *file : filelist) {
    juid_t juid = astro_db.LookupExposure(file);
    if (juid < 0) {
      std::cerr << "set_directive: Can't find juid for imagefile "
		<< file << std::endl;
      fault = true;
    }
    answer->push_back(juid);
  }
  if (fault) usage();
  return answer;
}

//****************************************************************
//        Utility: ConvertStarListIntoJSON()
//****************************************************************
JSON_Expression *ConvertStarListIntoJSON(AstroDB &astro_db,
					 std::list<std::pair<std::string,std::string>> &starlist) {
  JSON_Expression *jlist = new JSON_Expression(JSON_LIST);
  for (std::pair<std::string,std::string> starpair : starlist) {
    std::string &filter = starpair.first;
    std::string &starname = starpair.second;
    JSON_Expression *seq = new JSON_Expression(JSON_SEQ);
    if (filter.length() > 0) {
      seq->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT,
						       "filter",
						       filter.c_str()));
    }
    seq->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT,
						     "name",
						     starname.c_str()));
    jlist->AddToArrayEnd(seq);
  }
  return jlist;
}

//****************************************************************
//        Utility: Convert string into integer (JUID); if not
// valid JUID, return -1
//****************************************************************
juid_t StringToJUID(const char *s) {
  for (const char *x = s; *x; x++) {
    if (not isdigit(*x)) {
      return -1;
    }
  }

  char *end_ptr;
  long value = strtol(s, &end_ptr, 10);
  if (*end_ptr != '\0') {
    fprintf(stderr, "Invalid reference JUID: %s\n", s);
    return -1;
  }
  return value;
}
      
//****************************************************************
//        Main()
//****************************************************************

int main(int argc, char **argv) {
  int ch;
  const char *root_dir = nullptr;
  std::list<const char *> analy_img_exclude_str;
  std::list<const char *> stack_img_exclude_str;
  std::list<std::pair<std::string,std::string>> ensemble_star_exclude;
  std::list<std::pair<std::string,std::string>> check_star_exclude;
  std::list<std::string> color_exclude_str;
  const char *reference_imagefile = nullptr;
  bool do_transforms = false;
  bool use_ensembles = false;
  bool color_correct_zeros = false;

  while ((ch = getopt(argc, argv, "i:s:D:d:e:C:c:tEz")) != -1) {
    switch(ch) {
    case 'i':
      analy_img_exclude_str.push_back(optarg);
      break;

    case 's':
      stack_img_exclude_str.push_back(optarg);
      break;

    case 'C':
      color_exclude_str.push_back(optarg);
      break;

    case 'D':
      reference_imagefile = optarg;
      break;

    case 'd':
      root_dir = optarg;
      break;

    case 'e':
      ensemble_star_exclude.push_back(SplitIntoFilterStarPair(optarg));
      break;

    case 'c':
      check_star_exclude.push_back(SplitIntoFilterStarPair(optarg));
      break;

    case 't':
      do_transforms = true;
      break;

    case 'E':
      use_ensembles = true;
      break;

    case 'z':
      color_correct_zeros = true;
      break;

    case '?':
    default:
      usage();
      /*NOTREACHED*/
    }
  }

  if (root_dir == nullptr or reference_imagefile == nullptr) usage();

  AstroDB astro_db(JSON_READWRITE, root_dir);
  juid_t directive_juid = StringToJUID(reference_imagefile);    
  if (directive_juid < 0) {
    juid_t reference_juid = astro_db.LookupExposure(reference_imagefile);
    if (reference_juid < 0) {
      std::cerr << "set_directive: ERROR: cannot find image "
		<< reference_imagefile << std::endl;
      usage();
      /*NOTREACHED*/
    }
    JSON_Expression *ref_exp = astro_db.FindByJUID(reference_juid);
    directive_juid = ref_exp->Value("directive")->Value_int();
  }
    
  astro_db.DeleteEntryForJUID(directive_juid);

  astro_db.CreateEmptyDirective(directive_juid);
  JSON_Expression *new_seq = astro_db.FindByJUID(directive_juid);


  new_seq->InsertUpdateTSTAMPInSeq();
  new_seq->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT,
						       "use_ensemble",
						       (long) use_ensembles));
  new_seq->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT,
						       "do_transform",
						       (long) do_transforms));
  new_seq->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT,
						       "zero_pt_xform",
						       (long) color_correct_zeros));
  if (stack_img_exclude_str.size()) {
    std::list<juid_t> *stack_img_exclude_juid =
      ConvertFilenameListIntoJUIDList(astro_db, stack_img_exclude_str);
    new_seq->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT,
							 "stack_excl",
							 new JSON_Expression(JSON_LIST,
									     *stack_img_exclude_juid)));
  }

  if (analy_img_exclude_str.size()) {
    std::list<juid_t> *analy_img_exclude_juid =
      ConvertFilenameListIntoJUIDList(astro_db, analy_img_exclude_str);
    new_seq->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT,
							 "img_analy_excl",
							 new JSON_Expression(JSON_LIST,
									     *analy_img_exclude_juid)));
  }

  if (ensemble_star_exclude.size()) {
    JSON_Expression *exp = ConvertStarListIntoJSON(astro_db, ensemble_star_exclude);
    new_seq->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT,
							 "ensemble_excl",
							 exp));
  }
  if (check_star_exclude.size()) {
    JSON_Expression *exp = ConvertStarListIntoJSON(astro_db, check_star_exclude);
    new_seq->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT,
							 "check_excl",
							 exp));
  }
  if (color_exclude_str.size()) {
    new_seq->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT,
							 "color_excl",
							 color_exclude_str));
  }

  return 0;
}
