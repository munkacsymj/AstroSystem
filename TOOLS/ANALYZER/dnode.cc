/*  dnode.cc -- Managed dependencies within astro_db
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

#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>		// kill()
#include <signal.h>		// kill()
#include <unistd.h>		// unlink()

#include <iostream>
#include <ostream>
#include <list>
#include <algorithm>

#include <Image.h>
#include <IStarList.h>

#include "dnode.h"

//        CONCURRENCY RULES
// All methods/functions in here fall into one of three categories:
//    1. Stuff associated with the creation of the dependency
// tree. All of this assumes that the AstroDB is locked while they do
// their thing.
//    2. The Satisfy() method (which is actually about three, all with
// names that are some variant of "satisfy"). This is a recursive
// function.
//    3. The DoSomeAction() methods. These have a side-effect of
// changing the AstroDB disk file, and require a re-sync after the
// external program does its thing.

DNodeType DB2DNode(DB_Entry_t type) {
  switch(type) {
  case DB_SESSION:   break;
  case DB_IMAGE:     return DN_Image;
  case DB_SET:       return DN_Set;
  case DB_ANALYSIS:  return DN_Analysis;
  case DB_INST_MAGS: return DN_Inst_Mag;
  case DB_SUBMISSION: return DN_Submission;
  case DB_STACKS:    return DN_Stack;
  default:
    break;
  }

  std::cerr << "DB2DNode: cannot convert type "
	    << type << std::endl;
  return DN_Stack;		// value doesn't matter; error return
}

class DCommand {
public:
  DCommand(std::string command);
  ~DCommand(void);
  void Execute(void);
  std::string &GetCommand(void) { return command_txt; }
private:
  std::string command_txt;
  int return_value;
};

DCommand::DCommand(std::string command) : command_txt(command) {
  ;
}

DCommand::~DCommand(void) {;}

void
DCommand::Execute(void) {
  std::cerr << "Executing command: " << GetCommand() << std::endl;
  this->return_value = system(command_txt.c_str());
}

std::list<DCommand> pending_commands;
void ScheduleCommand(const char *string) {
  fprintf(stderr, "New Command: %s\n", string);
  pending_commands.push_back(std::string(string));
}

// return true if there were commands to execute
bool ExecuteCommands(void) {
  bool return_value = false;
  for (DCommand &c : pending_commands) {
    c.Execute();
    return_value = true;
  }
  pending_commands.clear();
  return return_value;
}

// returns TRUE if any node in dependencies has been updated after
// this node was last updated.
bool
DNode::DNodeTimestampIsStale(std::list<DNode *> &dependencies) {
  time_t dependency_update_time = MostRecentTimestamp(dependencies);
  
  return dependency_update_time > this->timestamp;
}

DNodeTree::DNodeTree(AstroDB &astro_db,
		     const char *analysis_technique)
  : analysis_tech(analysis_technique),
    host_db(astro_db) {
  this->RebuildEntireTree();
}

//****************************************************************
//        Concurrency Control
//****************************************************************
void
DNodeTree::ReleaseDatabase(void) {
  this->host_db.SyncAndRelease();
}

bool
DNodeTree::ReSyncDatabase(void) {
  bool anything_changed;
  this->host_db.Reactivate(&anything_changed);
  if (anything_changed) {
    this->RebuildEntireTree();
  }
  return anything_changed;
}

void
DNodeTree::RebuildEntireTree(void) {
  // All existing DNodes go away
  all_images.clear();
  all_sets.clear();
  all_analyses.clear();
  all_inst_mags.clear();
  all_stacks.clear();
  for (auto node : this->all_nodes) {
    delete node;
  }
  all_nodes.clear();

  BuildSubtree(DB_IMAGE, all_images, juid_lookup);
  BuildSubtree(DB_SET, all_sets, juid_lookup);
  BuildSubtree(DB_ANALYSIS, all_analyses, juid_lookup);
  BuildSubtree(DB_INST_MAGS, all_inst_mags, juid_lookup);
  BuildSubtree(DB_SUBMISSION, all_submissions, juid_lookup);
  BuildSubtree(DB_STACKS, all_stacks, juid_lookup);
    
  all_nodes.insert(all_nodes.end(), all_images.begin(), all_images.end());
  all_nodes.insert(all_nodes.end(), all_sets.begin(), all_sets.end());
  all_nodes.insert(all_nodes.end(), all_analyses.begin(), all_analyses.end());
  all_nodes.insert(all_nodes.end(), all_inst_mags.begin(), all_inst_mags.end());
  all_nodes.insert(all_nodes.end(), all_stacks.begin(), all_stacks.end());
  all_nodes.insert(all_nodes.end(), all_submissions.begin(), all_submissions.end());

  BuildDependencies();
}

void
DNodeTree::BuildSubtree(DB_Entry_t which_type,
			std::list<DNode *> &exp_list,
			std::unordered_map<long, DNode *> &juid_lookups) {
  std::list<JSON_Expression *> &json_list = host_db.FetchAllOfType(which_type);

  DNodeType this_type = DB2DNode(which_type);
  for (auto item : json_list) {
    DNode *new_node = new DNode(item, this_type, this);
    exp_list.push_back(new_node);
    if (new_node->juid) {
      juid_lookups[new_node->juid] = new_node;
    }
  }
}

void
DNodeTree::BuildDependencies(void) {
  for (auto node : this->all_nodes) {
    node->predecessors.clear();
    node->successors.clear();
    node->sidecars.clear();
    node->dirty = false;
  }
  
  for (auto node : this->all_nodes) {
    DNodeType &node_type = node->node_type;
      
    // PREDECESSORS
    const char *list_source_pri = nullptr; // looking for a list
    const char *list_source_sec = nullptr;
    const char *simple_source = nullptr; // looking for a juid
    bool is_sidecar = false;
    if (node_type == DN_Stack) {
      list_source_pri = "included";
      list_source_sec = "source";
    }
    if (node_type == DN_Set)      list_source_pri = "input";
    if (node_type == DN_Inst_Mag) {
      simple_source = "exposure"; is_sidecar = true;
    }
    if (node_type == DN_Analysis) {
      simple_source = "source"; is_sidecar = true;
    }
    if (node_type == DN_Submission) {
      simple_source = "analysis"; is_sidecar = true;
    }

    if (simple_source) {
      JSON_Expression *analy_assign = node->json->GetValue(simple_source);
      if (analy_assign) {
	juid_t juid = analy_assign->Value_int();
	auto existing_DNode = juid_lookup.find(juid);
	if (existing_DNode == juid_lookup.end()) {
	  std::cerr << "BuildDependencies: JUID " << juid
		    << " is unknown.\n";
	} else {
	  node->predecessors.push_back(existing_DNode->second);
	  if (is_sidecar) {
	    existing_DNode->second->sidecars.push_back(node);
	  }
	}
      }
    }

    if (list_source_pri) {
      JSON_Expression *set_exp = node->json->GetValue(list_source_pri);
      if (set_exp == nullptr) {
	set_exp = node->json->GetValue(list_source_sec);
      }
      std::list<JSON_Expression *> &list = set_exp->Value_list();
      for (auto item : list) {
	juid_t juid = item->Value_int();
	auto existing_DNode = juid_lookup.find(juid);
	if (existing_DNode == juid_lookup.end()) {
	  // Lookup failed: JUID is unknown
	  std::cerr << "BuildDependencies: JUID "
		    << juid
		    << " is unknown.\n";
	} else {
	  node->predecessors.push_back(existing_DNode->second);
	  if (is_sidecar) {
	    existing_DNode->second->sidecars.push_back(node);
	  }
	}
      }
    }
  }

  int link_count = 0;
  for (auto node : this->all_nodes) {
    // SUCCESSORS
    for (auto predecessor : node->predecessors) {
      predecessor->successors.push_back(predecessor);
      link_count++;
    }
  }
  std::cerr << "BuildDependencies: "
	    << link_count
	    << " dependencies found and mapped.\n";

  // Propagate target names down throughout the dependency tree, so
  // that each DNode has the name of the ultimate target it is
  // contributing to.
  for (auto t : this->all_sets) {
    if (strcmp(t->json->GetValue("stype")->Value_char(), "TARGET") == 0) {
      const char *t_name = t->json->GetValue("target")->Value_char();

      for (auto pred : t->predecessors) {
	pred->PropagateTargetDown(t_name);
      }
    }
  }
}

void
DNode::PropagateTargetDown(const char *target, int depth) {
  if (depth > 10) {
    std::cerr << "PropagateTargetDown depth = " << depth << '\n';
    std::cerr << "Current JUID = " << this->juid << '\n';
    for (auto item : this->predecessors) {
      std::cerr << "   Predecessor = " << item->juid << '\n';
    }
    for (auto item : this->sidecars) {
      std::cerr << "   Sidecar = " << item->juid << '\n';
    }
    kill(0, SIGABRT);
  }
  
  // Stop pushing target name down if we encounter a new target name
  if (this->node_type == DN_Set and
      strcmp(this->json->GetValue("stype")->Value_char(), "TARGET") == 0) {
    return;
  }
  this->ultimate_target = target;
  for (auto item : this->predecessors) {
    if (item->ultimate_target == nullptr) {
      item->PropagateTargetDown(target, depth+1);
    }
  }
  for (auto item : this->sidecars) {
    if (item->ultimate_target == nullptr) {
      item->PropagateTargetDown(target, depth+1);
    }
  }
}


#if 0
void
DNode::Refresh(JSON_Expression *exp) {
  // only thing we can trust at this point is the juid
  if (exp == nullptr) {
    JSON_Expression *new_json = this->parent_tree->host_db.FindMaybeDeleteByJUID(juid, false);
    this->delete_pending = (new_json == nullptr);
    this->json = new_json;
  } else {
    this->json = exp;
  }
  // A refresh will NEVER change a node's type or parent_tree
  if (node_type == DN_Stack ||
      node_type == DN_Inst_Mag ||
      node_type == DN_Submission ||
      node_type == DN_Analysis) {
    JSON_Expression *time_exp = exp->GetValue("tstamp");
    if (time_exp) {
      this->timestamp = time_exp->Value_int();
    } else {
      std::cerr << "DNode: missing timestamp.\n";
    }
  }
  
  
}
#endif

DNode::DNode(JSON_Expression *exp, DNodeType exp_type, DNodeTree *parent) {
  json = exp;
  node_type = exp_type;
  parent_tree = parent;

  // JUID
  JSON_Expression *juid_exp = exp->GetValue("juid");
  if (juid_exp) {
    this->juid = juid_exp->Value_int();
  } else {
    std::cerr << "ERROR: DNode: Node has no JUID.\n";
    this->juid = 0;
  }

  // TIMESTAMP
  this->timestamp = 0;
  if (node_type == DN_Stack ||
      node_type == DN_Inst_Mag ||
      node_type == DN_Submission ||
      node_type == DN_Analysis) {
    JSON_Expression *time_exp = exp->GetValue("tstamp");
    if (time_exp) {
      this->timestamp = time_exp->Value_int();
    } else {
      std::cerr << "DNode: missing timestamp. JUID = "
		<< this->juid << '\n';
    }
  }
}

DNodeTree::~DNodeTree(void) {
  for (auto x : all_nodes) {
    delete x;
  }
}

DNode *
DNodeTree::FindTarget(const char *target) {
  for (auto t : this->all_sets) {
    if (strcmp(t->json->GetValue("stype")->Value_char(), "TARGET") == 0) {
      const char *t_name = t->json->GetValue("target")->Value_char();
      if (strcmp(t_name, target) == 0) {
	return t;
      }
    }
  }
  return nullptr;
}

DNode *
DNodeTree::JUIDLookup(juid_t juid) {
  auto existing_DNode = juid_lookup.find(juid);
  if (existing_DNode == juid_lookup.end()) {
    // Lookup failed: JUID is unknown
    std::cerr << "JUIDLookup: JUID " << juid << " is unknown.\n";
    return nullptr;
  } else {
    return (existing_DNode->second);
  }  
}

//****************************************************************
//        The Action Functions:
//    DoNeedStars()
//    DoNeedStack()
//    DoBVRI()
//    DoDiffPhot()
//    DoInstMags()
//    DoMerge()
//****************************************************************

void DoFindStars(const char *filename,
		 const char *darkname,
		 const char *flatname) {
  char command[256];
  if (darkname) {
    sprintf(command, "find_stars -f -d %s -i %s",
	    darkname, filename);
  } else {
    sprintf(command, "find_stars -f -i %s", filename);
  }
  ScheduleCommand(command);
}

void DoStarMatch(const char *filename, const char *starname) {
  char command[256];
  sprintf(command, "star_match -n %s -b -h -f -e -i %s",
	  starname, filename);
  ScheduleCommand(command);
}

int
DNode::DoNeedStars(bool force_update) {
  std::cerr << "DoNeedStars(" << juid << ")"
	    << (force_update ? " [forced]" : "") << ":\n";
  if (this->satisfied) {
    std::cerr << "    (okay to skip; already checked.)\n";
    return 0;
  }
  this->satisfied = true;
  bool updated = false;
  
  auto *flatnode = this->json->GetValue("flat");
  auto *darknode = this->json->GetValue("dark");
  auto *imagenode = this->json->GetValue("filename");
  const char *target = this->json->GetValue("target")->Value_char();

  // imagenode is req'd. flatnode and darknode are optional.
  const char *flatname = nullptr;
  if (flatnode) flatname = flatnode->Value_char();
  const char *darkname = nullptr;
  if (darknode) darkname = darknode->Value_char();
  const char *imagename = nullptr;
  if (imagenode) {
    imagename = imagenode->Value_char();
  }
  
  //Image image(imagename);
  //IStarList *starlist = image.PassiveGetIStarList();
  IStarList *starlist = new IStarList(imagename);
  bool need_star_match = true;
  if (starlist) {
    for (int i=0; i<starlist->NumStars; i++) {
      if (starlist->FindByIndex(i)->validity_flags & CORRELATED) {
	need_star_match = false;
	break;
      }
    }
  }
  if (starlist == nullptr or starlist->NumStars <= 4 or need_star_match or force_update) {
    std::cerr << "    Invoking DoFindStars(" << imagename << ")\n";
    DoFindStars(imagename, darkname, flatname);
    need_star_match = true;
  } else {
    std::cerr << "    (stars already available.)\n";
  }
  if (need_star_match) {
    std::cerr << "    Invoking DoStarMatch(" << imagename << ")\n";
    DoStarMatch(imagename, target);
    updated = true;
  } else {
    std::cerr << "    (star_match already completed.)\n";
  }
  return updated;
}
  

int
DNode::DoNeedStack(const char *stackname,
		   std::list<DNode *> &image_nodes) {
  if (this->satisfied) return this->dirty;
  this->satisfied = true;
  
  if (image_nodes.size() == 0) {
    std::cerr << "Warning: DNodeTree:DoNeedStack(): stack needs no images.\n";
    return false;
  }

  const char *flatname = image_nodes.front()->json->GetValue("flat")->Value_char();
  const char *darkname = image_nodes.front()->json->GetValue("dark")->Value_char();

  // remove excluded images from the list of images to be stacked
  std::list<DNode *> images (image_nodes);
  JSON_Expression *directive_exp = this->json->Value("directive");
  if (directive_exp) {
    juid_t directive_juid = directive_exp->Value_int();
    JSON_Expression *exp = this->parent_tree->host_db.FindByJUID(directive_juid);
    JSON_Expression *excl_exp = exp->Value("stack_excl");
    if (excl_exp) {
      std::list<JSON_Expression *> &excl_list_exp = excl_exp->Value_list();
      for (JSON_Expression *e : excl_list_exp) {
	juid_t one_to_exclude = e->Value_int();
	bool erased = false;

	for (std::list<DNode *>::iterator it = images.begin(); it != images.end(); it++) {
	  if ((*it)->json->GetValue("juid")->Value_int() == one_to_exclude) {
	    images.erase(it);
	    erased = true;
	    break;
	  }
	}
	if (not erased) {
	  std::cerr << "DoNeedStack: exclusion juid not found in stack list: "
		    << one_to_exclude << std::endl;
	}
      }
    }
  }

  // build the stack command
  int sum_filename_lengths = 0;
  for (auto n : images) {
    const char *iname = n->json->GetValue("filename")->Value_char();
    sum_filename_lengths += strlen(iname);
    n->dirty |= n->DoNeedStars(false);	// force_update not needed here
  }

  this->dirty = true;
  char *command = (char *) malloc(256 + sum_filename_lengths);
  if (!command) {
    std::cerr << "Error: DoNeedStack(): malloc failed.\n";
    return true;
  }
  sprintf(command, "stack -o %s -d %s -s %s ",
	  stackname, darkname, flatname);
  for (auto n : images) {
    const char *iname = n->json->GetValue("filename")->Value_char();
    strcat(command, iname);
    strcat(command, " ");
  }

  // insert an assignment that lists the actual images stacked
  std::list<juid_t> included_juid;
  for (DNode *d : images) {
    included_juid.push_back(d->json->GetValue("juid")->Value_int());
  }
  JSON_Expression *included_list = new JSON_Expression(JSON_LIST,
						       included_juid);
  JSON_Expression *actual_exp = this->json->Value("included");
  if (actual_exp == nullptr) {
    this->json->InsertAssignmentIntoSeq(new JSON_Expression(JSON_ASSIGNMENT,
							    "included",
							    included_list));
  } else {
    this->json->ReplaceAssignment("included", included_list);
  }

  ScheduleCommand(command);
  free(command);
  this->DoNeedStars(false);	// gonna happen anyway
  //DoStarMatch(stackname, this->json->GetValue("target")->Value_char());
  return true;
}

int
DNode::DoInstPhotometry(juid_t phot_source) {
  std::cerr << "DoInstPhotometry(" << phot_source << "). building cmd...\n";
  DNode *source = parent_tree->JUIDLookup(phot_source);
  // source can be either an image or a stack
  if (!source) {
    std::cerr << "Error: DoInstPhotometry(): phot_source doesn't exist.\n";
    return false;
  }

  // see if this is an excluded image
  JSON_Expression *directive_exp = this->json->Value("directive");
  if (directive_exp) {
    juid_t directive_juid = directive_exp->Value_int();
    JSON_Expression *exp = this->parent_tree->host_db.FindByJUID(directive_juid);
    JSON_Expression *excl_exp = exp->Value("img_analy_excl");
    if (excl_exp) {
      std::list<JSON_Expression *> &excl_list_exp = excl_exp->Value_list();
      for (JSON_Expression *e : excl_list_exp) {
	if (e->Value_int() == phot_source) {
	  // yes, excluded
	  return this->dirty;
	}
      }
    }
  }

  this->dirty = true;
  auto *flatnode = source->json->GetValue("flat");
  auto *darknode = source->json->GetValue("dark");
  auto *imagenode = source->json->GetValue("filename");

  // imagenode is req'd. flatnode and darknode are optional.
  const char *flatname = nullptr;
  if (flatnode) flatname = flatnode->Value_char();
  const char *darkname = nullptr;
  if (darknode) darkname = darknode->Value_char();
  const char *imagename = nullptr;
  if (imagenode) {
    imagename = imagenode->Value_char();
  } else {
    std::cerr << "Error: DoInstPhotometry(): source image not specified.\n";
    return this->dirty;
  }

  std::string command("photometry ");
  command += " -i ";
  command += imagename;
  if (flatname) {
    command += " -s ";
    command += flatname;
  }
  if (darkname) {
    command += " -d ";
    command += darkname;
  }
  ScheduleCommand(command.c_str());
  return true;
}

int
DNode::DoDiffPhot(std::list<DNode *> &image_nodes, juid_t set_node) {
  // arguments:
  //    -d /home/IMAGES/xxx/astro_db.json
  //    -s juid -- this is the juid of the set the diff phot gets attached to
  //    -i juid -- (multiple), the juid of the image nodes
  //    -c starname -- (multiple), comp star names
  //
  char command[40+79+image_nodes.size()*15];
  char target[32] {""};
  if (set_node) {
    sprintf(target, " -s %ld ", set_node);
  }
  sprintf(command, "do_diff_phot -d %s %s ", this->parent_tree->host_db.AstroDBPathname(), target);

  // see which images are excluded
  std::list<juid_t> excluded_images;
  JSON_Expression *directive_exp = image_nodes.front()->json->Value("directive");
  if (directive_exp) {
    juid_t directive_juid = directive_exp->Value_int();
    JSON_Expression *exp = this->parent_tree->host_db.FindByJUID(directive_juid);
    JSON_Expression *excl_exp = exp->Value("img_analy_excl");
    if (excl_exp) {
      std::list<JSON_Expression *> &excl_list_exp = excl_exp->Value_list();
      for (JSON_Expression *e : excl_list_exp) {
	excluded_images.push_back(e->Value_int());
      }
    }
  }

  for (auto i : image_nodes) {
    // Find the inst mags for this image
    std::list<juid_t>::iterator it;
    it = std::find(excluded_images.begin(), excluded_images.end(), i->juid);
    if (it == excluded_images.end()) {
      char source[15];
      sprintf(source, " -i %ld ", i->juid);
      strcat(command, source);
    }
  }
  this->dirty = true;
  ScheduleCommand(command);
  return this->dirty;
}

int
DNode::DoBVRIAnalysis(const char *target_name) {
  char command[132];
  sprintf(command, "../../BIN/do_bvri -d %s -t %s ",
	  this->parent_tree->host_db.BaseDirectory(),
	  target_name);
  ScheduleCommand(command);
  return true;
}

void
DNode::DoMergeMags(std::list<DNode *> &phot_sets, juid_t target_juid) {
  char command[15*2+64];
  char target[15] {""};

  if (target_juid) {
    sprintf(target, " -o %ld ", target_juid);
  }
  sprintf(command, "do_merge %s ", target);
  for (DNode * input : phot_sets) {
    char source[15];
    sprintf(source, " -i %ld ", input->juid);
    strcat(command, source);
  }
  ScheduleCommand(command);
}

//****************************************************************
//        Satisfy() methods
//****************************************************************

void
DNodeTree::SatisfyTarget(const char *target, // okay to provide '*'
			 bool force_update) {
  if (strcmp(target, "*") == 0) {
    // User wants *all* targets satisfied. Loop through everything
    std::list<char *> all_targets;
    for (auto t : this->all_sets) {
      if (strcmp(t->json->GetValue("stype")->Value_char(), "TARGET") == 0) {
	const char *t_name = t->json->GetValue("target")->Value_char();
	all_targets.push_back(strdup(t_name));
      }
    }

    for (const char *t : all_targets) {
      fprintf(stderr, "Processing target %s\n", t);
      SatisfyTarget(t, force_update);
    }
  } else {
    DNode *tgt_node = FindTarget(target);
    if (tgt_node) {
      SatisfyTarget(tgt_node, force_update);
    } else {
      fprintf(stderr, "Target %s not defined.\n", target);
    }
  }
}

void
DNodeTree::SatisfyTarget(DNode *target, bool force_update) {
  bool anything_changed;
  this->host_db.Reactivate(&anything_changed);
  if (anything_changed) {
    this->RebuildEntireTree();
  }
  
  for (DNode *n : all_nodes) {
    n->satisfied = false;
  }
  
  do {
    // This will populate the list of commands to be issued.
    target->Satisfy(0, force_update);

    fprintf(stderr, "Commands to execute:\n");
    for (DCommand &c : pending_commands) {
      fprintf(stderr, "%s\n", c.GetCommand().c_str());
    }
    // Release lock
    this->host_db.SyncAndRelease();
    ExecuteCommands();
    bool anything_changed;
    this->host_db.Reactivate(&anything_changed);
    break;
  } while(pending_commands.size());

}

// Needed things:
// DoNeedStars(filename):
//    - forward declaration
//    - must handle darks/flats (safe temporary image filename)
// arguments passed to DoNeedStack()
// DoNeedStack()
//
// Need target starname in order to kick off star_match

void
DNode::Satisfy(int level, bool force_update) {
  // General approach: recursively Satisfy() all prerequisites to this
  // DNode, then do anything inherent in this node itself, then
  // Satisfy() all sidecars to this node as a final step.
  if (this->satisfied) return;
  //this->satisfied = true;
  {
    int i=0;
    while(i++ < level) std::cerr << "   ";

    std::cerr << "Satisfy(" << this->GetNodeTypename()
	      << ": " << this->juid << ")\n";
  }

  bool any_predecessor_dirty = false;
  std::cerr << "Checking predecessors of " << this->juid << "\n";
  for (auto p : this->predecessors) {
    p->Satisfy(level+1, force_update);
    std::cerr << "     " << p->juid << (p->dirty ? " dirty ":" unchanged ")
	      << " for target " << this->juid << '\n';
    any_predecessor_dirty |= p->dirty;
  }
  
  
  switch(this->node_type) {
  case DN_Image:
    {
      bool updated = this->DoNeedStars(force_update);
      this->dirty |= updated;
      // if no inst_mag sidecar, then clearly need inst mags
      if (updated || this->sidecars.size() == 0 || force_update) {
	this->dirty |= this->DoInstPhotometry(this->juid);
      }
    }
    break; 

  case DN_Stack:
    // Does stack file exist?
    {
      const char *stack_filename = this->json->GetValue("filename")->Value_char();
      if ((not FileExists(stack_filename)) or
	  force_update or
	  any_predecessor_dirty or
	  MostRecentTimestamp(this->predecessors) > this->timestamp) {
	// need to re-stack
	this->dirty |= DoNeedStack(stack_filename, this->predecessors);
      }
      bool updated = false;
      if (FileExists(stack_filename)) {
	updated = this->DoNeedStars(force_update);
	this->dirty |= updated;
      }
      // if no inst mag sidecar, then clearly need inst mags
      bool need_inst_update = updated or this->sidecars.size() == 0 or
	force_update or any_predecessor_dirty;
      if (need_inst_update) {
	this->DoInstPhotometry(this->juid);
      }
    }
    break;

  case DN_Inst_Mag:
    {
      const juid_t image_juid = this->json->GetValue("exposure")->Value_int();
      if (force_update or any_predecessor_dirty or
	  this->parent_tree->FileTimestampByJUID(image_juid) > this->timestamp or
	  this->parent_tree->JUIDLookup(image_juid)->timestamp > this->timestamp) {
	this->dirty |= this->DoInstPhotometry(image_juid);
      }
    }
    break;
    
  case DN_Analysis:
  case DN_Submission:
    this->dirty |= any_predecessor_dirty;
    break;

  case DN_Set:
    // which type of Set??
    switch(GetSetType()) {
    case ST_BVRI:
      if (any_predecessor_dirty or sidecars.size() == 0) {
	this->dirty |= DoBVRIAnalysis(this->ultimate_target);
      }
      break;
      
    case ST_SubExp:
#if 0 // ----------------- TURNED OFF
      {
	// should be at most a single sidecar
	if (sidecars.size() > 1) {
	  std::cerr << "Satisfy(SubExp): more than one sidecar!\n";
	  break;
	}
	AstroDB &astro_db = this->parent_tree->host_db;
	#juid_t diff_phot_juid = astro_db.DiffPhotForJUID(this->juid);
	time_t inst_phot_timestamp = MostRecentTimestamp(this->predecessors);
	#bool diff_phot_needs_update = true;
	if (diff_phot_juid > 0) {
	  time_t diff_phot_timestamp = this->parent_tree->JUIDLookup(diff_phot_juid)->timestamp;
	  if (diff_phot_timestamp > inst_phot_timestamp) {
	    diff_phot_needs_update = false;
	  }
	}

	if (diff_phot_needs_update) {
	  DoDiffPhot(this->predecessors, this->juid);
	}
      }
#endif
      break;

    case ST_Merge:
      {
	juid_t merge_juid = 0;
	if (sidecars.size() > 1) {
	  std::cerr << "Satisfy(Merge): more than one sidecar!\n";
	  break;
	} else if (sidecars.size() > 0) {
	  merge_juid = sidecars.front()->juid;
	}
	DoMergeMags(this->predecessors, merge_juid);
      }
      break;
      
    case ST_Target:
    case ST_TimeSeq:
      for (auto t : this->predecessors) {
	t->Satisfy(level+1, force_update);
      }
      break;
    }
    
    break;


  default:
    std::cerr << "SatisfyTarget(): invalid target node type: "
	      << this->node_type << std::endl;
  }
  this->satisfied = true;
  
  // Now handle any sidecars
  for (auto sidecar : sidecars) {
    std::cerr << "handling sidecare of " << juid
	      << " (" << sidecar->juid << ")\n";
    sidecar->Satisfy(level+1, (force_update or this->dirty));
  }
  if (sidecars.size() == 0) {
    std::cerr << "(info): " << juid << " has no sidecars.\n";
  }
}

const char *
DNode::GetNodeTypename(void) {
  switch(this->node_type) {
  case DN_Image: return "Image";
  case DN_Stack: return "Stack";
  case DN_Submission: return "Submission";
  case DN_Inst_Mag: return "InstMag";
  case DN_Set: return this->json->GetValue("stype")->Value_char();
  case DN_Analysis: return "Analysis";
  default:
    return "InvalidNodeType";
  }
}

SetType
DNode::GetSetType(void) {
  if (this->node_type != DN_Set) {
    std::cerr << "GetSetType(): Node isn't of type set: "
	      << this->node_type << std::endl;
    exit(2);
  }

  const char *settypename = this->json->GetValue("stype")->Value_char();
  if (strcmp(settypename, "BVRI") == 0) return ST_BVRI;
  if (strcmp(settypename, "SUBEXP") == 0) return ST_SubExp;
  if (strcmp(settypename, "MERGE") == 0) return ST_Merge;
  if (strcmp(settypename, "TARGET") == 0) return ST_Target;
  if (strcmp(settypename, "TIMESEQ") == 0) return ST_TimeSeq;
  std::cerr << "GetSetType(): Set has unknown type: "
	    << settypename << std::endl;
  exit(2);
  __builtin_unreachable(); /*NOTREACHED*/
}


time_t MostRecentTimestamp(std::list<DNode *> &predecessors) {
  time_t answer = (time_t) 0;
  for (auto p : predecessors) {
    if (p->timestamp > answer) answer = p->timestamp;
  }
  return answer;
}

time_t FileTimestamp(const char *filename) {
  struct stat stat_buf;
  int ret = stat(filename, &stat_buf);
  if (ret) {
    return (time_t) 0;
  }

  return stat_buf.st_mtime;
}

time_t
DNodeTree::FileTimestampByJUID(juid_t juid) {
  auto existing_DNode = juid_lookup.find(juid);
  if (existing_DNode == juid_lookup.end()) {
    // Lookup failed: JUID is unknown
    std::cerr << "JUIDLookup in FileTimestamp: JUID " << juid << " is unknown.\n";
    return 0;
  } else {
    DNode *target_node = existing_DNode->second;
    const char *filename = target_node->json->GetValue("filename")->Value_char();
    return FileTimestamp(filename);
  }
}

  

bool FileExists(const char *filename) {
  struct stat stat_buf;
  int ret = stat(filename, &stat_buf);
  return (not ret);
}
  
  
    
