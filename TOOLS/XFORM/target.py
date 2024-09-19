import sys

sys.path.insert(1, '/home/mark/ASTRO/CURRENT')
#from PYTHON_LIB.ASTRO_DB_LIB import util
from PYTHON_LIB.IMAGE_LIB import star as star_mod
from PYTHON_LIB.IMAGE_LIB import filter
from PYTHON_LIB.ASTRO_DB_LIB import astro_db

import context

class Target:
    def __init__(self, targetname):
        self.name = targetname
        self.image_juids = []
        self.exposures = []     # List of xforms.Exposure objects
        self.stars = {}         # key is starname; value is xforms.XStar
        self.points = {}        # key is Coefficient name; valueis is list of xforms.Point objects
        self.inst_mags = {}     # key is filter, value is list of inst_mag dictionaries
        self.catalog = dict([(x.name, x) for x in star_mod.ReadCatalog(targetname)])
        self.index = 0

    def AddInstMags(self, filter, mag_set):
        if filter not in self.inst_mags:
            self.inst_mags[filter] = []
        self.inst_mags[filter].append(mag_set)


def BuildTargetLists():
    print("Starting BuildTargetLists()")
    context.target_list = {}
    inst_mags = context.db['inst_mags']
    for mag_set in inst_mags:   # mag_set is a dictionary
        exposure_juid = mag_set['exposure']
        if astro_db.JUIDToTopLevelName(exposure_juid) != 'stacks':
            continue
        exposure = context.db_obj.FindExposureByJUID(exposure_juid)
        targetname = exposure['target']
        filtername = filter.ToCanonical(exposure['filter'])

        if targetname not in context.target_list:
            context.target_list[targetname] = Target(targetname)
            print("BuildTargetLists(): found ", targetname)

        this_target = context.target_list[targetname]
        this_target.AddInstMags(filtername, mag_set)
    print("context.target_list has ", len(context.target_list), " entries.")
