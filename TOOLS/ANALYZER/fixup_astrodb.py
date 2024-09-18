#!/usr/bin/python3

import sys
import json

sys.path.insert(1, '/home/mark/ASTRO/CURRENT')
from PYTHON_LIB.ASTRO_DB_LIB import astro_db

num_filter_fixes = 0

def main():
    dir = '/home/IMAGES/1-2-2024'
    db_obj = astro_db.AstroDB(dir)

    ################################
    #   Test: Missing "Submissions"
    # section
    ################################

    print("Submissions section check ...", end='')
    if 'submissions' not in db_obj.data:
        print(" failed. Fixing.")
        db_obj.data['submissions'] = []
    else:
        print(" passed.")

    ################################
    #   Fix non-canonical filter names
    ################################
    FixFilterNames(db_obj.data)
    print("Made ", num_filter_fixes, " filter name repairs.")

    ################################
    # Done: Save all changes.
    ################################
    db_obj.Write()


def FixFilterNames(node):
    global num_filter_fixes
    if isinstance(node, list):
        for x in node:
            FixFilterNames(x)
    elif isinstance(node, dict):
        for (x,val) in node.items():
            if x.upper() == 'FILTER':
                if val in ['Bc','Vc','Rc','Ic']:
                    node[x] = val[0]
                    num_filter_fixes += 1
            elif isinstance(val, list):
                for x in val:
                    FixFilterNames(x)
            elif isinstance(val, dict):
                FixFilterNames(val)
        
if __name__ == '__main__':
    main()
        
