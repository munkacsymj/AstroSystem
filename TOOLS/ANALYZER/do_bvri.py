#!/usr/bin/python3

import getopt
import sys
import json

sys.path.insert(1, '/home/mark/ASTRO/CURRENT')
from PYTHON_LIB.IMAGE_LIB import star
from PYTHON_LIB.ASTRO_DB_LIB import astro_db
from PYTHON_LIB.ASTRO_DB_LIB.util import FindJUID
from PYTHON_LIB.ASTRO_DB_LIB import util, astro_directive
from PYTHON_LIB.IMAGE_LIB import star
from PYTHON_LIB.IMAGE_LIB import filter

import per_image
import per_source # a source is an ASTRO-DB set (stack or subexp)
import context as context_module
import key_stars
import context
import diff_analysis

global all_stars

def DoBVRI(root_dir, target):
    db_obj = astro_db.AstroDB(root_dir)
    db = db_obj.GetData()
    
    # Find the target set for the target named 'target'
    tgt_set = [x for x in db['sets'] if x['stype'] == 'TARGET' and x['target'] == target]
    if len(tgt_set) != 1:
        if len(tgt_set) == 0:
            print("DoBVRI: target ", target, " not found.")
            sys.exit(-2)
        print("DoBVRI: target ", target, " had multiple matches?????")
        sys.exit(-2)
    # The set of 'input' to the target will be the BVRI set
    ProcessTarget(root_dir, db_obj, db, tgt_set[0])

def ProcessTarget(root_dir, db_obj, db, tgt):
    if tgt['stype'] == 'TARGET':
        for target in tgt['input']:
            ProcessTarget(root_dir, db_obj, db, FindJUID(db, 'sets', target))
    else:
        DoBVRISet(root_dir, db_obj, tgt)

def DoBVRISet(root_dir, db_obj, bvri):
    db = db_obj.data
    global all_stars
    # From the BVRI set, get the directive and the inputs (stack sets or subexp sets)
    if 'directive' not in bvri:
        print("Skipping this DoBVRISet")
        return
    directive_juid = bvri['directive']
    directive = FindJUID(db, 'directives', directive_juid)
    context.Setup(root_dir, directive, db_obj)
    inputs = bvri['input']

    # Now get the (approx 8) top-level sets for this BVRI set. (two per color)
    top_level_sets = [FindJUID(db, 'sets', x) for x in inputs]
    all_colors = set() # These are two-letter colors (e.g., 'Vc')
    images = [] # Instances of class PerImage
    for s in top_level_sets:
        all_colors.add(filter.to_canonical[s['filter']])
        for x in s['input']:
            pi = per_image.PerImage(db,x)
            if pi.inst_mags is not None:
                images.append(pi)

    target = images[0].target
    context.target = target
    
    print("BVRI: target is ", target)
    print("top_level_sets is ", top_level_sets)
    all_stars = star.ReadCatalog(target)
    context.catalog = dict([(x.name,x) for x in all_stars])
    
    print("catalog has ", len(all_stars), " stars.")

    directive_obj = astro_directive.Directive(db, directive)
    context.key_stars = key_stars.KeyStars(directive_obj, target)
    merged_stars = {} # indexed by color; dictionary of dictionaries
                      # (the stars are indexed by gstar reference

    ################################
    # Apply extinction corrections
    ################################
    try:
        output_csv = open('/tmp/extinction.csv', 'w')
    except OSError:
        output_csv = None

    colors_to_exclude = context.directive.ColorsToExclude()
    images_to_exclude = context.directive.ImagesToExclude() # list of JUIDs
    for i in images: # i is a PerImage
        this_color = filter.to_canonical[i.filter]
        if this_color in colors_to_exclude:
            continue
        if i.juid in images_to_exclude:
            continue
        i.FetchInstMags()
        context.key_stars.ConsiderObservations(i)
        if output_csv is not None and not i.stack:
            i.AddExtinctionStars(output_csv)

        i.CalculateZero(context.key_stars)
        if i.zero is not None:
            i.CalculateUntransformedMags()
            if this_color not in merged_stars:
                merged_stars[this_color] = {}
            i.AddToMerge(merged_stars[this_color])
            print("AddToMerge: adding image ", i.juid)

    if output_csv is not None:
        output_csv.close()      # only needed for extinctions

    context.key_stars.SelectCheckStars()

    ################################
    ## Merge results into MStars and
    ## create BVRIStars
    ################################
    #print(merged_stars)
    bvri_stars = {}             # index is gstar, value is bvri_star
    for f in all_colors:
        if f not in merged_stars:
            continue
        print("Performing merges for ", f)
        for merge in merged_stars[f].values():
            merge.DoMerge()
            if merge.gstar not in bvri_stars:
                bvri_stars[merge.gstar] = star.BVRIStar(merge)
            else:
                bvri_stars[merge.gstar].sources[merge.filter] = merge
            merge.group = bvri_stars[merge.gstar].group
    print("bvri_stars{} has ", len(bvri_stars), " entries.")
    bvri_counts = [len(x.sources) for x in bvri_stars.values()]
    count_summary = [sum(1 for x in bvri_counts if x == n) for n in [1,2,3,4]]
    print(count_summary)

    ################################
    ## Perform color transformations
    ################################
    print("Ref_Color = ", context.ref_color)
    for (g_star,bvri_star) in bvri_stars.items():
        filter.ImgColorTransform(bvri_star,
                                 context.ref_color,
                                 g_star)

    ################################
    ## Compute Statistics on the Check Stars
    ################################
    #context.key_stars.ComputeCheckStatistics(bvri_stars)

    ################################
    ## Create AnalysisSets (which are
    ## what actually go into astro_db)
    ################################
    analy_sets = diff_analysis.BVRIAnalysisSet(db,bvri,images)
    for b in bvri_stars.values():
        for s in b.sources.values():
            analy_sets.AddMeasurement(s)
    analy_sets.Close()
    db_obj.Write()

    per_image.error_table.PrintAsCSV('/tmp/ensemble_fitting.csv')
        
if __name__ == '__main__':
    #print(sys.argv)
    opts,args = getopt.getopt(sys.argv[1:], 't:d:', ['dir='])
    root_dir = None
    target = None
    #print(opts, args)
    for opt,arg in opts:
        #print('option = ', opt, ',    arg = ', arg)
        if opt == '-d':
            root_dir = arg
        elif opt == '-t':
            target = arg

    if root_dir == None:
        print("usage: do_bvri.py -d /home/IMAGES/1-1-2023")
        sys.exit(-2)

    DoBVRI(root_dir, target)
