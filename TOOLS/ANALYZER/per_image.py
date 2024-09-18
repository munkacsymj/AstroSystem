import math
import statistics
from PYTHON_LIB.ASTRO_DB_LIB.util import FindJUID
import context
from PYTHON_LIB.IMAGE_LIB import star as star_module
from PYTHON_LIB.IMAGE_LIB import filter
from PYTHON_LIB.DATA_LIB import extinction

class ErrorTable:
    def __init__(self):
        self.current_image = -1
        self.matrix = {} # indexed by starname, value is dictionary (indexed by image)
        self.filter_list = {} # indexed by image

    def StartNewImage(self, filter):
        self.current_image += 1
        self.filter_list[self.current_image] = filter

    def AddErr(self, starname, err):
        if starname not in self.matrix:
            self.matrix[starname] = {}
        self.matrix[starname][self.current_image] = err

    def PrintAsCSV(self, filename):
        with open(filename, "w") as fp:
            fp.write(",")
            for x in self.filter_list.values():
                fp.write(x + ',')
            fp.write("\n")

            for star,dict in self.matrix.items():
                fp.write(star + ',')
                for i in range(self.current_image+1):
                    if i in dict:
                        fp.write(str(dict[i])+',')
                    else:
                        fp.write(',')
                fp.write('\n')

global error_table
error_table = ErrorTable()

# This class covers a single color
# Remember: "error = catalog - measured"
class EnsembleFit:
    def __init__(self, filter):
        self.filter = filter
        self.errs = {}          # index is starname, value is err
        self.stdev = None
        self.offset = None

    def AddErr(self, starname, err):
        self.errs[starname] = err

    def JSONStruct(self, image):
        self.offset = statistics.mean(self.errs.values())
        if len(self.errs) > 1:
            self.stdev = statistics.stdev(self.errs.values())
        else:
            self.stdev = 0.0

        if image is not None:
            return { 'offset' : self.offset,
                     'filter' : self.filter,
                     'stdev'  : self.stdev,
                     'errs'   : self.errs,
                     'source' : image.juid }
        else:
            return { 'offset' : self.offset,
                     'filter' : self.filter,
                     'stdev'  : self.stdev,
                     'errs'   : self.errs }

class PerImage:
    # Member variables:
    # self.exposure        -- points to the {exposure or stack} for this image
    # self.ensemble[]      -- points to the
    # self.checks[]        --
    # self.inst_mags       -- points to the json record of inst_mags for this image/stack
    # self.istars[]        -- points to the IStars in this exposure/image
    # self.target          -- name of the original target star
    # self.db              -- the original (top-level) database
    # self.filter          -- canonical filter name for this exposure/stack
    # self.exposure_time   -- float(exposure time in seconds)
    # self.zero            -- (catalog_mag - instrumental_mag)
    # self.average_airmass -- average airmass for stars in the exposure/stack
    # self.ens_fitting_rms -- rms scatter during ensemble fitting (mags)
    # self.check_rms       -- rms scatter across check stars
    
    def __init__(self, db, exposure_juid):
        if 2000000 <= exposure_juid <= 2999999:
            self.stack = False
            self.exposure = FindJUID(db, "exposures", exposure_juid)
        elif 6000000 <= exposure_juid <= 6999999:
            self.stack = True
            self.exposure = FindJUID(db, "stacks", exposure_juid)
        self.ensemble = []
        self.checks = []
        self.inst_mags = next((x for x in db['inst_mags'] if int(x['exposure'] ) == exposure_juid),None)
        if self.inst_mags is None:
            print("PerImage: no inst mags found for exposure ", exposure_juid)
            return
        if type(self.inst_mags) is not dict:
            print("self.inst_mags = ", self.inst_mags)
        self.target = self.exposure['target']
        self.db = db
        self.filter = filter.to_canonical[self.exposure['filter']]
        self.exposure_time = self.exposure['exposure']
        self.juid = exposure_juid

    def FetchInstMags(self):
        self.istars = [star_module.IStar(x,self.stack,self) for x in self.inst_mags['measurements']]

    def AddExtinctionStars(self, output_csv):
        exposure_time_seconds = self.exposure_time
        inst_mag_adjust = -2.5 * math.log10(exposure_time_seconds)
        for i in self.istars:
            x = i.gstar
            if x is None:
                print("Missing gstar. Star = ", i.name, flush=True)
            if (x.IsEnsemble(self.filter) or x.is_comp_candidate or x.IsCheck(self.filter)):
                obs_filter = filter.FilterSynonyms(self.filter,'AAVSO')
                if obs_filter in x.ref_mag:
                    (ref_mag,uncty) = x.ref_mag[obs_filter]
                    output_csv.write(self.filter+','+
                                     str(i.airmass)+ ','+
                                     str(i.imag-inst_mag_adjust-ref_mag)+'\n')
            
    # comp_star_list is a list of catalog stars
    def CalculateZero(self, key_stars):
        global error_table
        # comp_stars is a list of IStars
        comp_stars = [x for x in self.istars if x.gstar in key_stars.ChooseCompStars(self)]
        self.comp_stars = comp_stars
        print("CalculateZero() for ", len(comp_stars), " comp stars, filter = ", self.filter)
        if len(comp_stars) == 0:
            print("*** No comp stars in image ***")
            print("Image: ", self.exposure['filename'])
            print("comp_star_list = ", comp_stars)
            for x in self.istars:
                msg = x.name
                if x.gstar in comp_stars:
                    msg += ' in comp_stars'
                    if self.filter in x.gstar.ref_mag:
                        msg += (' with data for '+self.filter)
                    else:
                        msg += (' with no data for '+self.filter)
                print(msg)
            self.zero = None
            return
        
        # These will be weighted identically, so can now compute reference color
        if context.ref_color == None:
            context.ref_color = {} # ref_color is held as a dictionary indexed by canonical filter names
            for color in filter.canonical_set:
                mag_set = [x.gstar.ref_mag[color][0]
                           for x in comp_stars if color in x.gstar.ref_mag]
                if len(mag_set) > 0:
                    avg_mag = statistics.mean(mag_set)
                    context.ref_color[color] = avg_mag

        # Compute average airmass
        self.average_airmass = statistics.mean(x.airmass for x in self.istars)
        print("Average airmass = ", self.average_airmass)
        extinction_coefficient = (context.extinction_coefficients[self.filter]
                                  if self.filter in context.extinction_coefficients
                                  else 0.0)
        
        # Now tweak all the comp stars
        for star in comp_stars:
            xform_adj = filter.CatColorTransform(self.filter, context.ref_color, None, star.gstar)
            print("xform_adj = ", xform_adj)
            star.xformed_imag = (star.imag +
                                 (star.airmass-self.average_airmass)*extinction_coefficient +
                                 xform_adj)
        # And compute the mean zero point
        self.zero = statistics.mean(star.gstar.ref_mag[self.filter][0]-star.xformed_imag
                                    for star in comp_stars)
        print("Zero point for image is ", self.zero)

        self.ensemble_fit = EnsembleFit(self.filter)

        print("Ensemble fitting errors for this image:")
        error_table.StartNewImage(self.filter)
        for star in comp_stars:
            this_error = star.gstar.ref_mag[self.filter][0]-star.xformed_imag-self.zero
            print('EFE,',star.gstar.name,',',self.filter,',', this_error)
            error_table.AddErr(star.gstar.name, this_error)
            self.ensemble_fit.AddErr(star.gstar.name, this_error)

        # Finally, compute the RMS fiting quality
        if len(comp_stars) > 1:
            self.ens_fitting_rms = statistics.stdev(star.gstar.ref_mag[self.filter][0]-star.xformed_imag
                                                    for star in comp_stars)
            print("ensemble fitting quality = ", self.ens_fitting_rms, " (mags, rms)")
        else:
            self.ens_fitting_rms = None

    #def CalculateCheckRMS(self, check_star_list):
    def CalculateUntransformedMags(self):
        airmass_avail = hasattr(self, 'average_airmass')
        for star in self.istars:
            if airmass_avail:
                extinct = extinction.ExtinctionCorrection(star.airmass,
                                                          self.average_airmass,
                                                          self.filter)
                if extinct is None:
                    extinct = 0
            else:
                extinct = 0

            print("star.imag = ", star.imag, ", extinct = ", extinct, ", self.zero = ",
                  self.zero)
            star.untransformed = (star.imag + extinct + self.zero)
            
    def AddToMerge(self, merge_dict):
        for star in self.istars:
            if star.gstar not in merge_dict:
                new_merge = star_module.MStar()
                merge_dict[star.gstar] = new_merge
                new_merge.filter = self.filter
                new_merge.gstar = star.gstar
            # The MStar goes into the merge_dict{}
            merge_dict[star.gstar].sources.append(star)
