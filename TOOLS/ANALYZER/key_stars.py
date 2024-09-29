import context
import math
import statistics

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
from PYTHON_LIB.IMAGE_LIB import filter

class KeyStars:
    def __init__(self, directive, target_name):
        fatal_error = False;
        self.comps = {}         # index by color, value is single  star.Star
        self.checks = {}        # index by color, value is list of star.Star
        self.ensemble = {}      # index by color, value is list of star.Star
        self.ref_check = {}     # index by color, value is a single star.Star
        self.observations = {}  # index by color, value is list of starnames
        
        for filter in ['B', 'V', 'R', 'I']:
            raw_comps = [x for x in context.catalog.values() if x.is_comp_candidate and
                         filter in x.ref_mag]
            raw_checks = [x for x in context.catalog.values() if x.IsCheck(filter) and
                          filter in x.ref_mag]
            raw_ensemble = [x for x in context.catalog.values() if x.IsEnsemble(filter) and
                            filter in x.ref_mag]
            raw_ref_check = [x for x in raw_checks if x.is_ref and filter in x.ref_mag]

            if context.use_ensemble:
                self.comps[filter] = None
                self.ensemble[filter] = [x for x in raw_ensemble
                                         if x.name not in directive.FetchEnsembleExclusions(filter)]
                self.checks[filter] = [x for x in raw_checks
                                       if x.name not in directive.FetchCheckExclusions(filter) and
                                       x not in self.ensemble[filter]]
                self.ref_check[filter] = next((x for x in self.checks[filter] if x.is_ref), None)
            else:
                if len(raw_comps) != 1:
                    print("KeyStars err for ", target_name, ": wrong # of comp stars: ",
                          len(raw_comps))
                    fatal_error = True
                else:
                    self.comps[filter] = raw_comps[0]
                    self.ensemble[filter] = []
                    self.checks[filter] = [x for x in raw_checks
                                           if x.name not in directive.FetchCheckExclusions(filter) and
                                           x != raw_comps[0]]
                    self.ref_check[filter] = next((x for x in self.checks[filter] if x.is_ref), None)

            if self.ref_check[filter] is None:
                fatal_error = True
                print("KeyStars err for ", target_name, ": cannot find REF checkstar. (",filter,")")

        #if fatal_error:
        #    quit()
                
    def ConsiderObservations(self, per_image):
        filter = per_image.filter
        if filter not in self.observations:
            self.observations[filter] = set()
        self.observations[filter] |= set([x.name for x in per_image.istars])

    def SelectCheckStars(self):
        for filter in self.ref_check.keys():
            self.ensemble[filter] = [x for x in self.ensemble[filter]
                                     if x.name in self.observations[filter]]
            self.checks[filter] = [x for x in self.checks[filter]
                                   if x.name in self.observations[filter]]
            if (self.ref_check[filter] is None or
                self.ref_check[filter].name not in self.observations[filter]):
                # Okay, have a problem: ref_check wasn't observed
                if len(self.checks[filter]) > 0:
                    self.ref_check[filter] = self.checks[filter][0]
                else:
                    self.ref_check[filter] = None
            
    # Returns a list of catalog stars
    def ChooseCompStars(self, per_image):
        color = filter.to_canonical[per_image.filter]
        if context.use_ensemble:
            return self.ensemble[color]
        else:
            return [self.comps[color]]

    def ComputeCheckStatistics(self, bvri_stars):
        # bvri_checks is a list of MStars
        bvri_checks = [x for y in bvri_stars.values() if y.g_star in self.check_stars
                       for x in y.sources.values()]
        all_colors = set() # canonical names
        for m in bvri_checks:
            all_colors.add(m.filter)

        rms = {}

        # Each color is independent
        for c in all_colors:
            check_subset = [x for x in bvri_checks if x.filter == c]
            residuals = []
            for star in check_subset:
                if c in star.gstar.ref_mag:
                    cat_mag = star.gstar.ref_mag[c][0]
                    my_mag = star.transformed_mag
                    print(star.gstar.name, ",", c, ",", cat_mag, ",", my_mag,",",star.mag)
                    residuals.append(cat_mag - my_mag)
            if len(residuals) > 0:
                rms[c] = math.sqrt(statistics.mean(x*x for x in residuals))
            else:
                rms[c] = None

        print("Check star residuals:")
        for (c,rms) in rms.items():
            print(c, f': {rms:.4f}')
            

            
