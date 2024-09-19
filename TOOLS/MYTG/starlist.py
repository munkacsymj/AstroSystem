import os

class StarInStarlist:
    def __init__(self, starlistline):
        words = starlistline.split()
        self.valid = True # might change later
        self.starname = words[0]
        self.photometry = {}
        photwords = [w for w in words if "Phot=" in w]
        if len(photwords) > 1:
            print("Star ", self.starname, "has too many Phot= entries in starlist.")
        elif len(photwords) < 1:
            pass # self.photometry will remain "None"
        else:
            self.photometry["INST"] = float(photwords[0][5:]) # [5:] will skip Phot= substring

        if self.starname[0] == "S" and self.starname[1:].isdecimal():
            self.valid = False
        if "INST" not in self.photometry:
            self.valid = False

class ImageStarlist:
    def __init__(self, image_filename):
        temp_file = "/tmp/star_list.txt"
        os.system("list_stars -i "+image_filename+ " > " + temp_file)
        with open(temp_file) as s_file:
            self.all_stars = [StarInStarlist(line) for line in s_file.readlines()
                              if len(line) > 20]
        os.remove(temp_file)

    def ValidateAgainstCatalog(self, catalog):
        for s in self.all_stars:
            cat_star = catalog.FindByName(s.starname)
            if s.valid and (cat_star == None or len(cat_star.mags) < 1):
                s.valid = False

    def AdjustZero(self, source, dest, catalog, color):
        sum_errors = 0.0
        sum_count = 0
        for s in self.all_stars:
            if s.valid:
                cat_match = catalog.FindByName(s.starname)
                if color not in cat_match.mags:
                    print("Warning: ", color, " not avail in catalog for ", s.starname)
                else:
                    ref_mag = cat_match.mags[color]
                    err = s.photometry[source] - ref_mag
                    sum_count += 1
                    sum_errors += err
        if (sum_count > 0):
            adjust = sum_errors/sum_count
        else:
            adjust = 0.0

        for s in self.all_stars:
            if s.valid:
                s.photometry[dest] = s.photometry[source] - adjust

if __name__ == "__main__":
    x = ImageStarlist("/home/IMAGES/11-10-2021/image132.fits")
    print("Finished loading starlist.")

    print("Number of valid stars = ",
          len([s for s in x.all_stars if s.valid]))

    print("Validating against catalog")
    import catalog
    c = catalog.Catalog("ngc-7790")
    x.ValidateAgainstCatalog(c)
    
    print("Number of valid stars = ",
          len([s for s in x.all_stars if s.valid]))

