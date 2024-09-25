import os
import statistics
from . import filter, moderole

# There are *global* stars and there are *image* stars
catalog = None

################################
##  Global Star
################################
class Star:
    def __init__(self, catalog_line):
        # first strip out comments
        words = catalog_line.split('#')
        clean_line = words[0].strip()
        words = clean_line.split()
        if len(words) < 1:
            print("Something wrong with catalog line: '", catalog_line, "'")
            quit()
        
        self.name = words[0]
        cl_dec_rad = float(words[1])
        cl_ra_rad = float(words[2])
        cl_nom_mag = float(words[3])
        self.is_comp_candidate = False
        self.is_comp_inuse = False
        self.is_ref = False
        self.AUID = None
        self.nom_vmag = None
        self.ref_mag = {}
        self.report_name = None
        self.do_submit = False
        self.is_ensemble_all_filters = False
        self.is_check_all_filters = False
        self.ensemble_filters = [] # list of canonical filter names
        self.check_filters = [] # list of canonical filter names

        self.inst_mag_raw = 0.0
        self.inst_mag_adj = 0.0
        self.airmass = 0.0
        self.mode_role = moderole.ModeRole()

        for word in words[4:]:
            # figure out whether keyword=value or just keyword
            keyword = word # might be overwritten later
            value = None
            if '=' in word:
                pieces = word.split('=')
                keyword = pieces[0]
                value = pieces[1]
            if keyword == 'WIDE' or keyword == 'VARIABLE':
                pass # ignore
            elif keyword == 'SUBMIT':
                self.do_submit = True
            elif keyword == 'MVE':
                pass # ignore
            elif keyword == 'COMP':
                self.is_comp_candidate = True
            elif keyword == 'REF':
                self.is_ref = True
            elif keyword == 'CHECK':
                if value is None:
                    self.is_check_all_filters = True
                else:
                    filter_string = value
                    self.check_filters = [filter.to_canonical[f] for f in filter_string.split(',')]
                self.is_check_all_filters = True
            elif 'CHECK:' in keyword:
                filter_string = keyword[6:]
                self.check_filters = [filter.to_canonical[f] for f in filter_string.split(',')]
            elif keyword == 'SUBMIT':
                self.do_submit = True
            elif keyword == 'NOPOSIT':
                pass # ignore
            elif keyword == 'ENSEMBLE':
                if value is None:
                    self.is_ensemble_all_filters = True
                else:
                    filter_string = value
                    self.ensemble_filters = [filter.to_canonical[f] for f in filter_string.split(',')]
                self.is_ensemble_all_filters = True
            elif 'ENSEMBLE:' in keyword:
                filter_string = keyword[9:]
                self.ensemble_filters = [filter.to_canonical[f] for f in filter_string.split(',')]
            elif keyword == 'FORCE':
                pass # ignore
            elif keyword == 'MV':
                self.nom_vmag = float(value)
            elif keyword == 'AUID':
                self.AUID = value
            elif keyword == 'REPORT':
                self.report_name = value
            elif keyword in ['PV','PB','PU','PR','PI','PJ','PH','PK']:
                self.GrabPhotometry(keyword, value)
            else:
                print("ERROR: catalog keyword unrecognized: ", keyword)
        self.SetModeRole()

    def SetModeRole(self):
        if self.is_comp_candidate:
            self.mode_role.Set(moderole.ModeRole.all_filters,
                               moderole.ModeRole.MODE_COMP,
                               moderole.ModeRole.COMP)
        if self.is_ref:
            self.mode_role.Set(moderole.ModeRole.all_filters,
                               moderole.ModeRole.MODE_ALL,
                               moderole.ModeRole.CHECKREF_CAND)
        for f in moderole.ModeRole.standard_filters:
            if self.IsCheck(f):
                self.mode_role.Set(f,
                                   moderole.ModeRole.MODE_COMP,
                                   moderole.ModeRole.CHECK_CAND)
                if not self.IsEnsemble(f):
                    self.mode_role.Set(f,
                                       moderole.ModeRole.MODE_ENS,
                                       moderole.ModeRole.CHECK_CAND)
            if self.IsEnsemble(f):
                self.mode_role.Set(f,
                                   moderole.ModeRole.MODE_ENS,
                                   moderole.ModeRole.ENS_CAND)
            

    def IsEnsemble(self, color):
        if self.is_ensemble_all_filters:
            return True
        return filter.to_canonical[color] in self.ensemble_filters;

    def IsCheck(self, color):
        if self.is_check_all_filters:
            return True
        return filter.to_canonical[color] in self.check_filters;

    def GrabPhotometry(self, keyword, value):
        # keyword is a two-character string (e.g., PB); by using the
        # second character, we end up with a canonical filter name
        color = keyword[1]
        words = value.split('|')
        magnitude = float(words[0].strip())
        if (len(words) > 1):
            uncertainty = float(words[1].strip())
        else:
            uncertainty = None

        self.ref_mag[color] = (magnitude,uncertainty)

def CatalogLookupByName(catalog, starname):
    resp = next((x for x in catalog if x.name == starname), None)
    if resp is None:
        print("CatalogLookupByName(", starname, ") failed:")
        print([x.name for x in catalog])
    return resp

def ReadCatalog(target_name):
    cat_path = os.path.join('/home/ASTRO/CATALOGS', target_name)
    #print("ReadCatalog(): reading catalog file ", cat_path)
    try:
        with open(cat_path, 'r') as fp:
            global catalog
            #oneline = fp.readline()
            #print("oneline = ", oneline, " with len = ", len(oneline.strip()))
            catalog = [Star(x) for x in fp.readlines() if len(x.strip()) > 25 and x.strip()[0] != '#']
            return catalog
    except IOError as x:
        print("Error opening catalog file ", cat_path, ": errno = ", x.errno, ', ',
              x.strerror)
        sys.exit(-2)

################################
##  Image Star
################################

class IStar:
    def __init__(self, inst_mag_measurement, is_stacked_image, image_ref):
        #print("inst_mag_measurement = ", inst_mag_measurement)
        self.image_ref = image_ref
        self.name = inst_mag_measurement['name']
        self.stacked = is_stacked_image
        self.gstar = next((x for x in catalog if x.name == self.name), None)
        self.imag = inst_mag_measurement['imag']
        self.uncty = inst_mag_measurement['uncty']
        self.airmass = inst_mag_measurement['airmass']
        self.untransformed = None
        
################################
## Merged Star
################################

class MStar:
    def __init__(self):
        self.sources = [] # IStar references
        self.filter = None # Canonical name
        self.mag = None # merged mag (untransformed)
        self.uncertainty = None
        self.from_stack = False
        self.uncty_source = None
        self.gstar = None
        self.group = None       # group number, integer used in AAVSO report

        self.transformed_mag = None
        self.transformed_ucty = None
        self.transform_info = {}
        self.xform_method = "untransformed"

    def DoMerge(self):
        print("Merging ", self.sources[0].name)
        stacks = []             # IStars
        singles = []            # IStars
        for i in self.sources:
            if i.stacked:
                stacks.append(i)
            else:
                singles.append(i)
        if len(stacks) > 0 and len(singles) < 2:
            # Use the stack
            print("    using stack image")
            self.from_stack = True
            self.mag = stacks[0].untransformed
            self.uncertainty = stacks[0].uncty
            self.uncty_source = "SNR"
            self.source_list = stacks
            self.inst_mag = stacks[0].imag
            self.num_images = 1

            if (hasattr(stacks[0].image_ref, 'ens_fitting_rms') and
                stacks[0].image_ref.ens_fitting_rms != None):
                self.uncertainty = stacks[0].image_ref.ens_fitting_rms
                self.uncty_source = 'Fitting'
            else:
                print("Stack ", stacks[0].image_ref.exposure['filename'],
                      " missing ens_fitting_rms")
                
        else:
            # Use the single images
            print("    using ", len(singles), " single images.")
            self.from_stack = False
            print([x.untransformed for x in singles])
            self.mag = statistics.mean([x.untransformed for x in singles])
            self.inst_mag = statistics.mean([x.imag for x in singles])
            if len(singles) > 1:
                self.uncertainty = statistics.stdev([x.untransformed for x in singles])
                self.uncty_source = "STDDEV"
            else:
                self.uncertainty = singles[0].uncty
                self.uncty_source = "SNR"
            self.num_images = len(singles)
            self.source_list = singles

################################
## BVRI Star
################################

class BVRIStar:
    next_group_id = 0
    def __init__(self, m_star):
        self.sources = {m_star.filter : m_star} # dictionary of MStars
        self.g_star = m_star.gstar
        self.group = BVRIStar.next_group_id
        BVRIStar.next_group_id += 1

################################
## PhotSummary
################################

class PhotSummary:
    def __init__(self, m_star):
        self.ref_mag = None
        self.mag = None
        self.uncty = None
        self.uncty_source = None
        self.filter = None      # always canonical filter name
        self.xform_info = None
        self.is_transformed = False

        self.num_obs = m_star.num_images
        self.inst_mag = m_star.inst_mag
        self.filter = m_star.filter

        if self.filter in m_star.gstar.ref_mag:
            # ref_mag is pair: (mag, uncty) -- just grab "mag"
            ref_mag = m_star.gstar.ref_mag[self.filter][0]

        if hasattr(m_star, "transformed_mag"):
            # Transformed == TRUE
            self.mag = m_star.transformed_mag
            self.uncty = m_star.transformed_uncty
            self.uncty_source = None
            self.xform_info = m_star.transform_info
            self.is_transformed = True
        else:
            # Transformed == FALSE
            self.mag = m_star.mag
            self.uncty = m_star.uncertainty
            self.uncty_source = None
            self.is_transformed = False
        

class AnalysisSummary:
    def __init__(self, analy, catalog):
        self.all_disp_stars = {} # index is canonical starname, value is star.StarDispSummary
        self.FiltersInUse = set()
        
        for res in analy['results']:
            starname = res['name']
            if starname not in self.all_disp_stars:
                self.all_disp_stars[starname] = StarDispSummary(starname, catalog)
            this_star = self.all_disp_stars[starname]
            profile_num = res['profile']
            profile = next((x for x in analy['profiles'] if x['pnum'] == profile_num),None)
            if profile is None:
                print("add_analysis: invalid profile: ", profile_num)
            else:
                this_star.technique = analy['technique']
                is_ensemble = (this_star.technique == 'ENSEMBLE')
                filter = profile['filter']
                self.FiltersInUse.add(filter)
                this_bvri = StarDispFilter(filter)
                this_star.bvri_data[filter] = this_bvri
                this_bvri.mag = res['mag']
                this_bvri.is_transformed = res['transformed']
                if 'airmass' in profile:
                    this_bvri.airmass = profile['airmass']
                if 'stack' in profile:
                    this_bvri.stack = profile['stack']
                    this_bvri.method_string = 'S'
                if 'numvals' in profile:
                    this_bvri.nobs = profile['numvals']
                    this_bvri.method_string = str(this_bvri.nobs)
                if 'uncty_src' in res:
                    this_bvri.uncty_src = res['uncty_src']
                elif 'UNCTY_SRC' in res['xform_info']:
                    this_bvri.uncty_src = res['xform_info']['UNCTY_SRC']
                this_bvri.uncty = res['uncty']
                if hasattr(this_bvri, 'uncty_src'):
                    if this_bvri.uncty_src == 'SNR':
                        this_bvri.uncty_snr = this_bvri.uncty
                    if this_bvri.uncty_src == 'Fitting':
                        this_bvri.uncty_seq = this_bvri.uncty
                    if this_bvri.uncty_src == 'STDDEV':
                        this_bvri.uncty_std = this_bvri.uncty

    def NameToMag(self, starname):
        return self.all_disp_stars[starname].chartname
            
class StarDispSummary:
    def __init__(self, starname, catalog):
        self.name = starname
        self.status = None
        self.color_index_val = None
        self.color_index_name = None
        self.bvri_data = {} # index is canonical filter name, value is StarDispFilter
        self.catalog = next((x for x in catalog if x.name == starname), None)
        self.chartname = self.catalog.report_name

    def ToDispString(self):
        header = '{:15} {:5} '
        status = '     '
        cname = ' '
        if self.chartname is not None:
            cname = self.chartname
        output = header.format(self.name, cname) + status
        color_data = '           '
        output += color_data
        for f in ['B','V','R','I']:
            if f in self.bvri_data:
                output += ' ' + self.bvri_data[f].ToDispString()
            else:
                output += ' '*18
        return output

class StarDispFilter:
    def __init__(self, filter):
        self.filter = filter
        self.mag = None         # standard mag
        self.is_transformed = None # normally, True or False
        self.uncty_snr = None
        self.uncty_seq = None
        self.uncty_std = None
        self.method_string = "" # "S" (stack) or n (multi-images)
        self.mag_err = None     # if standard photometry available
        self.nobs = None
        self.stack = None       # normally, True or False
        self.airmass = None
        self.xform_info = None  # dictionary
        self.xform_method = None # string: primary color index used, e.g., "BV"

    def ToDispString(self):
        bvri_line = '{mag:6.3f}{transformed} {err} {flags} '
        transformed = ' '
        if self.is_transformed:
            transformed = 't'
        err = '     '
        if self.uncty is not None:
            err = '{:5.3f}'.format(self.uncty)
        flags = self.method_string + '/'

        ## AFTER THE '/' SHOULD COME THE 3-CHAR UNCTY SOURCE; UNCTY IS DIFFERENT FROM ERROR!!!

        if self.nobs is not None:
            flags += '{:2}'.format(self.nobs)
        else:
            flags += '  '

        return bvri_line.format(mag=self.mag,
                                transformed=transformed,
                                err=err,
                                flags=flags)
