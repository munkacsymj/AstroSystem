/*  astro_db.cc -- Implements the JSON-based ASTRO database
 *
 *  Copyright (C) 2022 Mark J. Munkacsy

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

#include "astro_db.h"
#include <Image.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <ctype.h>		// isdigit()
#include <filesystem>
#include <iostream>

// This takes a pathname in the form
// "/home/IMAGES/5-30-2023/astro_db.json" and returns a new string
// (via strdup) equal to "5-30-2023"
const char *extract_date_string(const char *s) {
  char buffer[98];
  char *substrings[10];
  int num_parts = 0;
  strcpy(buffer, s);
  for(char *p = buffer; *p; p++) {
    if (*p == '/') {
      *p = 0;
      substrings[++num_parts] = (p+1);
    }
  }
  substrings[0] = buffer;
  const char *answer = substrings[num_parts-1];
  fprintf(stderr, "extract_date_string found '%s'\n", answer);
  return strdup(answer);
}

AstroDB::AstroDB(int mode, const char *date) {
  char astro_db_filename[96];
  const char *date_string;

  if (date and isdigit(date[0])) {
    sprintf(astro_db_filename, "/home/IMAGES/%s/astro_db.json", date);
    date_string = date;
  } else if (date) {
    int n = strlen(date);
    if (isdigit(*(date+n-4)) and isdigit(*(date+n-3))) {
      // ends with the date
      sprintf(astro_db_filename, "%s/astro_db.json", date);
      date_string = extract_date_string(astro_db_filename);
    } else {
      strcpy(astro_db_filename, date);
      date_string = extract_date_string(astro_db_filename);
    }
  } else {
    // The following call will also create the directory if it doesn't already exist
    const char *base_directory = DateToDirname();
    sprintf(astro_db_filename, "%s/astro_db.json", base_directory);
    date_string = base_directory+13;
  }

  this->working_date = strdup(date_string);
  this->sync_filename = strdup(astro_db_filename);
  this->file_mode = mode;

  // The following line will abort the program if it fails
  al_exp.SyncWithFile(astro_db_filename, mode);

  if (al_exp.IsEmpty()) {
    JSON_Expression *top = al_exp.CreateBlankTopLevelSeq();
    top->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT, "session",
						     new JSON_Expression(JSON_LIST)));
    top->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT, "exposures",
						     new JSON_Expression(JSON_LIST)));
    top->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT, "stacks",
						     new JSON_Expression(JSON_LIST)));
    top->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT, "inst_mags",
						     new JSON_Expression(JSON_LIST)));
    top->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT, "directives",
						     new JSON_Expression(JSON_LIST)));
    top->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT, "analyses",
						     new JSON_Expression(JSON_LIST)));
    top->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT, "sets",
						     new JSON_Expression(JSON_LIST)));
    top->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT, "submissions",
						     new JSON_Expression(JSON_LIST)));
    al_exp.Print(stderr);
  }
  juid.Initialize(al_exp);
  al_exp.Validate();
}

void assign_cat_string(char *buffer,
		       const char *variable_string,
		       const char *value_string) {
  char assignment[64];
  bool first = (*buffer == 0);
  sprintf(assignment, "%c \"%s\" : \"%s\"",
	  (first ? ' ' : ','),
	  variable_string,
	  value_string);
  strcat(buffer, assignment);
}

void assign_cat_int(char *buffer,
		    const char *variable_string,
		    int value) {
  char assignment[64];
  bool first = (*buffer == 0);
  sprintf(assignment, "%c \"%s\" : %d\n",
	  (first ? ' ' : ','),
	  variable_string,
	  value);
  strcat(buffer, assignment);
}



int
AstroDB::NewSession(const char *type) {
  al_exp.Validate();
  JSON_Expression *sessions = al_exp.GetValue("session");
  assert(sessions);
  long existing_session_id = -1;
  for (auto x : sessions->Value_list()) {
    int this_session_id = x->GetValue("seq")->Value_int();
    if (this_session_id > existing_session_id) existing_session_id = this_session_id;
  }

  // "sessions" is a JSON_LIST. We add a new sequence to the end of
  // the list and then we put assignments into that sequence
  JSON_Expression *seq = new JSON_Expression(JSON_SEQ);
  sessions->AddToArrayEnd(seq);
  al_exp.Validate();

  existing_session_id++;
  fprintf(stderr, "Creating session id = %ld\n", existing_session_id);
  char logfile_name[80];
  char stdout_name[80];
  sprintf(logfile_name, "session%ld.log", existing_session_id);
  sprintf(stdout_name, "session%ld.shell", existing_session_id);

  seq->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT,
						   "date",
						   new JSON_Expression(JSON_STRING,
								       this->working_date)));
  al_exp.Validate();
  seq->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT,
						   "seq",
						   new JSON_Expression(JSON_INT,
								       existing_session_id)));
  seq->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT,
						   "logfile",
						   new JSON_Expression(JSON_STRING,
								       strdup(logfile_name))));
  seq->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT,
						   "stdout",
						   new JSON_Expression(JSON_STRING,
								       strdup(stdout_name))));
  seq->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT,
						   "type",
						   new JSON_Expression(JSON_STRING, type)));


  al_exp.Validate();
  return existing_session_id;
}
  
			 
void
AstroDB::SyncAndRelease(void) {
  al_exp.WriteAndReleaseFileSync();
}

void
AstroDB::Reactivate(bool *anything_changed) {
  al_exp.ReSyncWithFile(file_mode, anything_changed);
  juid.Initialize(al_exp);
}

static bool starts_with(const char *fullstring, const char *pattern) {
  while(*fullstring and *pattern) {
    if (*fullstring++ != *pattern++) return false;
    if (*pattern == 0) return true;
  }
  return false;
}

juid_t
AstroDB::AddBVRISet(std::list<juid_t> &input, juid_t directive) {
  JSON_Expression *exp_list = al_exp.Value("sets");
  
  if (not exp_list->IsList()) {
    fprintf(stderr, "AddBVRISet: sets isn't list.\n");
    exp_list->Print(stderr);
    return -1;
  }

  JSON_Expression *new_seq = new JSON_Expression(JSON_SEQ);
  juid_t this_juid = juid.GetNextJUID(DB_SET);
  new_seq->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT,
						       "juid",
						       this_juid));
  if (directive) {
    new_seq->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT,
							 "directive",
							 directive));
  }

  JSON_Expression *new_exp = new JSON_Expression(JSON_LIST);
  for (juid_t juid : input) {
    new_exp->AddToArrayEnd(new JSON_Expression(JSON_INT,
					       juid));
  }
  new_seq->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT,
						       "input",
						       new_exp));
  new_seq->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT,
						       "stype",
						       "BVRI"));
  exp_list->AddToArrayEnd(new_seq);
  return this_juid;
}
  
juid_t
AstroDB::AddMergeSet(juid_t input_stack,
		     juid_t directive,
		     juid_t input_subexp) {
  JSON_Expression *exp_list = al_exp.Value("sets");

  if (not exp_list->IsList()) {
    std::cerr << "AddMergeSet: sets isn't list.\n";
    exp_list->Print(stderr);
    return -1;
  }

  JSON_Expression *exp = FindByJUID(input_subexp);
  const char *filter = exp->Value("filter")->Value_char();

  JSON_Expression *new_seq = new JSON_Expression(JSON_SEQ);
  juid_t this_juid = juid.GetNextJUID(DB_SET);
  new_seq->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT,
						       "juid",
						       this_juid));
  if (directive) {
    new_seq->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT,
							 "directive",
							 directive));
  }
  JSON_Expression *new_exp = new JSON_Expression(JSON_LIST);
  new_exp->AddToArrayEnd(new JSON_Expression(JSON_INT, input_stack));
  new_exp->AddToArrayEnd(new JSON_Expression(JSON_INT, input_subexp));
  new_seq->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT,
						       "input",
						       new_exp));
  new_seq->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT,
						       "stype",
						       "MERGE"));
  new_seq->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT,
						       "filter",
						       filter));
  new_seq->InsertUpdateTSTAMPInSeq();

  exp_list->AddToArrayEnd(new_seq);
  return this_juid;
}

juid_t
AstroDB::AddSubexpSet(const char *filter,
		      juid_t directive,
		      std::list<juid_t> &input) {
  JSON_Expression *exp_list = al_exp.Value("sets");
  
  if (not exp_list->IsList()) {
    fprintf(stderr, "AddSubexpSet: sets isn't list.\n");
    exp_list->Print(stderr);
    return -1;
  }

  JSON_Expression *new_seq = new JSON_Expression(JSON_SEQ);
  juid_t this_juid = juid.GetNextJUID(DB_SET);
  new_seq->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT,
						       "juid",
						       this_juid));
  if (directive) {
    new_seq->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT,
							 "directive",
							 directive));
  }

  JSON_Expression *new_exp = new JSON_Expression(JSON_LIST);
  for (juid_t juid : input) {
    new_exp->AddToArrayEnd(new JSON_Expression(JSON_INT,
					       juid));
  }
  new_seq->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT,
						       "input",
						       new_exp));

  new_seq->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT,
						       "stype",
						       "SUBEXP"));
  new_seq->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT,
						       "filter",
						       filter));
  
  exp_list->AddToArrayEnd(new_seq);
  return this_juid;
}
  
juid_t
AstroDB::AddExposure(const char *fits_filename,
		     const char *target,
		     const char *filter,
		     juid_t directive,
		     JULIAN midpoint,
		     double exposure_time,
		     double airmass,
		     const char *chartname,
		     bool needs_dark,
		     bool needs_flat) {
  std::filesystem::path fits_path_raw(fits_filename);
  std::filesystem::path fits_path_full = std::filesystem::weakly_canonical(fits_path_raw);
  
  JSON_Expression *exp_list = al_exp.Value("exposures");
  if (not exp_list->IsList()) {
    fprintf(stderr, "AddExposure: exposure isn't list.\n");
    exp_list->Print(stderr);
    return -1;
  }

  JSON_Expression *new_exp = new JSON_Expression(JSON_SEQ);
  new_exp->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT,
						       "filename",
						       strdup(fits_path_full.c_str())));
  juid_t this_juid = juid.GetNextJUID(DB_IMAGE);
  new_exp->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT,
						       "juid",
						       this_juid));
  new_exp->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT,
						       "target",
						       target));
  new_exp->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT,
						       "filter",
						       filter));
  new_exp->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT,
						       "directive",
						       directive));
  new_exp->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT,
						       "julian",
						       midpoint.day()));
  new_exp->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT,
						       "exposure",
						       exposure_time));
  new_exp->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT,
						       "airmass",
						       airmass));
  new_exp->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT,
						       "chart",
						       chartname));
  new_exp->InsertUpdateTSTAMPInSeq();

  // extract base directory, including date
  if (not starts_with(fits_filename, "/home/IMAGES/")) {
    fprintf(stderr, "AstroDB::AddExposure: invalid base directory: %s\n", fits_filename);
    return -1;
  }
  const char *p = fits_filename + 13;
  while(*p and *p != '/') p++;
  char base_dir[3+p-fits_filename];
  char *d = base_dir;
  for(const char *x = fits_filename; x<p; x++, d++) {
    *d = *x;
  }
  *d = 0;

  if (needs_dark or needs_flat) {
    Image image(fits_filename);
    ImageInfo *info = image.GetImageInfo();
    if (needs_dark and info and info->ExposureDurationValid()) {
      double exp_time = info->GetExposureDuration();
      // This checks to see if the specified exposure time is an
      // integer >= 1
      if (fabs(exp_time - int(exp_time + 0.5)) < 0.1 and exp_time > 0.9) {
	char dark_filename[80];
	
	sprintf(dark_filename, "%s/dark%d.fits", base_dir, int(exp_time+0.5));
	new_exp->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT,
							     "dark",
							     strdup(dark_filename)));
      }
    }

    if (needs_flat and info and info->FilterValid()) {
      Filter filter = info->GetFilter();
      char flat_filename[80];
      sprintf(flat_filename, "%s/flat_%s.fits", base_dir, filter.NameOf());
      new_exp->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT,
							   "flat",
							   strdup(flat_filename)));
    }
  }
  exp_list->AddToArrayEnd(new_exp);
  return this_juid;
}

juid_t
AstroDB::AddRefreshStack(const char *filter,
			 juid_t directive,
			 const char *target_object,
			 const char *stack_filename,
			 std::list<const char *> &constituent_filenames,
			 bool filenames_are_actual) {
  std::list<long> constituents;

  for (auto f : constituent_filenames) {
    std::filesystem::path fits_path_raw(f);
    std::filesystem::path fits_path_full = std::filesystem::weakly_canonical(fits_path_raw);

    juid_t one_juid = LookupExposure(fits_path_full.c_str());
    if (one_juid == 0) {
      fprintf(stderr, "RefreshStack: filename not in astro_db: %s\n",
	      fits_path_full.c_str());
    } else {
      constituents.push_back((long) one_juid);
    }
  }

  return AddRefreshStack(filter, directive, target_object,
			 stack_filename, constituents, filenames_are_actual);
}

juid_t
AstroDB::AddRefreshStack(const char *filter,
			 juid_t directive,
			 const char *target_object,
			 const char *stack_filename,
			 std::list<juid_t> &constituent_juids,
			 bool filenames_are_actual) {
  std::filesystem::path fits_path_raw(stack_filename);
  std::filesystem::path fits_path_full = std::filesystem::weakly_canonical(fits_path_raw);

  JSON_Expression *this_stack = nullptr;
  JSON_Expression *stack_list = al_exp.Value("stacks");
  std::list<JSON_Expression *> &all_stacks = stack_list->Value_list();
  for (auto one_stack : all_stacks) {
    if (strcmp(one_stack->GetValue("filename")->Value_char(),
	       fits_path_full.c_str()) == 0) {
      // yes, exists. 
      this_stack = one_stack;
      break;
    }
  }

  juid_t this_juid = 0;
  bool already_exists = false;

  // go through all the images in the stack to calculate exposure
  // midpoint, exposure time, and airmass
  double sum_midpoint {0.0};
  double sum_exposures {0.0};
  double sum_airmass {0.0};
  const char *chart {nullptr};
  int N = constituent_juids.size();
  for (juid_t input : constituent_juids) {
    JSON_Expression *image_exp = this->FindByJUID(input);
    sum_midpoint  += image_exp->Value("julian")->Value_double();
    sum_exposures += image_exp->Value("exposure")->Value_double();
    sum_airmass   += image_exp->Value("airmass")->Value_double();
    JSON_Expression *chart_exp = image_exp->Value("chart");
    if (chart_exp) chart = chart_exp->Value_char();
  }

  if (this_stack == nullptr) {
    already_exists = false;
    this_stack = new JSON_Expression(JSON_SEQ);
    this_stack->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT,
							    "filename",
							    strdup(fits_path_full.c_str())));
    this_stack->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT,
							    "target",
							    target_object));
    this_stack->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT,
							    "filter",
							    filter));
    this_juid = juid.GetNextJUID(DB_STACKS);
    this_stack->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT,
							    "juid",
							    this_juid));
    this_stack->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT,
							    "directive",
							    directive));
  } else {
    already_exists = true;
    JSON_Expression *value = this_stack->Value("juid");
    if (!value) value = this_stack->Value("JUID");
    this_juid = value->Value_int();
  }
  //
  // At this point, "this_stack" and "this_juid" are valid, and an
  // entry exists.
  //
  this_stack->InsertUpdateTSTAMPInSeq();

  // UPDATE "chart"
  JSON_Expression *t_exp = this_stack->Value("chart");
  if (t_exp == nullptr) {
    this_stack->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT,
							    "chart",
							    chart));
  }
  // UPDATE airmass
  t_exp = this_stack->FindAssignment("airmass");
  if (t_exp == nullptr) {
    this_stack->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT,
							    "airmass",
							    sum_airmass/N));
  } else {
    t_exp->ReplaceAssignment(new JSON_Expression(JSON_FLOAT,
						 sum_airmass/N));
  }
  // UPDATE exposure
  t_exp = this_stack->FindAssignment("exposure");
  if (t_exp == nullptr) {
    this_stack->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT,
							    "exposure",
							    sum_exposures/N));
  } else {
    t_exp->ReplaceAssignment(new JSON_Expression(JSON_FLOAT,
						 sum_exposures/N));
  }
  // UPDATE JulianDay
  t_exp = this_stack->FindAssignment("julian");
  if (t_exp == nullptr) {
    this_stack->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT,
							    "julian",
							    sum_midpoint/N));
  } else {
    t_exp->ReplaceAssignment(new JSON_Expression(JSON_FLOAT,
						 sum_midpoint/N));
  }
  

  // The source files either go into the "source:" list or into the
  // "included:" list depending on whether the bool argument
  // (filenames_are_actual) is set.
  const char *filename_keyword = (filenames_are_actual ? "included" : "source");
  // See if there's an existing "source" assignment...
  JSON_Expression *source = this_stack->FindAssignment(filename_keyword);
  if (source) {
    source->ReplaceAssignment(new JSON_Expression(JSON_LIST, constituent_juids));
  } else {
    this_stack->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT,
							    filename_keyword,
							    new JSON_Expression(JSON_LIST,
										constituent_juids)));
  }

  this_stack->Validate();
  if (not already_exists) {
    stack_list->AddToArrayEnd(this_stack);
  }
  return this_juid;
}

juid_t AstroDB::LookupExposure(const char *filename, const char *section) {
  std::filesystem::path fits_path_raw(filename);
  std::filesystem::path fits_path_full = std::filesystem::weakly_canonical(fits_path_raw);
  
  JSON_Expression *exp_list = al_exp.Value(section ? section : "exposures");
  if (not exp_list->IsList()) {
    fprintf(stderr, "AddExposure: exposure isn't list.\n");
    exp_list->Print(stderr);
    return -1;
  }

  std::list<JSON_Expression *> &exposures = exp_list->Value_list();
  for (auto exp : exposures) {
    const char *exp_filename = exp->GetValue("filename")->Value_char();
    if (strcmp(exp_filename, fits_path_full.c_str()) == 0) {
      JSON_Expression *value = exp->GetValue("juid");
      if (!value) value = exp->GetValue("JUID");
      juid_t target_juid = value->Value_int();
      return target_juid;
    }
  }
  if (section == nullptr) {
    return LookupExposure(filename, "stacks");
  } else {
    return (juid_t) 0;
  }
}

juid_t juid_root_values[] = { 1000000, // DB_SESSION
			      2000000, // DB_IMAGE
			      5000000, // DB_SET
			      3000000, // DB_ANALYSIS
			      4000000, // DB_INST_MAGS
			      7000000, // DB_DIRECTIVE
			      8000000, // DB_SUBMISSION
			      6000000, // DB_STACKS
};

struct JUID_Info {
  const char *top_level_name;
  bool requires_JUID;
  DB_Entry_t juid_type;
} JUIDinfo [] = {
	{ "session", true, DB_SESSION },
	{ "exposures", true, DB_IMAGE },
	{ "stacks", true, DB_STACKS },
	{ "inst_mags", true, DB_INST_MAGS },
	{ "analyses", true, DB_ANALYSIS },
	{ "directives", true, DB_DIRECTIVE },
	{ "submissions", true, DB_SUBMISSION },
	{ "sets", true, DB_SET },
};

DB_Entry_t GetJUIDType(juid_t juid) {
  for (unsigned int i=0; i < (sizeof(juid_root_values)/sizeof(juid_t)); i++) {
    if (juid/1000000 == juid_root_values[i]/1000000) return (DB_Entry_t) i;
  }
  std::cerr << "GetJUIDType(" << juid << ") failed. Type not found.\n";
  return (DB_Entry_t) -1;
}
    
int
SubtreeFindLargestJUID(JSON_Expression &exp) {
  if (exp.IsEmpty() or
      exp.IsInt() or
      exp.IsDouble() or
      exp.IsString()) return -1;

  if (exp.IsAssignment()) {
    if (strcmp(exp.Assignment_variable(), "juid") == 0 ||
	strcmp(exp.Assignment_variable(), "JUID") == 0) {
      if (not exp.GetAssignment().IsInt()) {
	// silently ignore; maybe should generate message??
	return -1;
      } else {
	return exp.GetAssignment().Value_int();
      }
    } else {
      return -1;
    }
    /*NOTREACHED*/
  } else if (exp.IsList()) {
    juid_t list_max = -1;
    for (auto s : exp.Value_list()) {
      juid_t this_max = SubtreeFindLargestJUID(*s);
      if (this_max > list_max) list_max = this_max;
    }
    return list_max;
  } else if (exp.IsSeq()) {
    juid_t seq_max = -1;
    for (auto s : exp.Value_seq()) {
      juid_t this_max = SubtreeFindLargestJUID(*s);
      if (this_max > seq_max) seq_max = this_max;
    }
    return seq_max;
  } else {
    fprintf(stderr, "SubtreeFindLargestJUID: fall-through!\n");
    return -1;
  }
  /*NOTREACHED*/
}

void
JUID::Initialize(JSON_Expression &exp) {
  if (not exp.IsSeq()) {
    exp.Validate();
    fprintf(stderr, "JUID: top-level expression not sequence.\n");
    return;
  }

  for (int i=0; i<DB_NUM_JUID_TYPES; i++) {
    next_juid[i] = -1;
  }
  
  for (auto s:exp.Value_seq()) {
    if (not s->IsAssignment()) {
      exp.Validate();
      fprintf(stderr, "JUID: found non-assignment in top-level.\n");
      return;
    }

    const char *tgt = s->Assignment_variable();
    bool found_in_JUIDinfo = false;
    for (auto trial : JUIDinfo) {
      if (strcmp(tgt, trial.top_level_name) == 0) {
	juid_t max_juid = SubtreeFindLargestJUID(s->GetAssignment());
	fprintf(stderr, "INFO: JUID found for %s: %ld\n",
		tgt, max_juid);
	if (max_juid > next_juid[trial.juid_type]) {
	  next_juid[trial.juid_type] = max_juid+1;
	}
	found_in_JUIDinfo = true;
	break;
      }
    }
    if (not found_in_JUIDinfo) {
      fprintf(stderr, "JUID: top-level list %s not recognized.\n",
	      tgt);
    }
  }
  for (int i=0; i<DB_NUM_JUID_TYPES; i++) {
    if (next_juid[i] < 0) next_juid[i] = juid_root_values[i];;
  }

  fprintf(stderr, "JUID next values = [");
  for (int i=0; i<DB_NUM_JUID_TYPES; i++) {
    fprintf(stderr, "%ld, ", next_juid[i]);
  }
  fprintf(stderr, "]\n");
}

juid_t
JUID::GetNextJUID(DB_Entry_t which_JUID_type) {
  return next_juid[which_JUID_type]++;
}

// ... an interesting helper function. This is normally invoked by
// passing in the filename of an image file. It will return a string
// that can be passed as an argument to the AstroDB constructor if an
// AstroDB file is in the same directory as the image file. Otherwise
// it will return nullptr.
const char *HasAstroDBInDirectory(const char *image_filename) {
  const char *s = image_filename;

  // find the last '/' in the filename
  char dir_buffer[strlen(image_filename) + 24];
  const char *last_slash = nullptr;
  while(*s) {
    if (*s == '/') last_slash = s;
    s++;
  }

  // if not found, then image_filename has no directory information
  if (last_slash == nullptr) {
    strcpy(dir_buffer, "./astro_db.json");
  } else {
    char *d = dir_buffer;
    for (s=image_filename; s != last_slash; s++) {
      *d++ = *s;
    }

    strcpy(d, "/astro_db.json");
  }
  struct stat statbuf;
  if (stat(dir_buffer, &statbuf)) {
    // not found
    return nullptr;
  } else {
    return strdup(dir_buffer);
  }
}
  
//****************************************************************
//        AddInstMags()
//****************************************************************
juid_t
AstroDB::AddInstMags(juid_t source_exposure,
		     const char *filter,
		     juid_t directive,
		     const char *method,	   // "aperture"
		     const char *uncty_technique,  // "snr"
		     std::list<InstMagMeasurement> &mags) {
  JSON_Expression *mag_list = al_exp.Value("inst_mags");
  
  if (not mag_list->IsList()) {
    fprintf(stderr, "AddInstMags: inst_mags isn't list.\n");
    mag_list->Print(stderr);
    return -1;
  }

  // There may already be an inst mag entry in the database. If so, we
  // create a new one with the exact same juid and delete the existing
  // entry with that juid.
  juid_t orig_juid = InstMagsForJUID(source_exposure);

  JSON_Expression *new_seq = new JSON_Expression(JSON_SEQ);
  juid_t this_juid = (orig_juid ? orig_juid : juid.GetNextJUID(DB_INST_MAGS));
  if (orig_juid) {
    DeleteEntryForJUID(orig_juid);
  }
  new_seq->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT,
						       "juid",
						       this_juid));
  if (directive) {
    new_seq->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT,
							 "directive",
							 directive));
  }
  new_seq->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT,
						       "method",
						       method));
  new_seq->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT,
						       "uncty_technique",
						       uncty_technique));
  new_seq->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT,
						       "filter",
						       filter));
  new_seq->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT,
						       "exposure",
						       (long) source_exposure));
  new_seq->InsertUpdateTSTAMPInSeq();

  // Get JD, exp_time of the exposure
  {
    JSON_Expression *host_image = this->FindByJUID(source_exposure);
    if (host_image == nullptr) {
      fprintf(stderr, "AddInstMags(): source exposure not found in astro_db: %ld\n",
	      source_exposure);
      return 0;
    }
    const char *base_filename = host_image->Value("filename")->Value_char();
    Image image(base_filename);
    ImageInfo *info = image.GetImageInfo();

    JULIAN timetag = info->GetExposureMidpoint();
    new_seq->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT,
							 "jd",
							 timetag.day()));
    new_seq->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT,
							 "exp_time",
							 (double) info->GetExposureDuration()));
    if (info->AirmassValid()) {
      new_seq->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT,
							   "airmass",
							   info->GetAirmass()));
    }
  }

  JSON_Expression *new_exp = new JSON_Expression(JSON_LIST);
  for (const InstMagMeasurement &one_mag : mags) {
    // sequence for one mag
    JSON_Expression *new_mag = new JSON_Expression(JSON_SEQ);
    new_mag->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT,
							 "name",
							 one_mag.star_id));
    new_mag->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT,
							 "imag",
							 one_mag.inst_mag));
    new_mag->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT,
							 "uncty",
							 one_mag.uncertainty));
    new_mag->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT,
							 "airmass",
							 one_mag.airmass));
    
    new_exp->AddToArrayEnd(new_mag);
  }

  new_seq->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT,
						       "measurements",
						       new_exp));
  mag_list->AddToArrayEnd(new_seq);

  new_seq->InsertUpdateTSTAMPInSeq();

  return this_juid;
}

//****************************************************************
//        AddPSF
//****************************************************************
void
AstroDB::AddPSF(juid_t inst_mags_juid,
		double par1,
		double par2) {
  JSON_Expression *mag_list = al_exp.Value("inst_mags");
  
  if (not mag_list->IsList()) {
    fprintf(stderr, "AddPSF: inst_mags isn't list.\n");
    mag_list->Print(stderr);
    return;
  }

  JSON_Expression *exp = FindByJUID(inst_mags_juid);
  JSON_Expression *psf1_exp = exp->Value("psf_1");
  JSON_Expression *psf2_exp = exp->Value("psf_2");
  if (psf1_exp == 0) {
    // no existing PSF
    exp->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT,
						     "psf_1",
						     par1));
    exp->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT,
						     "psf_2",
						     par2));
  } else {
    // PSF already exists
    psf1_exp->ReplaceAssignment(new JSON_Expression(JSON_FLOAT, par1));
    psf2_exp->ReplaceAssignment(new JSON_Expression(JSON_FLOAT, par2));
  }
  exp->InsertUpdateTSTAMPInSeq();
}

//****************************************************************
//        AddDiffMags()
//****************************************************************
juid_t
AstroDB::AddDiffMags(juid_t source_set,
		     juid_t directive,
		     std::list<DiffMagMeasurement> &mags) {
  JSON_Expression *mag_list = al_exp.Value("analyses");
  std::list<DiffMagProfile *> profile_list;
  
  if (not mag_list->IsList()) {
    fprintf(stderr, "AddDiffMags: analyses list isn't list.\n");
    mag_list->Print(stderr);
    return -1;
  }

  // Create the profile_list
  for (auto mag : mags) {
    bool found = false;
    for (auto p : profile_list) {
      if (strcmp(p->profile_name, mag.profile->profile_name) == 0) {
	found = true;
	break;
      }
    }
    if (not found) {
      profile_list.push_back(mag.profile);
    }
  }

  // There may already be a diff mag entry in the database. If so, we
  // create a new one with the exact same juid and delete the existing
  // entry with that juid.
  juid_t orig_juid = DiffPhotForJUID(source_set);

  JSON_Expression *new_seq = new JSON_Expression(JSON_SEQ);
  juid_t this_juid = (orig_juid ? orig_juid : juid.GetNextJUID(DB_ANALYSIS));
  if (orig_juid) {
    DeleteEntryForJUID(orig_juid);
  }
  
  new_seq->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT,
						       "juid",
						       this_juid));
  if (directive) {
    new_seq->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT,
							 "directive",
							 directive));
  }
  std::list<juid_t> s_list { source_set };
  JSON_Expression *source_as_list = new JSON_Expression(JSON_LIST, s_list);
  new_seq->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT,
						       "source",
						       source_as_list));
  new_seq->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT,
						       "atype",
						       "DIFF"));
  new_seq->InsertUpdateTSTAMPInSeq();

  JSON_Expression *prof_exp_list = new JSON_Expression(JSON_LIST);
  new_seq->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT,
						       "profile",
						       prof_exp_list));

  for (auto p : profile_list) {
    JSON_Expression *profile_exp = new JSON_Expression(JSON_SEQ);

    profile_exp->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT,
							     "name",
							     p->profile_name));
    profile_exp->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT,
							     "julian",
							     p->julian));
    profile_exp->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT,
							     "technique",
							     p->technique));
    profile_exp->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT,
							     "filter",
							     p->filter));
    profile_exp->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT,
							     "exposure",
							     p->exposure_time));
    profile_exp->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT,
							     "airmass",
							     p->airmass));
    profile_exp->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT,
							     "target",
							     p->target));
    profile_exp->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT,
							     "crefmag",
							     p->crefmag));
    profile_exp->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT,
							     "check_rms",
							     p->check_rms));
    profile_exp->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT,
							     "chartid",
							     p->chartID));
    profile_exp->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT,
							     "comp",
							     *p->comp_star_names));
    profile_exp->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT,
							     "checks",
							     *p->check_star_names));
    prof_exp_list->AddToArrayEnd(profile_exp);
  }

  // list of the results (DiffMagMeasurements)
  JSON_Expression *new_exp = new JSON_Expression(JSON_LIST);
  for (const DiffMagMeasurement &mag : mags) {
    // sequence for one star
    JSON_Expression *new_mag = new JSON_Expression(JSON_SEQ);
    new_mag->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT,
							 "name",
							 mag.star_id));
    new_mag->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT,
							 "mag",
							 mag.diff_mag));
    new_mag->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT,
							 "uncty/stddev",
							 mag.uncertainty));
    new_mag->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT,
							 "uncty/snr",
							 mag.uncty_snr));
    new_mag->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT,
							 "numvals",
							 mag.num_vals));
    new_mag->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT,
							 "profile",
							 mag.profile->profile_name));
    new_exp->AddToArrayEnd(new_mag);
  }
  new_seq->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT,
						       "results",
						       new_exp));
  mag_list->AddToArrayEnd(new_seq);
  return this_juid;
}

JSON_Expression *
AstroDB::FindByJUID(juid_t juid) {
  return FindMaybeDeleteByJUID(juid, false);
}

// if you request the deletion, this returns the JSON expression for
// the item described by the JUID (a Sequence of assignments) and
// unlinks the sequence from the list in the AstroDB. It returns a
// pointer to the (unlinked) sequence of assignments.
JSON_Expression *
AstroDB::FindMaybeDeleteByJUID(juid_t juid, bool do_delete) {
  juid_t juid_root = 1000000*(juid/1000000);
  const char *top_level_name = nullptr;
  
  for (unsigned int i=0; i<sizeof(JUIDinfo)/sizeof(JUIDinfo[0]); i++) {
    if (juid_root_values[JUIDinfo[i].juid_type] == juid_root) {
      top_level_name = JUIDinfo[i].top_level_name;
    }
  }

  if (top_level_name == nullptr) {
    fprintf(stderr, "FindByJUID: juid value of %ld not recognized.\n", juid);
    return nullptr;
  }

  JSON_Expression *search_tree = al_exp.Value(top_level_name);
  if (not search_tree->IsList()) {
    fprintf(stderr, "FindByJUID: search tree isn't a list.\n");
    return nullptr;
  }

  std::list<JSON_Expression *> &candidates = search_tree->Value_list();
  for (auto item : candidates) {
    JSON_Expression *value = item->GetValue("juid");
    if (! value) value = item->GetValue("JUID");
    juid_t candidate_juid = value->Value_int();
    if (candidate_juid == juid) {
      if (do_delete) {
	// unlink the item from the list
	search_tree->DeleteFromArray(item);
      }
      return item;
    }
  }
  return nullptr;
}

std::list<JSON_Expression *> &
AstroDB::FetchAllOfType(DB_Entry_t which_type) {
  for (unsigned int i=0; i<sizeof(JUIDinfo)/sizeof(JUIDinfo[0]); i++) {
    if (JUIDinfo[i].juid_type == which_type) {
      const char *type_string = JUIDinfo[i].top_level_name;
      JSON_Expression *requested_list = al_exp.Value(type_string);
      if (requested_list == 0) {
	static std::list<JSON_Expression *> empty_list;
	return empty_list;
      } 
      return requested_list->Value_list();
    }
  }
  fprintf(stderr, "FetchAllOfType: failed to find type %d\n", which_type);
  static std::list<JSON_Expression *> empty_list;
  return empty_list;
}

juid_t
AstroDB::CreateEmptyDirective(juid_t new_juid) {
  JSON_Expression *directive_list = al_exp.Value("directives");
  if (not directive_list->IsList()) {
    fprintf(stderr, "CreateEmptyDirective(): directive list isn't list.\n");
    directive_list->Print(stderr);
    return -1;
  }

  JSON_Expression *new_seq = new JSON_Expression(JSON_SEQ);
  juid_t this_juid = (new_juid < 0 ? juid.GetNextJUID(DB_DIRECTIVE) : new_juid);

  new_seq->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT,
						       "juid",
						       this_juid));
  new_seq->InsertUpdateTSTAMPInSeq();
  directive_list->AddToArrayEnd(new_seq);
  return this_juid;
}
//****************************************************************
//        Class DB_Measurement
//****************************************************************
DB_Measurement::DB_Measurement(AstroDB &astro_db,
			       const char *target) : target_name(target),
						     host_db(astro_db) {
  astro_db.Reactivate();
  this->this_directive = astro_db.CreateEmptyDirective();
  astro_db.SyncAndRelease();
}

void
DB_Measurement::AddExposure(const char *fits_filename,
			    const char *filter,
			    JULIAN midpoint,
			    double exposure_time,
			    double airmass,
			    const char *chartname,
			    bool needs_dark,
			    bool needs_flat) {
  host_db.Reactivate();
  juid_t this_juid = host_db.AddExposure(fits_filename,
					 this->target_name,
					 filter,
					 this->this_directive,
					 midpoint,
					 exposure_time,
					 airmass,
					 chartname,
					 needs_dark,
					 needs_flat);
  host_db.SyncAndRelease();
  this->exposure_list.push_back(std::pair<const char *,juid_t>(filter,this_juid));
}

char *
AstroDB::BaseDirectory(void) {
  char base_dir[64];
  sprintf(base_dir, "/home/IMAGES/%s", working_date);
  return strdup(base_dir);
}

//****************************************************************
//        Close
// This must allow for the possibility that the stack already exists.
//****************************************************************
juid_t
DB_Measurement::Close(bool include_stack) {
  std::list<const char *> all_filters;
  std::list<juid_t> bvri_input;
  for (auto p : exposure_list) {
    const char *this_filter = p.first;
    bool found = false;
    for (auto f : all_filters) {
      if (strcmp(f, this_filter) == 0) {
	found = true;
	break;
      }
    }

    if (not found) {
      all_filters.push_back(this_filter);
    }
  }

  host_db.Reactivate();
  // For each filter, create SubExp set and (perhaps) a stack
  for (auto filter : all_filters) {
    std::list<juid_t> subexposures;
    for (auto p : exposure_list) {
      const char *this_filter = p.first;
      juid_t this_juid = p.second;

      if (strcmp(this_filter, filter) == 0) {
	subexposures.push_back(this_juid);
      }
    }

    juid_t color_juid = host_db.AddSubexpSet(filter, this->this_directive, subexposures);
    bvri_input.push_back(color_juid);
    if(include_stack) {
      char *base_dir = host_db.BaseDirectory(); // needs to be free'd later
      char stack_filename[132];
      Filter f(filter);
      sprintf(stack_filename, "%s/%s_%s.fits",
	      base_dir, target_name, f.CanonicalNameOf());
      free(base_dir);
      juid_t juid = host_db.AddRefreshStack(f.CanonicalNameOf(),
					    this->this_directive,
					    target_name,
					    stack_filename,
					    subexposures,
					    false /*not actuals; these
						    are planned*/);
      std::list<juid_t> stack_juid_list { juid };
      juid_t sub_juid = host_db.AddSubexpSet(f.CanonicalNameOf(),
					     this->this_directive, stack_juid_list);
      bvri_input.push_back(sub_juid);
    }
  }

  if (bvri_input.size() > 1) {
    return host_db.AddBVRISet(bvri_input, this->this_directive);
  }
  host_db.SyncAndRelease();

  if (bvri_input.size() == 0) {
    std::cerr << "DB_Measurment::Close(): No filters found!!\n";
    return 0;
  }
  return bvri_input.front();
}

// If the specified image has instrumental mags, the juid of the
// instrumental mags will be returned, otherwise, 0 is returned.
juid_t
AstroDB::InstMagsForJUID(juid_t image_juid) {
  // should be either a stack or an image
  // Look for an inst mag that links back to this image_juid
  JSON_Expression *mag_list = al_exp.Value("inst_mags");

  std::list<JSON_Expression *> &candidates = mag_list->Value_list();
  for (auto item : candidates) {
    JSON_Expression *value = item->GetValue("exposure");
    juid_t candidate_juid = value->Value_int();
    if (candidate_juid == image_juid) {
      // "item" is a seq of assignments
      JSON_Expression *juid_value = item->GetValue("juid");
      if (!juid_value) juid_value = item->GetValue("JUID");
      return juid_value->Value_int();
    }
  }
  return 0;
}

juid_t
// Note: the "instmags_juid" must be a set, either a SubExp or Merge 
AstroDB::DiffPhotForJUID(juid_t instmags_juid) {
  // Look for a diff phot that links back to this image_juid
  JSON_Expression *mag_list = al_exp.Value("analyses");

  std::list<JSON_Expression *> &candidates = mag_list->Value_list();
  for (auto item : candidates) {
    JSON_Expression *value = item->GetValue("source")->Value_list().front();
    juid_t candidate_juid = value->Value_int();
    if (candidate_juid == instmags_juid) {
      // "item" is a seq of assignments
      JSON_Expression *juid_value = item->GetValue("juid");
      if (!juid_value) juid_value = item->GetValue("JUID");
      return juid_value->Value_int();
    }
  }
  return 0;
}

void
AstroDB::DeleteEntryForJUID(juid_t item_to_delete) {
  (void) FindMaybeDeleteByJUID(item_to_delete, true);
}

// Locking
// Default state is LOCKED
// the std::pair<int,int> in lock_stack contains
// the region_id in .first and the state in .second
// (state == 0 means release, state == 1 means lock)
//
// host_db.SyncAndRelease(); initiates a release
// host_db.Reactivate(); initiates a lock
int
AstroDB::BeginLockRegion(void) {
  const int id = lock_stack.size();
  lock_stack.push_back({id, DB_LOCK});
  if (current_state != DB_LOCK) {
    Reactivate();
    current_state = DB_LOCK;
  }
  return id;
}

void
AstroDB::EndLockRegion(int id) {
}

int
AstroDB::BeginReleaseRegion(void) {
  const int id = lock_stack.size();
  lock_stack.push_back({id, DB_RELEASE});
  if (current_state != DB_RELEASE) {
    SyncAndRelease();
    current_state = DB_RELEASE;
  }
  return id;
}

void
AstroDB::EndReleaseRegion(int id) {
}

//****************************************************************
//        CreateNewTarget()
//    It's not an error if one already exists
//****************************************************************
juid_t
AstroDB::CreateNewTarget(const char *target_name) {
  JSON_Expression *set_list = al_exp.Value("sets");
  
  if (not set_list->IsList()) {
    fprintf(stderr, "CreateNewTarget: set_list isn't list.\n");
    set_list->Print(stderr);
    return -1;
  }

  // iterate through the list of sets
  std::list<JSON_Expression *> &sets = set_list->Value_list();
  for (auto set : sets) {
    const char *stype = set->GetValue("stype")->Value_char();
    if (strcmp("TARGET", stype) == 0) {
      const char *tgt_name = set->GetValue("target")->Value_char();
      if (strcmp(tgt_name, target_name) == 0) {
	juid_t juid = set->GetValue("juid")->Value_int();
	return juid;
      }
    }
  }

  // nothing preexisting, need to create
  JSON_Expression *new_exp = new JSON_Expression(JSON_SEQ);
  new_exp->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT,
						       "target",
						       strdup(target_name)));
  new_exp->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT,
						       "stype",
						       "TARGET"));
  juid_t this_juid = juid.GetNextJUID(DB_SET);
  new_exp->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT,
						       "juid",
						       this_juid));
  JSON_Expression *empty_list = new JSON_Expression(JSON_LIST);
  new_exp->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT,
						       "input",
						        empty_list));

  new_exp->InsertUpdateTSTAMPInSeq();

  set_list->AddToArrayEnd(new_exp);
  return this_juid;
}

void
AstroDB::AddJUIDToTarget(juid_t target_set, juid_t new_member) {
  JSON_Expression *exp = FindByJUID(target_set);
  if (!exp) {
    fprintf(stderr, "ERROR: AddJUIDToTarget: target set %ld not found.\n",
	    target_set);
    return;
  }

  JSON_Expression *input_list = exp->GetValue("input");
  if (!input_list) {
    fprintf(stderr, "ERROR: AddJUIDToTarget: target set %ld has no input assignment.\n",
	    target_set);
    return;
  }

  JSON_Expression *new_exp = new JSON_Expression(JSON_INT, new_member);
  input_list->AddToArrayEnd(new_exp);
}
