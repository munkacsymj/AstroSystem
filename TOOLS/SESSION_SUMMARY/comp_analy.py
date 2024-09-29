import SessionGlobal
import history

import os
import sys
import gi
import statistics
import math
gi.require_version('Gtk', '3.0')
from gi.repository import Gtk as gtk

def ToolRoot(start):
    while True:
        (head,tail) = os.path.split(start)
        if tail == "TOOLS":
            return head
        elif tail == '':
            raise Exception("filepath does not contain TOOLS")
        else:
            start = head

sys.path.insert(1, ToolRoot(__file__))
from PYTHON_LIB.ASTRO_DB_LIB import astro_db,astro_directive
from PYTHON_LIB.IMAGE_LIB import star, moderole
from PYTHON_LIB.ASTRO_DB_LIB.util import FindJUID

overall_summary = None # Reference to singleton "OverallSummary" object
standard_colors = ['B','V','R','I']

def Quadrature(datalist):
    if datalist is None or len(datalist) == 0:
        return None
    return math.sqrt(sum((x*x for x in datalist))/len(datalist))

################################################################
# Three key classes:
#    OverallSummary: Holds the data that most external users of this
# module need. Contains a list of StarDisp objects.
#    StarDisp: Holds the data on one star (all
# filters/colors). Contains a dictionary of 4 StarDispFilter objects.
#    StarDispFilter: Holds the data on one star/filter combination
#
# The general approach is to create a StarDisp object for any star
# that might conceivably need to be displayed in the CompTAB graph,
# with enough attributes visible that CompTab can decide how (or even
# if) to display that particular star, given what the operator has set
# for display switches.
################################################################

class StarDispFilter:
    def __init__(self, filter, analysis_result, profile, cat_star):
        self.filter = filter
        self.cat_star = cat_star
        self.mag = analysis_result['mag'] # standard mag
        #print('\nStarDispFilter(',self.cat_star.name,'/',self.filter,
        #      '): profile = ', profile['pnum'], ', mag = ', self.mag)
        self.is_transformed = analysis_result['transformed'] # normally, True or False
        self.uncty_snr = None
        self.uncty_seq = None
        self.uncty_std = None
        uncty = analysis_result['uncty']
        uncty_src = analysis_result['uncty_src']
        if uncty_src == 'STDDEV':
            self.uncty_std = uncty
        elif uncty_src == 'Fitting':
            self.uncty_seq = uncty
        elif uncty_src == 'SNR':
            self.uncty_snr = uncty
        self.uncty_src = uncty_src
        self.uncty = uncty

        # self.method_string = "" # "S" (stack) or n (multi-images)
        self.stack = profile['stack']
        if self.stack:
            self.method_string = 'S'
            self.nobs = None
        else:
            self.nobs = profile['numvals']
            self.method_string = str(self.nobs)

        if filter in cat_star.ref_mag:
            (ref,ref_uncty) = cat_star.ref_mag[filter]
            self.mag_err = (ref-self.mag)
        else:
            self.mag_err = None     # if standard photometry available

        self.airmass = profile['airmass']
        if 'xform_info' in analysis_result:
            self.xform_info = analysis_result['xform_info']  # dictionary
        else:
            self.xform_info = None

        # string: primary color index used, e.g., "BV"
        if 'xform_method' in analysis_result:
            self.xform_method = analysis_result['xform_method']
        else:
            self.xform_method = None

    def Print(self):
        print('\nStarDispFilter(',self.cat_star.name,'/',self.filter,'):')
        print('mag = ', self.mag, ', uncty = ', self.uncty)

    def ToDispString(self):
        bvri_line = '<b>{mag:6.3f}</b>{transformed} {err} {uncty} {flags} '
        transformed = ' '
        if self.is_transformed:
            transformed = 't'
        uncty = '     '
        if self.uncty is not None:
            uncty = '{:5.3f}'.format(self.uncty)
        err = '      '
        if self.mag_err is not None:
            err = '{:6.3f}'.format(self.mag_err)
        flags = self.method_string + '/'

        if self.uncty_src == 'SNR':
            flags += 'SNR'
        elif self.uncty_src == 'STDDEV':
            flags += 'STD'
        elif self.uncty_src == 'Fitting':
            flags += 'ENS'
        else:
            flags += '   '

        #if self.nobs is not None:
        #   flags += '{:2}'.format(self.nobs)
        #else:
        #   flags += '  '

        return bvri_line.format(mag=self.mag,
                                transformed=transformed,
                                err=err,
                                uncty=uncty,
                                flags=flags)

class StarDisp:
    def __init__(self, cat_star):
        self.name = cat_star.name
        self.chartname = cat_star.report_name # might be "None"
        self.sort_mag = None # used when sorting by mag; color-dependent
        self.cat_star = cat_star
        self.mode_role = cat_star.mode_role
        self.star_flags = {} # index is filter, value is tuple:
                             # (is_comp, is_check, is_ensemble, used_comp,  used_check, used_ensemble)
        self.phot = {} # index is filter, value is StarDispFilter

    def Load(self, analysis):
        self.use_ensemble = (analysis['technique'] == 'ENSEMBLE')
        filters_in_use = self.GetFiltersInUse(analysis)
        for filter in filters_in_use:
            self.star_flags[filter] = self.GetStarFlags(analysis, filter)

        self.is_any_check = any((self.cat_star.IsCheck(f) for f in self.star_flags))
        self.is_any_comp = self.cat_star.is_comp_candidate
        self.is_any_ens = any((self.cat_star.IsEnsemble(f) for f in self.star_flags))
        self.is_any_submit = self.cat_star.do_submit
        self.is_comp = ('comp' in analysis and analysis['comp'] == self.name)

        # "results" is list of analysis structs for this star; should
        # be one per filter
        results = [x for x in analysis['results'] if x['name'] == self.name]
        for x in results:
            prof_num = x['profile']
            profile = next((x for x in analysis['profiles']
                            if x['pnum'] == prof_num), None)
            assert profile is not None
            filter = profile['filter']
            #print('@@@',x,profile,filter,self.cat_star.name)
            self.phot[filter] = StarDispFilter(filter, x, profile, self.cat_star)
        self.sort_mag = next((self.phot[x].mag for x in ['V','R','I','B'] if x in self.phot),None)

    def GetFiltersInUse(self, analysis):
        filters_in_use = set()
        filters_in_use = [x['filter'] for x in analysis['profiles']]
        return filters_in_use

    def GetStarFlags(self, analysis, filter):
        is_comp = self.cat_star.is_comp_candidate
        is_check = self.cat_star.IsCheck(filter)
        is_ensemble = self.cat_star.IsEnsemble(filter)

        profile = None
        for s in analysis['results']:
            if s['name'] == self.name:
                prof_num = s['profile']
                profile = next((x for x in analysis['profiles'] if x['pnum'] == prof_num), None)
                assert profile is not None
                if profile['filter'] == filter:
                    break
                profile = None
            
        used_comp = False
        # if this star name shows up in any of the ensemble lists in
        # this analysis for this color, then this star was used in an
        # ensemble
        if 'ensembles' in analysis:
            used_ensemble = any(self.name in p['errs']
                                for p in analysis['ensembles'] if p['filter'] == filter)
        else:
            used_ensemble = False
            used_comp = (self.name == analysis['comp'])
        # Same concept for check stars...
        used_check = False
        if filter in analysis['check_fit']:
            used_check = self.name in analysis['check_fit'][filter]['errs']
        
        return (is_comp, is_check, is_ensemble, used_comp,
                used_check, used_ensemble)

    def ToDispString(self):
        startype = ' '
        self.text_tag = None

        if self.is_any_check:
            startype = 'CHECK'
            self.text_tag = "check"
        if self.is_any_comp:
            startype = 'COMP'
            self.text_tag = "ensemble"
        if self.is_any_ens and self.use_ensemble:
            startype = 'ENS'
            self.text_tag = "ensemble"
        if self.is_any_submit:
            self.text_tag = "submit"
        if self.is_comp:
            self.text_tag = "comp"
            startype = 'COMP'

        if startype == 'ENS':
            print("ToDispString(",self.name,"): ", (self.use_ensemble,
                                                    self.is_any_check,
                                                    self.is_any_comp,
                                                    self.is_any_ens,
                                                    self.is_any_submit,
                                                    self.is_comp))

        chartname = 5*' '
        if self.chartname is not None:
            chartname = self.chartname.ljust(5)
        disp_string = self.name.ljust(15) + chartname + startype.ljust(6)

        xform_string = 11*' '
        for f in ['V','R','I','B']:
            if f in self.phot and self.phot[f].is_transformed:
                if self.phot[f].xform_info is not None:
                    delta_str = next((x for x in
                                      self.phot[f].xform_info if 'DELTA' in x),None)
                    if delta_str is not None:
                        color_string = delta_str.lower()[6:]
                        print("color_string = ", color_string)
                        xform_string = color_string[0] + '-' + color_string[1]
                        xform_string += ' {:6.3f} '.format(self.phot[f].xform_info[delta_str])
                        break

        standard_colors = ['B','V','R','I']
        color_string = {}
        for f in standard_colors:
            if f not in self.phot:
                color_string[f] = 27*' '
            else:
                color_string[f] = self.phot[f].ToDispString()

        disp_string = (disp_string + xform_string + ''.join(color_string[x] for x in
                                                            standard_colors) + '\n')
        return disp_string
                     
class OverallSummary:
    def __init__(self, target):
        self.bvri_string = None
        self.use_ensemble = True # will be overwritten later
        self.SetTarget(target)

    def SetBVRITab(self, bvri_tab):
        self.bvri_tab = bvri_tab

    def SetTarget(self, target):
        self.bvri_string = ""
        self.stars = {} # index is name, value is StarDisp
        self.target = target
        self.history = None

        if target is not None:
            print("OverallSummary: new SetTarget(): ", target)
            self.catalog = star.ReadCatalog(target)
            if target in history.history_by_target:
                self.history = history.history_by_target[target]
            self.Reload()
            self.ens_star_candidates = {} # index is color, value is list of starnames
            self.check_star_candidates = {} # index is color, value is list of starnames

            for f in standard_colors:
                self.ens_star_candidates[f] = [x.name for x in self.catalog
                                               if x.IsEnsemble(f)]
                self.check_star_candidates[f] = [x.name for x in self.catalog
                                                 if x.IsCheck(f)]
        else:
            print("OverallSummary: new SetTarget(): <None>")
            self.catalog = None
            SessionGlobal.db = None

    def Reload(self):
        global standard_colors
        print("OverallSummary.Reload() starting.")
        SessionGlobal.db_obj = astro_db.AstroDB(SessionGlobal.homedir)
        db = SessionGlobal.db_obj.GetData()
        SessionGlobal.db = db
        if db is None:
            return
        self.ens_history = {}
        self.check_history = {}
        self.ens_stars_active = {}     # index: filter, value: set(starname)
        self.check_stars_active = {}   # index: filter, value: set(starname)
        self.ens_stars_superset = {}   # index: filter, value: starname
        self.check_stars_superset = {} # index: filter, value: starname
        self.ensemble_rms = {}         # index by filter, value is rms error (or None)
        self.check_rms = {}            # same thing for check stars
        self.directive_juid = None
        for f in standard_colors:
            self.ens_stars_active[f] = set()
            self.check_stars_active[f] = set()
            self.ens_stars_superset[f] = set()
            self.check_stars_superset[f] = set()

        if self.target is None:
            return
        
        analy_list = [x for x in db['analyses'] if x['target'] == self.target]
        # It's an error if the target has no analysis
        if len(analy_list) == 0:
            if True:
                command = '../ANALYZER/analyzer '
                command += ' -t ' + self.target
                command += ' -d ' + SessionGlobal.homedir
                print("Running: ", command, flush=True)
                retval = os.system(command)
                print("^^^Command completed. Return value is ", retval)

            command = '../ANALYZER/do_bvri.py '
            command += ' -t ' + self.target
            command += ' -d ' + SessionGlobal.homedir
            command += ' > /tmp/do_bvri.out 2>&1 '

            print("Running: ", command)
            retval = os.system(command)
            print("^^^Command completed. Return value is ", retval)
            SessionGlobal.db_obj = astro_db.AstroDB(SessionGlobal.homedir)
            db = SessionGlobal.db_obj.GetData()
            SessionGlobal.db = db
            analy_list = [x for x in db['analyses'] if x['target'] == self.target]

            if len(analy_list) == 0:
                dialog = gtk.MessageDialog(
                    transient_for = SessionGlobal.root,
                    flags = 0,
                    message_type=gtk.MessageType.INFO,
                    buttons=gtk.ButtonsType.OK,
                    text="No analysis found in astro_db.json for this target.")
                dialog.format_secondary_text("Target = " + self.target)
                dialog.run()
                dialog.destroy()
                return
                        
        # It's also an error if the target has multiple analyses
        elif len(analy_list) > 1:
            dialog = gtk.MessageDialog(
                transient_for = SessionGlobal.root,
                flags = 0,
                message_type=gtk.MessageType.INFO,
                buttons=gtk.ButtonsType.OK,
                text="Multiple analyses found in astro_db.json for this target.")
            dialog.format_secondary_text(
                "Target = " + self.target)
            dialog.run()
            dialog.destroy()
            return
        # Otherwise we're good with a single analysis for this target
        a = analy_list[0]
        self.analysis = a

        directive_juid = self.analysis['directive']
        self.directive_juid = directive_juid
        self.directive_data = FindJUID(SessionGlobal.db, 'directives', directive_juid)
        self.use_ensemble = True
        excluded_checks = {}    # index: filter, value: starname
        excluded_ens = {}       # index: filter, value: starname
        if self.directive_data is not None:
            print("comp_analy: fetching directive for juid ", self.directive_juid)
            print("comp_analy: directive built from ", self.directive_data)
            self.directive = astro_directive.Directive(SessionGlobal.db, self.directive_data)
            self.use_ensemble = self.directive.UseEnsemble()
            for f in standard_colors:
                if self.use_ensemble:
                    excluded_ens[f] = self.directive.FetchEnsembleExclusions(f)
                excluded_checks[f] = self.directive.FetchCheckExclusions(f)
        else:
            self.directive = None

        if self.use_ensemble:
            print("+++comp_analy: excluded_ensembles = ", excluded_ens)
        print("+++comp_analy: excluded_checks = ", excluded_checks)

        current_mode = (moderole.ModeRole.MODE_ENS if self.bvri_tab.QueryUseEnsemble()
                        else moderole.ModeRole.MODE_COMP)
        
        if 'ensembles' in a:
            self.ensembles = a['ensembles']
        else:
            self.ensembles = None

        if 'ensemble_fit' in a:
            self.ensemble_fit = a['ensemble_fit']
        else:
            self.ensemble_fit = None

        if 'check_fit' in a:
            self.check_fit = a['check_fit']
        else:
            self.check_fit = None
                
        filters_in_use = set([x['filter'] for x in self.analysis['profiles']])
        starnamelist = set(x['name'] for x in a['results'])
        self.stars = {} # index is name, value is StarDisp
        for x in starnamelist:
            cat = star.CatalogLookupByName(self.catalog, x)
            if cat is None:
                print("comp_analy.py: starname not in catalog? (", x, ")")
            else:
                this_star_info = StarDisp(star.CatalogLookupByName(self.catalog, x))
                self.stars[x] = this_star_info
                this_star_info.Load(a)


        # The 'ensembles' section of the analysis lists the stars
        # actually used in each ensemble. We pull the union of those
        # lists in as the "ens_active" set. Note that this doesn't
        # touch the concept of a directive's exclusions list; that is
        # taken care of by do_bvri (so is already reflected in the
        # 'ensembles' section list).
        stdev_list = {}      # key is filter, value is list of stdev's
        if self.ensembles is not None:
            for section in self.ensembles:
                filter = section['filter']
                self.ens_stars_active[filter].update(set(section['errs'].keys()))
                if filter not in stdev_list:
                    stdev_list[filter] = []
                stdev_list[filter].append(section['stdev'])
        for filter in filters_in_use:
            self.ens_stars_superset[filter] = set([x.name for x in self.catalog
                                             if x.IsEnsemble(filter)])
            self.check_stars_superset[filter] = set([x.name for x in self.catalog
                                               if x.IsCheck(filter) and
                                               x.name not in self.ens_stars_active[filter]])

            # Now need to calculate rms error summaries for top of the
            # Fitting tab
            if len(stdev_list[filter]) > 0:
                sumsq = sum((x*x for x in stdev_list[filter]))
                self.ensemble_rms[filter] = math.sqrt(sumsq/len(stdev_list[filter]))

        if 'check_fit' in a:
            section = a['check_fit']
            for filter in section:
                check_errorlist = []
                check_exclusions = self.directive.FetchCheckExclusions(filter)
                for starname in section[filter]['errs'].keys():
                    if starname not in check_exclusions:
                        self.stars[starname].mode_role.Set(filter,
                                                           moderole.ModeRole.MODE_ALL,
                                                           moderole.ModeRole.CHECK_ACT)
                        check_errorlist.append(section[filter]['errs'][starname])
                        self.check_stars_active[filter].add(starname)
                if len(check_errorlist) > 0:
                    sumsq = sum((x*x for x in check_errorlist))
                    self.check_rms[filter] = math.sqrt(sumsq/len(check_errorlist))
                else:
                    self.check_rms[filter] = None

        #
        # Generate the BVRI tab text string
        #
        sort_list = list(self.stars.values())
        sort_list.sort(key = lambda s: s.sort_mag)
        
        self.bvri_string = 64*'#'+'\n'+32*' '+self.target+'\n'+64*'#'+4*'\n'
        self.bvri_string += 36*' '+'|'+3*(26*' '+'|')+'\n'
        self.bvri_string += ('Name          Chart Status  Color   |'+
                             '        BLUE              |'+
                             '        GREEN             |'+
                             '        RED               |'+
                             '        IR\n')
        self.bvri_string += (14*'-'+' ---- ----- ----------|'+
                             4*('------- ------ ----- -----|')+
                             '\n')
                             
        #self.bvri_string += 'Column headers go here\n'
        self.header_chars = len(self.bvri_string)
        line_no = 0
        for x in sort_list:
            x.line_no = line_no
            line_no += 1
            self.bvri_string += x.ToDispString()
        self.bvri_string += '\n'

        #print(self.bvri_string)

        self.GenerateSummaryData()
        SessionGlobal.notifier.trigger("comp_analy", "overall_summary", "value_change")

    def GetDisplayString(self):
        if self.bvri_string is None:
            return '[No data available]'
        return self.bvri_string

    def GenerateSummaryData(self):
        db = SessionGlobal.db
        self.ens_stdev = {}
        self.check_stdev = {}
        self.ens_history = {}
        self.check_history = {}

        ################################
        # Today's ensemble fit
        ################################
        if 'ensemble_fit' in self.analysis:
            ef = self.analysis['ensemble_fit']
            for (filter,data) in ef.items():
                if 'stdev' in data:
                    stdev = data['stdev']
                else:
                    stdev = None
                self.ens_stdev[filter] = stdev

        ################################
        # Today's check fit
        ################################
        if 'check_fit' in self.analysis:
            cf = self.analysis['check_fit']
            for (filter,data) in cf.items():
                if 'rms' in data:
                    stdev = data['rms']
                else:
                    stdev = None
                self.check_stdev[filter] = stdev

        ################################
        # Historical ensemble fit
        ################################
        if self.target in history.history_by_target:
            hist = history.history_by_target[self.target]
            all_stdev = {} # index is filter, value is list of "stdev" entries
            for session in hist: # session is a reference to a history.TargetHistory
                if session.ensemble_fit is None:
                    continue
                for (filter,data) in session.ensemble_fit.items():
                    if 'stdev' in data:
                        if filter not in all_stdev:
                            all_stdev[filter] = []
                        all_stdev[filter].append(data['stdev'])
            for (filter,datalist) in all_stdev.items():
                if datalist is not None and len([x for x in datalist if x is not None]) > 0:
                    self.ens_history[filter] = Quadrature(datalist)
        
        ################################
        # Historical check fit
        ################################
            all_stdev = {} # index is filter, value is list of "stdev" entries
            for session in hist:
                if session.check_fit is None:
                    continue
                for (filter,data) in session.check_fit.items():
                    if 'rms' in data:
                        if filter not in all_stdev:
                            all_stdev[filter] = []
                        all_stdev[filter].append(data['rms'])
            for (filter,datalist) in all_stdev.items():
                if datalist is not None and len([x for x in datalist if x is not None]) > 0:
                    self.check_history[filter] = Quadrature(datalist)

        print("comp_analy: check_stdev = ", self.check_stdev)
        print("comp_analy: check_history = ", self.check_history)
        

