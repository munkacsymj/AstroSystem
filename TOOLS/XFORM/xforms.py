from sklearn.linear_model import LinearRegression
import statsmodels.api as sm
import numpy as np
import math
import statistics

import context
import plotter
import coefficients_json

class Exposure:
    def __init__(self):
        self.juid = None
        self.json = None
        self.filter = None
        self.inst_mags = None
        self.catalog = None
        self.index = 0
        self.exposure_zero = 0.0

class Measurement:
    def __init__(self):
        self.filter = None
        self.inst_mag = 0.0
        self.exclude = False

xform_targets = []

class XStar:
    def __init__(self, starname, tgt_obj):
        self.name = starname
        self.tgt_obj = tgt_obj
        self.cat_star = tgt_obj.catalog[starname] # cat_star is a star.Star
        self.measurements = {}  # key is filter, value is a measurement
        self.mag = {}

def ExtractMags():
    count = 0
    for filter in ['B','V','R','I']:
        for target in xform_targets:
            for thisstar in target.stars.values():
                if filter in thisstar.measurements:
                    thisstar.mag[filter.lower()] = thisstar.measurements[filter].inst_mag
                if filter in thisstar.cat_star.ref_mag:
                    thisstar.mag[filter] = thisstar.cat_star.ref_mag[filter][0]
                    count += 1
    print("extracted ", count, " reference mags")

# "list_of_targets" is a list of target name strings
def LoadExposures(list_of_targets):
    global xform_targets
    xform_targets = []
    next_index = 0

    print("xforms.LoadExposures(): target_list = ", list_of_targets)
    print("context.target_list[] holds: ", context.target_list.keys())
    for tgt in list_of_targets: # "tgt" is the (string) name of the target star
        if tgt not in context.target_list:
            print("LoadExposures: ",tgt," not in ",context.target_list)
        tgt_obj = context.target_list[tgt] # tgt_obj is the Target object
        xform_targets.append(tgt_obj)
        tgt_obj.exposures = []
        tgt_obj.stars = {}            # key is name, value is XStar object
        tgt_obj.points = {}
        xform_measurements = []
        tgt_obj.index = next_index
        next_index += 1
        for (filter, mag_set) in tgt_obj.inst_mags.items():
            for mag_dict in mag_set:
                ################################
                # Create the Exposure objects
                ################################
                exposure = Exposure()
                exposure_juid = mag_dict['exposure']
                exposure.juid = exposure_juid
                exposure.json = context.db_obj.FindExposureByJUID(exposure_juid)
                exposure.filter = filter
                exposure.inst_mags = mag_dict
                exposure.catalog = tgt_obj.catalog
                print("Ingested exposure ", exposure_juid, ' (',filter,'), with dict =')
                #print(mag_dict)

                tgt_obj.exposures.append(exposure)

                ################################
                # Create the XStar objects
                # Create the Measurement objects
                ################################
                for m in mag_dict['measurements']:
                    name = m['name']
                    imag = m['imag']
                    if name not in tgt_obj.stars:
                        star_obj = XStar(name, tgt_obj)
                        tgt_obj.stars[name] = star_obj
                    else:
                        star_obj = tgt_obj.stars[name]

                    meas = Measurement()
                    meas.filter = filter
                    meas.inst_mag = imag
                    if filter not in star_obj.measurements:
                        star_obj.measurements[filter] = []
                    star_obj.measurements[filter] = meas
                    xform_measurements.append(meas)
                    #print("Added measurement to star_obj(",
                    #      star_obj.name, ',', filter, ')')

    ExtractMags()
    LoadPoints()
                        
class Coefficients:
    def __init__(self, name, y_pair, x_pair, reciprocal, grid_loc):
        self.name = name
        self.y_axis_pair = y_pair
        self.x_axis_pair = x_pair
        self.slope = 0.0
        self.slope_err = 0.0
        self.reciprocal = reciprocal
        self.grid_loc = grid_loc
        self.model = None

    def YValue(self, xstar):
        if (self.y_axis_pair[0] not in xstar.mag or
            self.y_axis_pair[1] not in xstar.mag):
            #print("Coef.YValue: ",self.y_axis_pair, xstar.mag)
            return None
        return (xstar.mag[self.y_axis_pair[0]] -
                xstar.mag[self.y_axis_pair[1]])

    def XValue(self, xstar):
        if (self.x_axis_pair[0] not in xstar.mag or
            self.x_axis_pair[1] not in xstar.mag):
            #print("Coef.XValue: ",self.x_axis_pair, xstar.mag)
            return None
        return (xstar.mag[self.x_axis_pair[0]] -
                xstar.mag[self.x_axis_pair[1]])

coefficients = [
    Coefficients('Tbv',('b','v'),('B','V'),True, (0,0)),
    Coefficients('Tvr',('v','r'),('V','R'),True, (0,1)),
    Coefficients('Tri',('r','i'),('R','I'),True, (0,2)),
    Coefficients('Tbr',('b','r'),('B','R'),True, (0,3)),
    Coefficients('Tbi',('b','i'),('B','I'),True, (0,4)),
    Coefficients('Tvi',('v','i'),('V','I'),True, (0,5)),
    Coefficients('Tb_bv',('B','b'),('B','V'),False, (1,0)),
    Coefficients('Tv_bv',('V','v'),('B','V'),False, (2,0)),
    Coefficients('Tr_vr',('R','r'),('V','R'),False, (3,0)),
    Coefficients('Ti_ri',('I','i'),('R','I'),False, (4,0)),
    Coefficients('Tv_vr',('V','v'),('V','R'),False, (2,1)),
    Coefficients('Tr_ri',('R','r'),('R','I'),False, (3,1)),
    Coefficients('Tb_br',('B','b'),('B','R'),False, (1,1)),
    Coefficients('Ti_vi',('I','i'),('V','I'),False, (4,1)),
    Coefficients('Tv_vi',('V','v'),('V','I'),False, (2,2)),
    Coefficients('Tr_vi',('R','r'),('V','I'),False, (3,2)),
    Coefficients('Tb_bi',('B','b'),('B','I'),False, (1,2)),
    ]

class Point:
    def __init__(self):
        self.xy = (None,None)
        self.x_val = None
        self.y_val_unadjusted = None
        self.y_val_adjusted = None
        self.target = None
        self.exclude = False

def LoadPoints():
    print("LoadPoints: starting")
    for target in xform_targets:
        for coef in coefficients:
            for onestar in target.stars.values():
                p = Point()
                p.x_val = coef.XValue(onestar)
                p.y_val_unadjusted = coef.YValue(onestar)
                if p.x_val == None or p.y_val_unadjusted == None:
                    continue # Not a valid point
                p.y_val_adjusted = p.y_val_unadjusted
                p.target = target
                if coef.name not in target.points:
                    target.points[coef.name] = [p]
                else:
                    target.points[coef.name].append(p)
        #for onestar in target.stars.values():
            #print(target.name,onestar.name,coef.name,p.x_val,p.y_val_unadjusted, onestar.mag, onestar.cat_star.ref_mag)

class Model:
    # self.disp_slope and self.disp_intercept give the slope & intercept actually calculated from
    # the graph. self.value holds the *parameter's* slope.
    def __init__(self, sm_results):
        self.sm_results = sm_results
        self.slope = sm_results.params[0]
        self.intercept = sm_results.params[-1]
        self.rsquared = sm_results.rsquared
        self.std_err_slope = sm_results.bse[0]
        self.std_err_intercept = sm_results.bse[-1]
        self.vert_axis_std_err = None # filled in later
        self.display_points = []      # list of Point objects
                
# Return value is dictionary of coefficients
def RunRegressions():
    c_dict = {}                 # key is coefficient name, value is dict with "value" & "err"
    for c in coefficients:
        (model, value, err) = RunOneRegression(c)
        if model == None:
            continue
        AutoFilter(c, model)
        (model, value, err) = RunOneRegression(c)
        if model == None:
            continue
        c_dict[c.name] = { "value" : value,
                           "err" : err,
                           "r_sq" : model.rsquared }
        c.model = model
        c.value = value
        c.std_err = err
    return c_dict

def RunOneRegression(c_set):
    #for tgt in xform_targets:
    #    print("tgt ", tgt.name, ", index = ", tgt.index)
    raw_xy_values = [(p.x_val, p.y_val_unadjusted, p.target.index) for tgt in xform_targets
                     if c_set.name in tgt.points
                     for p in tgt.points[c_set.name] if not p.exclude ]
    print("Have ", len(raw_xy_values), " points to plot for ", c_set.name, ".")
    if len(raw_xy_values) == 0:
        print("Processing ", c_set.name, ", but no points.")
        #print("tgt ", xform_targets[0].name, xform_targets[0].points.keys())
        return (None,None,None)
    py_x_array = []
    py_y_array = [v[1] for v in raw_xy_values]
    for (x,y,i) in raw_xy_values:
        x_vector = [0.0]*(1+len(xform_targets))
        #print("x_vector = ", x_vector)
        x_vector[0] = x
        if i != 0:
            #print("i=",i)
            x_vector[i] = 1.0
        x_vector[-1] = 1.0
        py_x_array.append(x_vector)
    
    x_array = np.array(py_x_array)
    y_array = np.array(py_y_array)
    #model = LinearRegression().fit(x_array, y_array)
    #r_sq = model.score(x_array, y_array)
    #sigma = model.coef_[0]*math.sqrt(((1.0/r_sq)-1.0)/(len(y_array)-2))
    
    #print(model.coef_, sigma)

    sm_model = sm.OLS(y_array, x_array)
    results = sm_model.fit()
    #print(results.summary())
    model = Model(results)

    slope = model.slope

    # Now put in the image (target)-specific offsets
    for tgt in xform_targets:
        offset = 0.0 if tgt.index == 0 else results.params[tgt.index]
        if c_set.name in tgt.points:
            for p in tgt.points[c_set.name]:
                p.y_val_adjusted = p.y_val_unadjusted - offset
    model.display_points = []   # list of Point objects
    model.display_points = [p for tgt in xform_targets if c_set.name in tgt.points
                            for p in tgt.points[c_set.name]]

    err = model.std_err_slope
    model.disp_slope = slope
    model.disp_intercept = results.params[-1]
    if c_set.reciprocal:
        # order of the next two lines matters...
        err = err/(slope*(slope-err))
        slope = 1.0/slope
    model.value = slope
    model.std_err_slope = err

    return (model, slope, err)

def make_plot(c_set):
    points = [p for tgt in xform_targets for p in tgt.points[c_set.name]]
    plotter.MakePlot(points, c_set)


def AutoFilter(c_set, model):
    def Model(x):
        return model.intercept + x*model.slope

    errs = [p.y_val_adjusted - Model(p.x_val) for tgt in xform_targets if c_set.name in tgt.points
            for p in tgt.points[c_set.name]]
    stdev = statistics.stdev(errs)

    excl_count = 0
    for tgt in xform_targets:
        if c_set.name in tgt.points:
            for p in tgt.points[c_set.name]:
                err = p.y_val_adjusted - Model(p.x_val)
                p.exclude = abs(err) > 3*stdev
                if p.exclude:
                    excl_count += 1
    print("AutoFilter: excluded ", excl_count, " points.")

        
        
