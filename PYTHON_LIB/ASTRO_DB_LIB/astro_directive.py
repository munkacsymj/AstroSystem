#!/usr/bin/python3
import sys
import traceback

from ..IMAGE_LIB import filter
from .util import FindJUID

class Directive:
    def __init__(self, db, directive):
        self.directive = directive
        self.db = db
        self.juid = directive['juid']

    def ExclFromStackingJUID(self):
        if 'stack_excl' in self.directive:
            return self.directive['stack_excl']
        else:
            return []

    # Returns a list of starnames
    def FetchExclusionStars(self, color, keyword):
        #traceback.print_stack()
        if keyword not in self.directive:
            return []
        raw_list = self.directive[keyword]
        #print('first raw_list = ', raw_list)
        for x in raw_list:
            # x['name'] == 'GSC01234-1234' (typical)
            # x['filter'] == 'B,V' (typical)
            if 'filter' in x and isinstance(x['filter'],str):
                raw_filter = x['filter']
                x['filter'] = [filter.to_canonical[f] for f in raw_filter.split(',')]
        #print('final raw list = ', raw_list)

        excl_list_names = [x['name'] for x in raw_list
                           if 'filter' not in x or color in x['filter']]
        #print('excl_list_names = ', excl_list_names)
        return excl_list_names

    # Returns a list of starnames
    def FetchEnsembleExclusions(self, color):
        return self.FetchExclusionStars(color, 'ensemble_excl')

    # Returns a list of starnames
    def FetchCheckExclusions(self, color):
        return self.FetchExclusionStars(color, 'check_excl')

    # Returns a boolean (or None)
    def UseEnsemble(self):
        if 'use_ensemble' in self.directive:
            return bool(self.directive['use_ensemble'])
        else:
            return True # default if know nothing else

    # Returns a boolean (or None)
    def DoTransform(self):
        if 'do_transform' in self.directive:
            return bool(self.directive['do_transform'])
        else:
            return None

    # Returns a list of canonical filter names
    def ColorsToExclude(self):
        if 'color_excl' not in self.directive:
            return []
        return [filter.to_canonical[x] for x in self.directive['color_excl']]

    # Returns a list of JUIDs from 'exposures': images to exclude from analysis
    def ImagesToExclude(self):
        if 'img_analy_excl' not in self.directive:
            return []
        return self.directive['img_analy_excl']

    # Returns a dictionary of (image_juid, threshold)
    def FindThresholds(self):
        if 'find_threshold' not in self.directive:
            return []
        return self.directive['find_threshold']

    # which_excl is one of "both", "ensemble_excl", "check_excl"
    def JSONToExpandedExclusions(self, which_excl="both"):
        if which_excl == "both":
            self.JSONToExpandedExclusions("ensemble_excl")
            self.JSONToExpandedExclusions("check_excl")
        else:
            if which_excl == "ensemble_excl":
                self.expanded_excl_ens = {}
                target = self.expanded_excl_ens
            else:
                self.expanded_excl_check = {}
                target = self.expanded_excl_check
            if which_excl in self.directive:
                src = self.directive[which_excl]
                for x in src: # "x" is a dictionary; must contain "name" and might contain "filter"
                    starname = x['name']
                    if 'filter' not in x:
                        filterlist = standard_colors
                    else:
                        filterlist = x['filter']
                    for f in filterlist:
                        if f not in target:
                            target[f] = []
                        target[f].append(starname)
            print('Target = ', target)

    def ExpandedExclusionsToJSON(self, which_excl='both'):
        if which_excl == 'both':
            self.ExpandedExclusionsToJSON('ensemble_excl')
            self.ExpandedExclusionsToJSON('check_excl')
        else:
            if which_excl in self.directive:
                del self.directive[which_excl]
            if which_excl == 'ensemble_excl':
                src = self.expanded_excl_ens
            else:
                src = self.expanded_excl_check

            tgt = []
            if len(src) > 0:
                self.directive[which_excl] = tgt
                for (filter,starlist) in src.items():
                    for starname in starlist:
                        this_star = next((x for x in tgt if x['name'] == starname),None)
                        if this_star == None:
                            this_star = {"name" : starname, "filter" : []}
                            tgt.append(this_star)
                        this_star['filter'].append(filter)
            
            print('Target = ', self.directive)

    # type_of_exclusion: "ensemble_excl" or "check_excl"
    # filter: canonical filter name. If None, then starlist is excluded for all filters
    # starlist: list of catalog star names
    def SetStarExclusions(self, type_of_exclusion, filter, starlist):
        self.JSONToExpandedExclusions(type_of_exclusion)
        if type_of_exclusion == 'ensemble_excl':
            src = self.expanded_excl_ens
        else:
            src = self.expanded_excl_check
        src[filter] = starlist
        self.ExpandedExclusionsToJSON(type_of_exclusion)
            
    def SetUseEnsemble(self, use_ensemble):
        self.directive['use_ensemble'] = int(use_ensemble)

    def SetColorExclusions(self, exclude_color_list):
        if exclude_color_list is None or len(exclude_color_list) == 0:
            if 'color_excl' in self.directive:
                del self.directive['color_excl']
        else:
            self.directive['color_excl'] = [filter.to_canonical[x] for x in exclude_color_list]

    def SetImageAnalyExclusions(self, exclude_juid_list):
        if exclude_juid_list is None or len(exclude_juid_list) == 0:
            if 'img_analy_excl' in self.directive:
                del self.directive['img_analy_excl']
        else:
            self.directive['img_analy_excl'] = exclude_juid_list
