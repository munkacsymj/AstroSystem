import pdb
import sys
import statistics

import context

sys.path.insert(1, '/home/mark/ASTRO/CURRENT')
from PYTHON_LIB.ASTRO_DB_LIB import astro_db

# A "source_list" is ALWAYS a list of PerImage references

def mean_jd(jd_list):
    jd_offset = 2450000.0
    return jd_offset + statistics.mean([x-jd_offset for x in jd_list])

next_profile_num = 0

class Profile:
    def __init__(self, source_list):
        global next_profile_num
        
        self.source = source_list
        self.julian    = mean_jd([x.exposure['julian'] for x in source_list])
        self.exposure  = sum([x.exposure_time for x in source_list])
        self.airmass   = statistics.mean([x.average_airmass for x in source_list])
        self.target    = source_list[0].target
        self.filter    = source_list[0].filter
        self.comps     = set()
        self.checks    = None
        self.check_rms = None
        self.profile_num = next_profile_num
        self.stack     = source_list[0].stack # boolean
        next_profile_num += 1

        self.comps = set()
        for source in source_list:
            self.comps.update([x.name for x in source.comp_stars])

    # Returns a dictionary
    def ToJSON(self):
        ret = {}
        ret['jd'] = self.julian
        ret['exp_time'] = self.exposure
        ret['airmass'] = self.airmass
        ret['filter'] = self.filter
        if self.comps is not None and len(self.comps) > 0:
            ret['comps'] = list(self.comps)
        if self.checks != None and len(self.checks) > 0:
            ret['checks'] = [x.name for x in self.checks]
        if self.check_rms != None:
            ret['check_rms'] = self.check_rms
        ret['pnum'] = self.profile_num
        ret['stack'] = self.stack
        if not self.stack:
            ret['numvals'] = len(self.source)
        ret['sources'] = [x.exposure['juid'] for x in self.source] 
        return ret
        
class ProfileList:
    def __init__(self):
        self.profiles = []

    def GetProfile(self, source_list):
        found = self.LookupSourceList(source_list)
        if found == None:
            found = Profile(source_list)
            self.profiles.append(found)
        return found

    def LookupSourceList(self, source_list):
        found = next((x for x in self.profiles if x.source == source_list), None)
        return found

class Measurement:
    def __init__(self, mstar):
        self.mstar = mstar
        self.profile = None

    # returns a dictionary
    def ToJSON(self):
        ret = {}
        ret['name'] = self.mstar.gstar.name
        if self.mstar.transformed_mag is None:
            ret['mag'] = self.mstar.mag
            ret['uncty'] = self.mstar.uncertainty
            ret['uncty_src'] = self.mstar.uncty_source
            ret['transformed'] = False
        else:
            ret['mag'] = self.mstar.transformed_mag
            ret['uncty'] = self.mstar.uncertainty
            ret['uncty_src'] = self.mstar.uncty_source
            ret['transformed'] = True
            ret['xform_info'] = self.mstar.transform_info
            ret['xform_method'] = self.mstar.xform_method
        ret['profile'] = self.profile.profile_num
        ret['group'] = self.mstar.group
        return ret

class BVRIAnalysisSet:
    def __init__(self, db, bvri, images):
        self.db = db
        self.profiles = ProfileList()
        self.measurements = [] # a list of diff_analysis.Measurement
        self.bvri_set = bvri
        self.images = images
        self.star_xref = {}     # index is starname, value is gstar

    def AddMeasurement(self, mstar):
        # mstar.source_list is a list of IStar references
        source_list = [x.image_ref for x in mstar.source_list]
        profile = self.profiles.GetProfile(source_list)
        this_measurement = Measurement(mstar)
        self.measurements.append(this_measurement)
        this_measurement.profile = profile
        self.star_xref[mstar.gstar.name] = mstar.gstar
        
    def FindMeasurement(self, starname, filter):
        return next((m for m in self.measurements
                     if m.mstar.gstar.name == starname and m.profile.filter == filter),None)

    def EnsembleFit(self, target_analysis):
        ensemble_stars = set()
        filters_in_use = set()
        for x in target_analysis['ensembles']:
            ensemble_stars.update(x['errs'].keys())
            filters_in_use.add(x['filter'])

        res = {}
        for filter in filters_in_use:
            this_section = {}
            this_section['errs'] = {}
            all_errors = []
            for starname in ensemble_stars:
                measurement = self.FindMeasurement(starname, filter)
                if measurement is not None and filter in measurement.mstar.gstar.ref_mag:
                    err = measurement.mstar.gstar.ref_mag[filter][0]-measurement.mstar.transformed_mag
                    this_section['errs'][starname] = err
                    all_errors.append(err)
            if len(all_errors) > 0:
                this_section['offset'] = statistics.mean(all_errors)
                this_section['stdev'] = statistics.stdev(all_errors) if len(all_errors) > 1 else None
                res[filter] = this_section
        print("EnsembleFit() returning ", res)
        return res

    def CheckStarFit(self, target_analysis):
        # bvri_checks is a list of MStars
        bvri_checks = set()
        filters = context.key_stars.comps.keys()

        rms = {}

        # Each color is independent
        for c in filters:
            this_section = {}
            this_section['errs'] = {}
            check_subset = [x.mstar for x in self.measurements if x.mstar.filter == c and
                            x.mstar.gstar in context.key_stars.checks[c]]
            residuals = []
            for star in check_subset:
                cat_mag = star.gstar.ref_mag[c][0]
                my_mag = star.transformed_mag
                print(star.gstar.name, ",", c, ",", cat_mag, ",", my_mag,",",star.mag)
                this_err = cat_mag - my_mag
                residuals.append(this_err)
                this_section['errs'][star.gstar.name] = this_err
                print("CheckStarFit: (",c,"): ",star.gstar.name,this_err)
            if len(residuals) > 1:
                this_section['rms'] = statistics.stdev(residuals)
                this_section['offset'] = statistics.mean(residuals)
            elif len(residuals) == 1:
                this_section['rms'] = abs(residuals[0])
                this_section['offset'] = residuals[0]
            rms[c] = this_section
        return rms
        
    def UpdateSubmissions(self, analysis):
        if 'submissions' in self.db:
            submissions = self.db['submissions']
        else:
            submissions = []
            self.db['submissions'] = submissions
            
        for r in analysis['results']: # each r is a "results" dictionary
            starname = r['name']
            profile_no = r['profile']
            profile = next((x for x in analysis['profiles'] if x['pnum'] == profile_no),None)
            filter = profile['filter']
            cat_entry = self.star_xref[starname]
            if cat_entry.do_submit:
                # Find existing submission, if any
                existing_sub = next((x for x in submissions if x['filter'] == filter and
                                     x['name'] == starname), None)
                if existing_sub is None:
                    print("UpdateSubmissions(): missing entry for ", starname, '/', filter)
                    existing_sub = {'juid' : context.db.GetNextJUID('submission'),
                                    'filter' : filter,
                                    'name' : starname,
                                    'tstamp' : 0, # fixed later
                                    'analysis' : analysis['juid'],
                                    'inhibit' : 0,
                                    'use_override' : 0
                                    } 
                    submissions.append(existing_sub)
                dl = DataLine(context.db, starname)
                dl.InitFromJSON(analysis, r, self.db['inst_mags'], cat_entry, context.catalog)
                dld = DataLineDict(dl)
                existing_sub['computed'] = dld.value
                context.db.UpdateTStamp(existing_sub)
        print("UpdateSubmissions() completed:")
        print(submissions)
                    
    def Close(self):
        #pdb.set_trace()
        print("BVRIAnalysisSet uses ", len(self.profiles.profiles), " profiles.")
        print("Analyses uses an ensemble: ", context.use_ensemble)
        # Does an existing analysis set exist for this BVRI Set?
        bvri_set_juid = self.bvri_set['juid']
        target_analysis = next((x for x in self.db['analyses'] if x['source'] == bvri_set_juid),
                               None)
        
        if target_analysis == None:
            target_analysis = {
                'juid' : context.db.GetNextJUID('analysis'),
                'source' : bvri_set_juid,
                'target' : context.target,
                'directive' : context.directive.juid,
                'tstamp' : 0,   # filled in later
                'atype' : 'BVRI',
            }
            self.db['analyses'].append(target_analysis)

        target_analysis['ref_color'] = context.ref_color
        target_analysis['technique'] = 'ENSEMBLE' if context.use_ensemble else 'SINGLE_COMP'
        target_analysis['profiles'] = [x.ToJSON() for x in self.profiles.profiles]
        target_analysis['results'] = [x.ToJSON() for x in self.measurements]
        context.db.UpdateTStamp(target_analysis)

        if context.use_ensemble:
            target_analysis['ensembles'] = [x.ensemble_fit.JSONStruct(x) for x in self.images
                                            if hasattr(x, 'ensemble_fit') ]
            target_analysis['ensemble_fit'] = self.EnsembleFit(target_analysis)
            if 'comp' in target_analysis:
                del target_analysis['comp']
        else:
            comp_star_name = context.key_stars.raw_comps[0].name
            target_analysis['comp'] = comp_star_name
            if 'ensembles' in target_analysis:
                del target_analysis['ensembles']
            if 'ensemble_fit' in target_analysis:
                del target_analysis['ensemble_fit']

        target_analysis['check_fit'] = self.CheckStarFit(target_analysis)

        self.UpdateSubmissions(target_analysis)

        print("Close() set target_analysis to: ", target_analysis)
        
class DataLineDict:
    def __init__(self, dataline, json=None):
        if json is not None:
            self.value = json
            return
        
        ret = {}
        ret['technique'] = dataline.technique       # string
        ret['cat_name'] = dataline.cat_name         # string
        ret['report_name'] = dataline.report_name   # string or None
        ret['time'] = "{:.4f}".format(dataline.time) # float
        ret['mag'] = "{:.3f}".format(dataline.mag)   # float
        ret['mag_inst'] = "{:.3f}".format(dataline.mag_inst)   # float
        ret['nobs'] = str(dataline.nobs) if dataline.nobs is not None else None
        ret['filter'] = dataline.filter             # string
        ret['is_transformed'] = dataline.is_transformed # boolean
        ret['compname'] = dataline.compname         # string
        ret['comp_auid'] = dataline.comp_auid       # string
        ret['compmag_inst'] = ("{:.3f}".format(dataline.compmag_inst)
                               if isinstance(dataline.compmag_inst, float) else dataline.compmag_inst)
        ret['compmag_std'] = ("{:.3f}".format(dataline.compmag_std) if
                              isinstance(dataline.compmag_inst, float) else dataline.compmag_inst)
        ret['checkname'] = dataline.checkname       # string
        ret['check_auid'] = dataline.check_auid     # string
        if isinstance(dataline.checkmag_inst, float):
            ret['checkmag_inst'] = "{:.3f}".format(dataline.checkmag_inst)
        else:
            if dataline.checkmag_inst is None:
                ret['checkmag_inst'] = "na"
            else:
                ret['checkmag_inst'] = dataline.checkmag_inst
        if isinstance(dataline.checkmag_std, float):
            ret['checkmag_std'] = "{:.3f}".format(dataline.checkmag_std)
        else:
            if dataline.checkmag_std is None:
                ret['checkmag_std'] = 'na'
            else:
                ret['checkmag_std'] = dataline.checkmag_std
                
        ret['uncty'] = "{:.3f}".format(dataline.uncty)
        ret['uncty_src'] = dataline.uncty_src       # string
        ret['airmass'] = "{:.4f}".format(dataline.airmass)
        ret['chart'] = dataline.chart               # string
        ret['xform_del_color'] = "{:.3f}".format(dataline.xform_del_color)
        ret['xform_coef_name'] = dataline.xform_coef_name
        ret['xform_coef_value'] = "{:.3f}".format(dataline.xform_coef_value)
        ret['xform_adj_amount'] = "{:.3f}".format(dataline.xform_adj_amount)
        ret['xform_ref_color'] = "{:.3f}".format(dataline.xform_ref_color)
        ret['xform_ref_color_name'] = dataline.xform_ref_color_name
        ret['ensemble_members'] = dataline.ensemble_members # list of strings
        if dataline.ensemble_fitting is not None:
            ret['ensemble_fitting'] = "{:.3f}".format(dataline.ensemble_fitting)
        else:
            ret['ensemble_fitting'] = 'na'
        ret['check_rms'] = ("{:.3f}".format(dataline.check_rms)
                            if dataline.check_rms is not None else None)
        ret['num_check_stars'] = str(dataline.num_check_stars) # integer
        ret['group'] = str(dataline.group)                     # integer
        ret['comments'] = dataline.comments

        self.value = ret
        
    # This creates an AEFF line
    def ToString(self):
        ret = (self.value['cat_name'] if self.value['report_name'] ==
               None else self.value['report_name'])
        ret += ','
        ret += self.value['time']
        ret += ','
        ret += self.value['mag']
        ret += ','
        ret += self.value['uncty']
        ret += ','
        ret += self.value['filter'] # canonical filter name is okay
        ret += ','
        ret += "YES" if self.value['is_transformed'] else "NO"
        ret += ',STD,'
        ret += self.value['comp_auid']
        ret += ','
        ret += self.value['compmag_inst']
        ret += ','
        ret += self.value['check_auid']
        ret += ','
        ret += self.value['checkmag_inst']
        ret += ','
        ret += self.value['airmass']
        ret += ','
        ret += self.value['group']
        ret += ','
        ret += self.value['chart']
        ret += ','
        if self.value['technique'] != 'ENSEMBLE':
            if self.value['comments'] == None or len(self.value['comments']) == 0:
                ret += 'na'
            else:
                ret += self.value['comments']
        else:
            is_first = True
            for (key,var,value) in [
                    ('uncty_src',
                     'UNCTY',
                     self.value['uncty_src']),
                    ('xform_del_color',
                     'DEL_'+self.value['xform_ref_color_name'],
                     self.value['xform_del_color']),
                    ('xform_coef_value',
                     self.value['xform_coef_name'],
                     self.value['xform_coef_value']),
                    ('nobs','NOBS',self.value['nobs']),
                    ('xform_ref_color',
                     'REF_'+self.value['xform_ref_color_name'],
                     self.value['xform_ref_color']),
                    ('xform_adj_amount',
                     'XADJ', self.value['xform_adj_amount']),
                    ('ensemble',
                     'ENSEMBLE',
                     self.ConcatAUIDs(self.value['ensemble_members'])),
                    ('check_rms','KERR',self.value['check_rms']),
                    ('num_check_stars',
                     'NUMCHECKSTARS',
                     self.value['num_check_stars'])]:
                ret += self.AddNote(is_first, key, var, value)
                is_first = False
        return ret

    def ConcatAUIDs(self, auidlist): # auidlist is list of integers
        ret = ""
        is_first = True
        for auid in auidlist:
            if is_first:
                is_first = False
            else:
                ret += ','
            ret += str(auid)
        return ret

    def AddNote(self, is_first, key, var, value):
        if value is None:
            return ""
        ret = ""
        if not is_first:
            ret += '|'
        try:
            ret += (var + '=' + value)
        except:
            print("key = ", key, "var = ", var, "value = ", value)
            raise
        return ret

class DataLine:
    def __init__(self, db_obj, name):
        self.db_obj = db_obj
        self.cat_name = name
        self.calculated = True  # False if human-entered
        self.report_name = None # string
        self.time = None        # float
        self.mag = None         # float
        self.mag_inst = None    # float
        self.nobs = None        # integer or None
        self.filter = None      # string
        self.is_transformed = None # boolean
        self.compname = None       # string
        self.comp_auid = None      # string (might be "NA")
        self.compmag_inst = None   # float or string ("na")
        self.compmag_std = None    # float or string ("na")
        self.checkname = None
        self.check_auid = None
        self.checkmag_inst = None
        self.checkmag_std = None
        self.uncty = None       # float
        self.uncty_src = None   # string (e.g., 'STDDEV', 'Fitting', )
        self.airmass = None     # float or string ('na')
        self.chart = None       # WHERE DOES THIS COME FROM?????
        self.xform_del_color = None
        self.xform_coef_name = None
        self.xform_coef_value = None
        self.xform_adj_amount = None
        self.xform_ref_color = None
        self.xform_ref_color_name = None
        self.ensemble_members = None # string (list of AUID)
        self.ensemble_fitting = None # float (or None)
        self.check_rms = None        # float when reporting multiple checkstars
        self.num_check_stars = None  # int when reporting multiple checkstars
        self.group = None
        self.comments = ""

    # "analysis" -- reference to the entire list item in analyses for
    # this target
    # "result" -- This refers to a single standard mag item in "results"
    def InitFromJSON(self, analysis, result, inst_mags, cat_star, catalog):
        def GetInstMag(cat_name, sources): #
            print("GetInstMag: ", cat_name, " from ", sources)
            inst_mag_list = []
            for d in inst_mags:
                if d['exposure'] in sources:
                    measurements = d['measurements']
                    match = next((x for x in measurements if x['name'] == cat_name),None)
                    if match is not None:
                        inst_mag_list.append(match['imag'])
            if len(inst_mag_list) > 0:
                return statistics.mean(inst_mag_list)
            else:
                return None

        def GetChartID(sources):
            chartlist = set()
            for s in sources:
                image = self.db_obj.FindExposureByJUID(s)
                if image is not None and 'chart' in image:
                    chartlist.add(image['chart'])
            if len(chartlist) == 0:
                return None
            if len(chartlist) != 1:
                print("Multiple charts referenced: ", chartlist)
            return chartlist.pop()
                    
        profile_dict = dict([(x['pnum'],x) for x in analysis['profiles']])
        profile = profile_dict[result['profile']]
        #profile = next((x for x in analysis['profiles'] if x['pnum'] == result['profile']), None)

        self.analysis = analysis
        self.calculated = True
        self.report_name = self.cat_name if cat_star.report_name is None else cat_star.report_name
        self.time = profile['jd']
        self.mag = result['mag']
        self.mag_inst = GetInstMag(self.cat_name, profile['sources'])
        self.nobs = profile['numvals'] if 'numvals' in profile else 1
        self.chart = GetChartID(profile['sources'])
        self.filter = profile['filter']
        self.is_transformed = result['transformed']
        if analysis['technique'] == 'ENSEMBLE':
            self.technique = 'ENSEMBLE'
            self.compname = 'ENSEMBLE' 
            self.comp_auid = 'ENSEMBLE'
            self.compmag_inst = 'na'
            self.compmag_std = 'na'
            self.ensemble_members = [catalog[x].AUID for x in profile['comps']]
            self.ensemble_fitting = next((fit['stdev'] for
                                          (filter,fit) in analysis['ensemble_fit'].items()
                                          if filter == self.filter), None)
        else:
            self.technique = 'COMP'
            self.compname = profile['comps'][0]
            self.comp_auid = catalog[self.compname].AUID # Might be None
            self.ensemble_members = []

        checkfit = next((fit for (filter,fit) in analysis['check_fit'].items()
                         if filter == self.filter), None)
        if checkfit is not None:
            self.num_check_stars = len(checkfit['errs'])
            if 'rms' in checkfit:
                self.check_rms = checkfit['rms']
            else:
                self.check_rms = None
        if 'airmass' in profile:
            self.airmass = profile['airmass']
        else:
            self.airmass = 'na'
        self.uncty = result['uncty']
        self.uncty_src = result['uncty_src']
        self.group = result['group']

        ################################
        # Reportable check star
        ################################
        chosen_check = None
        #checkrefs = [name for (name,cat) in catalog.items() if cat.IsCheck(self.filter) and
        #             cat.is_ref and StarInResult(name, analysis['results']) and
        #             cat.AUID not in self.ensemble_members]
        chosen_check = context.key_stars.ref_check[self.filter]
        if chosen_check is not None:
            chosen_check = chosen_check.name
            
        #if len(checkrefs) == 1:
        #    chosen_check = checkrefs[0]
        #elif len(checkrefs) > 1:
        #    # Pick one; what criteria to use?? Choose brightest.
        #    best_starname = checkrefs[0]
        #    best_mag = catalog[best_starname].ref_mag[self.filter]
        #    for c in checkrefs[1:]:
        #        starname = c
        #        (mag, uncty) = catalog[starname].ref_mag[self.filter]
        #        if mag < best_mag:
        #            best_starname = starname
        #            best_mag = mag
        #    chosen_check = best_starname
        #else:
        #    # Need to pick a non-ref check
        #    checks = [name for (name,cat) in catalog.items() if cat.IsCheck(self.filter) and
        #              StarInResult(name, analysis['results']) and
        #              cat.AUID not in self.ensemble_members]
        #    best_starname = None
        #    best_mag = 99.9
        #    for c in checks:
        #        if self.filter not in catalog[c].ref_mag:
        #            continue
        #        (mag, uncty) = catalog[c].ref_mag[self.filter]
        #        if mag < best_mag:
        #            best_starname = c;
        #            best_mag = mag
        #    chosen_check = best_starname
        cat = None
        measurement = None
        if chosen_check == None:
            self.checkname = 'na'
            self.check_auid = 'na'
            self.checkmag_inst='na'
            self.checkmag_std='na'
        else:
            print("chosen_check = ", chosen_check)
            self.checkname = chosen_check
            #print(catalog)
            cat = catalog[self.checkname]
            self.check_auid = cat.AUID
            measurement = next((r for r in analysis['results']
                                if r['name'] == chosen_check and
                                profile_dict[r['profile']]['filter'] == self.filter),None)
            if measurement is not None:
                self.checkmag_std = measurement['mag']
                print("Measurement tied to profile ", measurement['profile'])
                profile = profile_dict[measurement['profile']]
            if 'sources' in profile:
                self.checkmag_inst = GetInstMag(self.checkname, profile['sources'])
            else:
                self.checkmag_inst = 'na'

        print("Checkmag: name=",chosen_check,"(",self.filter,
              "), analysis juid = ", self.analysis['juid'], "value=",self.checkmag_inst)
        if not isinstance(self.checkmag_inst, float):
            print("Checkmag concern: ", chosen_check, "(",
                  self.filter, ")")
            if cat is not None:
                print("check_auid = ", cat.AUID)
            print("measurement = ", measurement)
            print("analysis juid = ", self.analysis['juid'])
                

        ################################
        # Color Transform Info
        ################################
        if 'xform_info' in result:
            info = result['xform_info']
            for (varname,value) in info.items():
                if 'DELTA' in varname:
                    self.xform_ref_color_name = varname[6:]
                    self.xform_del_color = value
                    #print("xform_ref_color_name: ", varname, self.xform_ref_color_name)
                elif varname[0] == 'T' and '_' in varname:
                    self.xform_coef_name = varname
                    self.xform_coef_value = value
                    
            self.xform_adj_amount = info['X_ADJ']
            self.xform_ref_color = None # the reference DELTA_BV value (float)

        if 'ref_color' in analysis:
            ref_color_data = analysis['ref_color']
            if self.xform_ref_color_name is not None and len(self.xform_ref_color_name) == 2:
                color1 = self.xform_ref_color_name[0]
                color2 = self.xform_ref_color_name[1]
                if color1 in ref_color_data and color2 in ref_color_data:
                    self.xform_ref_color = ref_color_data[color1]-ref_color_data[color2]
            
def StarInResult(starname, result):
    try:
        #print("StarInResult(", starname, ", ", result, ")")
        matches = [r for r in result if r['name'] == starname]
        return len(matches) >= 1
    except:
        print("StarInResult failed: starname = ", starname, " and result = ", result)
        raise

