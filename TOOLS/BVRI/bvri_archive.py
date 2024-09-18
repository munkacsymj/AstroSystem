#!/usr/bin/python3

from os import listdir
from os import path
import sys
import getopt
import re
import shutil

date_regex = re.compile(r'\d{1,2}-\d{1,2}-\d{4}')

archive_dir = '/home/ASTRO/ARCHIVE'

def GetExistingArchives():
    return RecursiveGetArchives(archive_dir)

def RecursiveGetArchives(d):
    def is_archive(f):
        (root, ext) = path.splitext(f)
        return path.isfile(f) and ext == ".db"

    allfiles = [path.join(d,f) for f in listdir(d)
                if is_archive(path.join(d, f))]

    alldirs = [path.join(d,d2) for d2 in listdir(d) if path.isdir(path.join(d,d2))]

    for d3 in alldirs:
        allfiles.extend(RecursiveGetArchives(d3))
    return allfiles

def ArchiveSingleDir(d):
    global existing_archive_files
    sourcefile = path.join(d, "bvri.db")
    if not path.isfile(sourcefile):
        #print('Nothing found: ', sourcefile)
        return

    # Find the date associated with this directory
    np = path.normpath(d)
    candidate_dirs = np.split('/')[-2:]
    #print(candidate_dirs)
    for d1 in candidate_dirs:
        candidate = date_regex.search(d1)
        if candidate != None:
            # match found
            destbase = 'bvri_'+d1+'.db'
            if destbase not in existing_archive_files:
                destfile = path.join(archive_dir, destbase)
                print('Copying ', sourcefile, ' to ', destfile)
                shutil.copy2(sourcefile, destfile)
                         
            return

def main(argv):
    global existing_archive_paths
    global existing_archive_files
    force_update = False
    singledir = None
    try:
        opts, args = getopt.getopt(argv, "ad:")
    except getopt.GetoptError:
        print('bvri_archive.py [-a|-d /home/IMAGES/date]')
        sys.exit(2)
    for opt,arg in opts:
        if opt == '-a':
            singledir = ''
        elif opt == '-d':
            singledir = arg
    if singledir == None:
        print('bvri_archive.py [-a|-d /home/IMAGES/date]')
        sys.exit(2)

    #print('singledir = ', singledir)
    existing_archive_paths = GetExistingArchives()
    existing_archive_files = [path.basename(d) for d in existing_archive_paths]
    if singledir == '':
        # look at all the bvri.db files in /home/IMAGES/dates
        for f in listdir('/home/IMAGES/'):
            fullpath = path.join('/home/IMAGES', f)
            if path.isdir(fullpath):
                ArchiveSingleDir(fullpath)
    else:
        ArchiveSingleDir(singledir)
        
                
#print(GetExistingArchives())

if __name__ == "__main__":
    main(sys.argv[1:])



