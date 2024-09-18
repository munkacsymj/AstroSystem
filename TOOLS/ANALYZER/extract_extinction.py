#!/usr/bin/python3

import numpy as np
from sklearn.linear_model import LinearRegression
import getopt
import math
import sys
import os
import json
from util import FindJUID
import star
import per_image
import per_source # a source is an ASTRO-DB set (stack or subexp)
import context as context_module
import key_stars
import filter
import context

sys.path.insert(1, '/home/mark/ASTRO/CURRENT/TOOLS/SESSION_SUMMARY')
import astro_db

all_sequences = {}

def PreprocessSetup(root_dir):
    db_obj = astro_db.AstroDB(root_dir)
    db = db_obj.GetData()

    commands = []

    for i_mag_set in db['inst_mags']:
        exp_juid = i_mag_set['exposure']
        if exp_juid > 3000000:
            continue # skip images that are stacks
        if 'psf_1' not in i_mag_set:
            img = FindJUID(db, 'exposures', exp_juid)
            imagename = img['filename']
            flatname = img['flat']
            darkname = img['dark']
            
            commands.append('photometry ' +
                            ' -i ' + imagename +
                            ' -s ' + flatname +
                            ' -d ' + darkname)
    return commands

def ExtractExtinction(root_dir):
    db_obj = astro_db.AstroDB(root_dir)
    db = db_obj.GetData()
    x_arrays = {}
    y_arrays = {}
    exposures = {}

    for i_mag_set in db['inst_mags']:
        exp_juid = i_mag_set['exposure']
        if exp_juid > 3000000:
            print("Skipping stacked image ", exp_juid)
            continue # skip images that are stacks
        measurements = i_mag_set['measurements']
        exp_time = i_mag_set['exposure']
        mag_adjust = -2.5*math.log10(exp_time)
        target = FindJUID(db, 'exposures', exp_juid)['target']
        img_filter = filter.to_canonical[i_mag_set['filter']]
        if img_filter not in x_arrays:
            x_arrays[img_filter] = []
            y_arrays[img_filter] = []
            exposures[img_filter] = []
        #print("Working ", target, ' ', img_filter)
        num_stars = 0
        catalog = GetSequence(target)
        for m in measurements:
            cat_star = next((x for x in catalog if x.name == m['name']), None)
            #print("    ", m['name'], ' cat = ', cat_star, " mags = ", cat_star.ref_mag)
            if cat_star is not None and img_filter in cat_star.ref_mag:
                x_arrays[img_filter].append(m['airmass'])
                y_arrays[img_filter].append(m['imag']-mag_adjust-cat_star.ref_mag[img_filter][0])
                exposures[img_filter].append(exp_juid)

    for color in x_arrays.keys():
        x_dat = x_arrays[color]
        y_dat = y_arrays[color]
        errors = y_dat # just creating a placeholder of the correct size
        biglist = list(zip(x_dat, y_dat, errors, exposures[color]))
        trimlist = biglist
        counter = 4
        model = None
        slope = None
        x = None
        y = None
        while counter > 0:
            counter -= 1
            
            biglist = trimlist
            z = list(zip(*biglist))
            x_dat = list(z[0])
            y_dat = list(z[1])
            exp_info = list(z[3])
            x = np.array(x_dat).reshape((-1,1))
            y = np.array(y_dat)
            model = LinearRegression().fit(x,y)
            slope = model.coef_
            intercept = model.intercept_
            y_model = intercept + slope*np.array(x_dat)
            errors = y_dat - y_model
            stddev = np.std(errors)

            biglist = list(zip(x_dat, y_dat, errors, exp_info))
            trimlist = [x for x in biglist if abs(x[2]) < 1*stddev]
            print("Orig list has ", len(x_dat), " points. Trimmed list has ",
                  len(trimlist), " points.")

            
        r_sq = model.score(x,y)
        sigma = model.coef_*math.sqrt(((1.0/r_sq)-1.0)/(len(x_dat)-2))
        print("Extinction coefficient for ", color, " is ", model.coef_, " +/- ",
              sigma)
        with open('/tmp/extinction_'+color+'.csv', 'w') as fp:
            for t in biglist:
                fp.write(str(t[0])+','+str(t[1])+','+str(t[3])+'\n')

                         
def GetSequence(target):
    global all_sequences
    if target not in all_sequences:
        all_sequences[target] = star.ReadCatalog(target)

    return all_sequences[target]

if __name__ == '__main__':
    #print(sys.argv)
    opts,args = getopt.getopt(sys.argv[1:], 'd:', ['dir='])
    root_dir = None
    target = None
    #print(opts, args)
    for opt,arg in opts:
        #print('option = ', opt, ',    arg = ', arg)
        if opt == '-d':
            root_dir = arg

    if root_dir == None:
        print("usage: extract_extinction.py -d /home/IMAGES/1-1-2023")
        sys.exit(-2)

    commands = PreprocessSetup(root_dir)
    if len(commands) > 0:
        print('Running preprocess commands:')
        for command in commands:
            print('Running ', command)
            os.system(command)

    ExtractExtinction(root_dir)
