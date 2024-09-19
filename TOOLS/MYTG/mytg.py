import sys
from astropy.io import fits
import catalog
import starlist

def get_image_hdu_number(hdul):
    """
    Get index of ImageHDU from HDUList
    """
    for n in [0, 1]:
        if 'EXPOSURE' in hdul[n].header:
            return n
    print("get_image_hdu_number: Cannot find valid HDU keywords")
    raise ValueError

def CanonicalFilter(raw_filter_string):
    return raw_filter_string[0]


class ImageFile:
    def __init__(self, filename):
        self.filename = filename
        self.IStarList = starlist.ImageStarlist(filename)

        with fits.open(filename) as hdul:
            self.image_hdu = get_image_hdu_number(hdul)
            self.filter = CanonicalFilter(hdul[self.image_hdu].header['FILTER'])

        print("ingested ", self.filename, " [", self.filter, "]")

def usage(message = None):
    if message != None:
        print(message)
    print("usage: mytg.py -n fieldname image1.fits image2.fits ...")
    exit(-1)

class OneStar:
    def __init__(self, star_in_starlist):
        self.count = {} # indexed by color
        self.mag_sum = {} # also indexed by color
        self.mag_sumsq = {} # also indexed by color
        self.starname = star_in_starlist.starname

class StarSummary:
    def __init__(self, mag_type, catalog, image_list):
        self.star_dictionary = {}
        self.catalog = catalog
        for i in image_list:
            color = i.filter
            for s in i.IStarList.all_stars:
                if not s.valid: continue
                if s.starname not in self.star_dictionary:
                    self.star_dictionary[s.starname] = OneStar(s)
                this_star = self.star_dictionary[s.starname]
                if color not in this_star.count:
                    this_star.count[color] = 0
                    this_star.mag_sum[color] = 0.0
                    this_star.mag_sumsq[color] = 0.0
                this_star.count[color] += 1
                this_star.mag_sum[color] += s.photometry[mag_type]
                this_star.mag_sumsq[color] += (s.photometry[mag_type]*
                                               s.photometry[mag_type])

    def Print(self):
        for name,star in self.star_dictionary.items():
            cat_star = self.catalog.FindByName(name)
            for color,cat_mag in cat_star.mags.items():
                if color in star.count and star.count[color] > 0:
                    avg_mag = star.mag_sum[color]/star.count[color]
                    stddev = (star.mag_sumsq[color]/star.count[color] -
                              avg_mag*avg_mag)
                    print(name,
                          color,
                          cat_mag,
                          avg_mag,
                          (cat_mag - avg_mag),
                          stddev)
        

def main():
    if sys.argv[1] != "-n":
        usage()
    else:
        fieldname = sys.argv[2]
        AAVSOCatalog = catalog.Catalog(fieldname)

    all_images = [ImageFile(i) for i in sys.argv[3:]]

    for i in all_images:
        i.IStarList.ValidateAgainstCatalog(AAVSOCatalog)
        i.IStarList.AdjustZero("INST", "UNTRANSFORMED", AAVSOCatalog, i.filter)

    ss = StarSummary("UNTRANSFORMED", AAVSOCatalog, all_images)
    ss.Print()

if __name__ == "__main__":
    main()
        
