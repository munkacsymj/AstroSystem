/* This may look like C code, but it is really -*-c++-*- */
/*  dnode.h -- Provides a dependency tree for Astro_DB.
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
#ifndef _DNODE_H
#define _DNODE_H

#include <list>
#include <unordered_map>
#include <json.h>
#include <astro_db.h>

enum DNodeType {
  DN_Image,
  DN_Stack,
  DN_Inst_Mag,
  DN_Set,
  DN_Analysis,
  DN_Submission,
};

enum SetType {
  ST_BVRI,
  ST_SubExp,
  ST_Merge,
  ST_Target,
  ST_TimeSeq,
};

class DNodeTree;

// The dependency structure is contained in the contents of the
// predecessors, successors, and sidecars attached to each DNode. 

class DNode {
 public:
  DNode(JSON_Expression *exp, DNodeType exp_type, DNodeTree *parent);
  ~DNode(void) {;}
  JSON_Expression *json;
  time_t timestamp;
  juid_t juid;
  std::list<DNode *> predecessors;
  std::list<DNode *> successors;
  std::list<DNode *> sidecars;	// these are the attached analyses, etc.

  void Satisfy(bool force_update);
  SetType GetSetType(void); // only valid for DNodeType == DN_Set
  const char *GetNodeTypename(void);

  // returns TRUE if any node in dependencies has been updated after
  // this node was last updated.
  bool DNodeTimestampIsStale(std::list<DNode *> &dependencies);

  // This should be invoked after a resync() returns a true value for
  // anything_changed. 
  //void Refresh(JSON_Expression *exp = nullptr);  

 private:
  DNodeType node_type;
  bool satisfied;
  friend class DNodeTree;
  int DoNeedStack(const char *stackname,
		   std::list<DNode *> &image_nodes);
  // phot_source is either an image or a stack
  int DoInstPhotometry(juid_t phot_source);
  int DoDiffPhot(std::list<DNode *> &image_nodes, juid_t target_juid);
  int DoBVRIAnalysis(const char *target_name);
  void DoMergeMags(std::list<DNode *> &phot_sets, juid_t target_juid);
  // returns nonzero if stars are being updated
  int DoNeedStars(bool force_update);
  void Satisfy(int level, bool force_update);

  // All DNodes are "children" of the top-level DNodeTree. All DNodes
  // point back to the parent DNodeTree. 
  DNodeTree *parent_tree;

  // This is normally of no interest. However, during a refresh cycle,
  // it will be set true if it's known that this DNode needs to go
  // away (the underlying JUID has been dropped).
  bool delete_pending;

  // This flag is used to identify nodes that will be changed as a
  // result of commands that have been put into the queue. Anything
  // that depends on this node should also have an update command
  // scheduled.
  bool dirty {false};

  // This is sometimes used as part of issuing commands
  const char *ultimate_target {nullptr};
  void PropagateTargetDown(const char *target, int depth=0);
};

struct Target {
  const char *target_name;
  DNode *target_node;
};

// We normally deal with a single DNodeTree. Even though
// "BuildSubtree()" makes it sound like the concept of a tree is
// recursive, it really isn't. This class only applies at the top-most
// level. 

class DNodeTree {
 public:
  DNodeTree(AstroDB &astro_db, const char *analysis_technique);
  ~DNodeTree(void);

  DNode *FindTarget(const char *target);
  void SatisfyTarget(const char *target, bool force_update); // okay to provide '*'
  void SatisfyTarget(DNode *target, bool force_update);
  DNode *JUIDLookup(juid_t juid);
  time_t FileTimestampByJUID(juid_t juid);
  
 private:
  const char *analysis_tech;
  AstroDB &host_db;
  std::list<DNode *> all_nodes;
  std::list<DNode *> all_images;
  std::list<DNode *> all_sets;
  std::list<DNode *> all_analyses;
  std::list<DNode *> all_inst_mags;
  std::list<DNode *> all_submissions;
  std::list<DNode *> all_stacks;

  std::list<Target> all_targets;

  std::unordered_map<long, DNode *> juid_lookup;
  
  void BuildSubtree(DB_Entry_t which_type,
		    std::list<DNode *> &exp_list,
		    std::unordered_map<long, DNode *> &juid_lookups);
  void BuildDependencies(void);

  void ReleaseDatabase(void);
  bool ReSyncDatabase(void); // return value true == change occurred
  void RebuildEntireTree(void);
  
  friend DNode;
};

time_t MostRecentTimestamp(std::list<DNode *> &predecessors);
time_t FileTimestamp(const char *filename);
bool FileExists(const char *filename);

#endif
