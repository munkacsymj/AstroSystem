#!/usr/bin/python3

import json
import getopt
import sys
import os

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
import PYTHON_LIB.ASTRO_DB_LIB.astro_db

def UnMerge(root_dir):
    db_obj = astro_db.AstroDB(root_dir)
    db = db_obj.GetData()

    merge_sets = [x for x in db['sets'] if x['stype'] == 'MERGE']
    bvri_sets = [x for x in db['sets'] if x['stype'] == 'BVRI']

    print(len(merge_sets), " merge sets found.")
    print(len(bvri_sets), " BVRI sets found.")

    for set in bvri_sets:
        ReWrite_BVRI(set, merge_sets)
    db_obj.Write()
        
def ReWrite_BVRI(bvri_set, merge_sets):
    input_list = bvri_set['input']
    new_list = []
    for i in input_list:
        added_items = [i]
        for x in merge_sets:
            if x['juid'] == i:
                added_items = x['input']
                break
        new_list += added_items
    print('converted ', input_list, ' into ', new_list)
    bvri_set['input'] = new_list

if __name__ == '__main__':
    #print(sys.argv)
    opts,args = getopt.getopt(sys.argv[1:], 'd:', ['dir='])
    root_dir = None
    #print(opts, args)
    for opt,arg in opts:
        #print('option = ', opt, ',    arg = ', arg)
        if opt == '-d':
            root_dir = arg

    if root_dir == None:
        print("usage: unmerge -d /home/IMAGES/1-1-2023")
        sys.exit(-2)

    UnMerge(root_dir)
    
