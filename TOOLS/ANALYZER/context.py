import sys

sys.path.insert(1, '/home/mark/ASTRO/CURRENT')
from PYTHON_LIB.ASTRO_DB_LIB import astro_directive


ensemble_exclusions = [] # (name,filter) pairs
image_exclusions = [] # 
check_exclusions = [] # (name,filter) pairs
filter_exclusions = [] # canonical filter names
use_ensemble = None
do_transform = None
pre_transform = True # transform as part of zero-point calc
target = None
catalog = None
directive = None
extinction_coefficients = {} # indexed by canonical filter names
ref_color = None
db = None
db_json = None
root_dir = None

def Setup(root_directory, root_directive, database):
    global use_ensemble
    global do_transform
    global directive
    global root_dir
    global db, db_json

    root_dir = root_directory
    db = database
    db_json = db.data

    if root_directive != None:
        directive = astro_directive.Directive(db_json, root_directive)
        ref_color = None
        if 'use_ensemble' in directive.directive:
            use_ensemble = directive.directive['use_ensemble']
        else:
            use_ensemble = True
        if 'do_transform' in directive.directive:
            do_transform = directive.directive['do_transform']
        else:
            do_transform = True

            
