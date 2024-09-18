#!/usr/bin/python3

import getopt
import sys
import json

sys.path.insert(1, '/home/mark/ASTRO/CURRENT')
from PYTHON_LIB.IMAGE_LIB import star
from PYTHON_LIB.ASTRO_DB_LIB import astro_db
from PYTHON_LIB.ASTRO_DB_LIB.util import FindJUID
from PYTHON_LIB.ASTRO_DB_LIB import util, astro_directive

def ListTargetNames(root_dir):
    db_obj = astro_db.AstroDB(root_dir)
    db = db_obj.GetData()
    
    tgt_set = [x for x in db['sets'] if x['stype'] == 'TARGET']
    for tgt in tgt_set:
        print(tgt['target'])

if __name__ == '__main__':
    opts,args = getopt.getopt(sys.argv[1:], 'd:', ['dir='])
    root_dir = None
    for opt,arg in opts:
        if opt == '-d':
            root_dir = arg

    if root_dir == None:
        print("usage: list_target_names.py -d /home/IMAGES/1-1-2023")
        sys.exit(-2)

    ListTargetNames(root_dir)
