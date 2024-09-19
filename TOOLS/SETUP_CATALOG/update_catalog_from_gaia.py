#!/usr/bin/python3

import getopt
import sys
import csv
import math

global cos_dec
cos_dec = 1.0 # updated later

class Catalog_Star:
    def __init__(self, cat_line):
        self.orig_line = cat_line
        words = cat_line.split(maxsplit=3)
        self.starname = words[0]
        self.orig_ra = float(words[2])
        self.orig_dec = float(words[1])
        self.rest_of_line = words[3]
        self.best_gaia = None

    def WriteString(self):
        if self.best_gaia == None:
            return (self.orig_line)
        else:
            return (self.starname + ' ' +
                    "{:.10f}".format(self.best_gaia.gaia_dec_radians) + ' ' +
                    "{:.10f}".format(self.best_gaia.gaia_ra_radians) + ' ' +
                    self.rest_of_line)

class Catalog:
    def __init__(self, catalog_name):
        filename = '/home/ASTRO/CATALOGS/' + catalog_name
        self.stars = []
        with open(filename) as f:
            content = f.readlines()
            
        for line in content:
            self.stars.append(Catalog_Star(line))

        print("Found ", len(self.stars), " stars in catalog of ", catalog_name)

    def Write(self):
        with open("/tmp/catalog.update", "w") as f:
            for star in self.stars:
                f.write(star.WriteString())

class Gaia_Star:
    def __init__(self, cat_line_words):
        self.gaia_name = cat_line_words[0]
        self.gaia_ra_deg = float(cat_line_words[1])
        self.gaia_dec_deg = float(cat_line_words[2])
        self.gaia_dec_radians = self.gaia_dec_deg * math.pi/180.0
        self.gaia_ra_radians = self.gaia_ra_deg * math.pi/180.0
        self.number_of_matches = 0
        

class Gaia_Catalog:
    def __init__(self, catalog_name):
        global cos_dec
        self.stars = []
        with open(catalog_name) as csv_file:
            csv_reader = csv.reader(csv_file, delimiter=',')
            line = 0 # skip the first line (header)
            for row in csv_reader:
                if line != 0:
                    self.stars.append(Gaia_Star(row))
                else:
                    line += 1

        print("Found ", len(self.stars), " stars in Gaia catalog.")
        cos_dec = math.cos(self.stars[0].gaia_dec_radians)

def distance(my_star, gaia_star):
    global cos_dec
    del_dec = abs(my_star.orig_dec - gaia_star.gaia_dec_radians)
    #print("del_dec = ", del_dec, " [ ", my_star.orig_dec, ", ", gaia_star.gaia_dec_radians, ']')
    if del_dec > (5.0/3600.0)*math.pi/180.0:
        # outlandish delta dec means no way is this correct match
        return del_dec
    del_ra = abs(my_star.orig_ra - gaia_star.gaia_ra_radians)*cos_dec
    #print("del_ra = ", del_ra, " [ ", my_star.orig_ra, ", ", gaia_star.gaia_ra_radians, ']')
    if del_ra > (5.0/3600.0)*math.pi/180.0:
        return del_ra
    return math.sqrt(del_dec*del_dec + del_ra*del_ra)

def FindMatches(my_cat, gaia_cat):
    for mystar in my_cat.stars:
            closest_distance = 999.9
            for gaia in gaia_cat.stars:
                this_distance = distance(mystar, gaia)
                if this_distance < closest_distance:
                    closest_distance = this_distance
                    mystar.best_gaia = gaia
            if closest_distance > (2.0/3600.0)*math.pi/180.0:
                mystar.best_gaia = None
                print("No match found for ", mystar.starname)
            else:
                mystar.best_gaia.number_of_matches += 1

    num_unmatched = 0
    num_single_match = 0
    num_multiple_match = 0
    for gaia in gaia_cat.stars:
        if gaia.number_of_matches == 0:
            num_unmatched += 1
        elif gaia.number_of_matches == 1:
            num_single_match += 1
        else:
            num_multiple_match += 1
    print("Gaia unmatched = ", num_unmatched)
    print("Gaia perfect match = ", num_single_match)
    print("Gaia multiple match = ", num_multiple_match)

def usage():
    print("Usage: update_catalog_from_gaia -n starname -g gaia_file.csv")
    exit(2)

def main():
    try:
        opts, args = getopt.getopt(sys.argv[1:], "n:g:")
    except getopt.GetoptError as err:
        print(err)
        sys.exit(2)
    catalog_name = None
    gaia_filename = None

    for o,a in opts:
        if o == "-n":
            catalog_name = a
        elif o == "-g":
            gaia_filename = a
        else:
            assert False, "unhandled option"

    if catalog_name == None or gaia_filename == None:
        usage()
        sys.exit(2)

    c = Catalog(catalog_name)
    g = Gaia_Catalog(gaia_filename)
    FindMatches(c, g)
    c.Write()

if __name__ == "__main__":
    main()

