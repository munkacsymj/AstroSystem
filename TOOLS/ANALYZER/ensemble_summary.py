#!/usr/bin/python3
import statistics

class OneFit:
    def __init__(self, full_line):
        words = full_line.split(',')
        self.starname = words[1].strip()
        self.filter = words[2].strip()
        self.err = float(words[3].strip())

class FitSum:
    def __init__(self, one_fit):
        self.starname = one_fit.starname
        self.filter = one_fit.filter
        self.errs = [one_fit.err]
        self.mean_err = None

class Star:
    def __init__(self, name):
        self.starname = name
        self.errs = {} # indexed by filter, value is list of errs

class Color:
    def __init__(self, filter):
        self.filter = filter
        self.errs = []
        
def Summarize_ensemble():
    with open('/tmp/bvri.out','r') as fp:
        points = [OneFit(l) for l in fp.readlines() if 'EFE,' in l]

    summary_points = [] # list of FitSum
    stars = {} # indexed by starname, value is Star
    colors = {} # indexed by filter, value is Color

    for one in points:
        match = next((x for x in summary_points if x.starname == one.starname and
                      x.filter == one.filter), None)
        if match == None:
            summary_points.append(FitSum(one))
        else:
            match.errs.append(one.err)

    for x in summary_points:
        x.mean_err = statistics.mean(x.errs)
        if x.starname not in stars:
            stars[x.starname] = Star(x.starname)
        if x.filter not in stars[x.starname].errs:
            stars[x.starname].errs[x.filter] = [x.mean_err]
        else:
            stars[x.starname].errs[x.filter].append(x.mean_err)
                                 
        if x.filter not in colors:
            colors[x.filter] = Color(x.filter)
        colors[x.filter].errs.append(x.mean_err)

    output = 'Starname,'
    for color in colors:
        output += (color + ',')
    print(output)
    for star in stars.values():
        output = star.starname+','
        for color in colors:
            if color in star.errs:
                output += (str(statistics.mean(star.errs[color])) + ',')
            else:
                output += ','
        print(output)
        

    

if __name__ == '__main__':
    Summarize_ensemble()
