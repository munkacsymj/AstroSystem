

class CatalogStar:
    def __init__(self, catalog_line):
        words = catalog_line.split()
        if len(words) < 4:
            print("CatalogStar: bad catalog_line: ", catalog_line)
            return

        self.name = words[0]
        self.dec_radians = words[1]
        self.ra_radians = words[2]
        self.simple_mag = words[3]

        self.check = False
        self.comp = False
        self.mags = {}
        self.visual_mag = None
        self.auid = None

        for word in words[4:]:
            if word == "CHECK":
                self.check = True
            elif word == "COMP":
                self.comp = True
            elif '=' in word:
                subwords = word.split('=')
                keyword = subwords[0]
                value = subwords[1]

                if keyword == "MV":
                    self.visual_mag = float(value)
                elif keyword == "AUID":
                    self.auid = value
                elif keyword == "REPORT":
                    pass # ignore
                elif keyword[0] == "P" and len(keyword) == 2:
                    handled = False
                    for filter in ["U","B","V","R","I"]:
                        if keyword == "P"+filter:
                            handled = True
                            self.mags[filter] = float(value)
                    if not handled:
                        print("CatalogStar: funny color? : ", keyword)
                elif keyword == "WIDE":
                    pass # ignore
                else:
                    print("CatalogStar: Unrecognized keyword: ", keyword)
        
class Catalog:
    def __init__(self, fieldname):
        with open("/home/ASTRO/CATALOGS/"+fieldname) as c_file:
            self.all_stars = [CatalogStar(line) for line in c_file.readlines()
                              if (len(line)-line.count(' ')) > 16]

    def FindByName(self, starname):
        for s in self.all_stars:
            if s.name == starname:
                return s
        return None
    
if __name__ == "__main__":
    x = Catalog("ngc-7790")
    print("Finished loading test catalog.")

    print("Number of stars with photometry = ",
          len([s for s in x.all_stars if len(s.mags) > 1]))
