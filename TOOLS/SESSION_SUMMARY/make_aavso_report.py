#!/usr/bin/python3
import gi
gi.require_version('Gdk', '3.0')
gi.require_version('Gtk', '3.0')
from gi.repository import Gtk as gtk
import getopt
import sys
import json
import os

sys.path.insert(1, '/home/mark/ASTRO/CURRENT')
from PYTHON_LIB.IMAGE_LIB import star
from PYTHON_LIB.ASTRO_DB_LIB import astro_db
from PYTHON_LIB.ASTRO_DB_LIB.util import FindJUID
from PYTHON_LIB.ASTRO_DB_LIB import util
from PYTHON_LIB.IMAGE_LIB import star
from PYTHON_LIB.IMAGE_LIB import filter

import aavso_report
import comp_analy
import SessionGlobal

def generate_aavso_report(root_dir):
    if SessionGlobal.db_obj is None or SessionGlobal.db is None:
        db_obj = astro_db.AstroDB(root_dir)
        db = db_obj.GetData()
        SessionGlobal.homedir = root_dir
        SessionGlobal.db_obj = db_obj
        SessionGlobal.db = db
    else:
        db = SessionGlobal.db
        db_obj = SessionGlobal.db_obj
        root_dir = SessionGlobal.homedir

    output_filename = os.path.join(root_dir, 'aavso.report.txt')
    
    # fetch existing submissions
    if 'submissions' not in db:
        return
    submissions = db['submissions'] # submissions is a list of dictionaries
    submissions.sort(key=lambda x: float(x['computed']['time']))

    # Set up the header
    header_command = 'bvri_report -h -o /tmp/bvri_header.txt'
    os.system(header_command)
    with open('/tmp/bvri_header.txt', 'r') as fp_header:
        header_lines = fp_header.readlines()

    with open(output_filename, 'w') as fp_out:
        for line in header_lines:
            fp_out.write(line)

        # Now write the data
        for one_submission in submissions:
            if 'inhibit' in one_submission and one_submission['inhibit'] != 0:
                continue # skip this entry
            source = 'computed'
            if 'use_override' in one_submission and one_submission['use_override'] != 0:
                source = 'override'

            dld = aavso_report.DataLineDict(None, json=one_submission[source])
            fp_out.write(dld.ToString()+'\n')
            print(dld.value['report_name'], '(', dld.value['filter'], ')')
        
    
if __name__ == "__main__":
    opts,args = getopt.getopt(sys.argv[1:], 'd:', ['dir='])
    root_dir = None
    #print(opts, args)
    for opt,arg in opts:
        #print('option = ', opt, ',    arg = ', arg)
        if opt == '-d':
            root_dir = arg

    if root_dir == None:
        print("usage: make_aavso_report -d /home/IMAGES/1-1-2023")
        sys.exit(-2)

    generate_aavso_report(root_dir)
