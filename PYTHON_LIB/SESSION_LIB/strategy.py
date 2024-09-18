import os

STRATEGY_DIR = '/home/ASTRO/STRATEGIES'

class Strategy:
    def __init__(self, starname):
        self.filename = os.path.join(STRATEGY_DIR, starname)+'.str'
        with open(self.filename, 'r') as fp:
            self.strategies = {}
            for x in fp:
                (a,b,c) = x.partition('#')
                a = a.strip()
                if a != '':
                    self.AddLine(a)

    def AddLine(self, line):
        words = line.split('=')
        if len(words) < 2:
            print('Bad strategy line: ' + line)
        else:
            self.strategies[words[0].strip()] = words[1].strip()

    def LookupAsString(self, keyword):
        if keyword in self.strategies:
            return self.strategies[keyword]
        else:
            return None

    def LookupAsNumber(self, keyword):
        s = self.LookupAsString(keyword)
        try:
            v = float(s)
        except ValueError:
            v = None
        return v

    def LookupAsOffset(self, keyword):
        s = self.LookupAsString(keyword)
        north = 0.0
        west = 0.0
        
        words = s.split()
        for w in words:
            x = w.strip()
            if x[-1].upper() == 'N':
                north = float(x[:-1])
            elif x[-1].upper() == 'S':
                north = -float(x[:-1])
            elif x[-1].upper() == 'W':
                west = -float(x[:-1])
            elif x[-1].upper() == 'E':
                west = float(x[:-1])
            else:
                print('Invalid direction character: ' + keyword)
        return (north, west)

    def SaveOffset(self, north, west):
        ns_dir = 'S' if north > 0.0 else 'N'
        ew_dir = 'W' if west > 0.0 else 'E'
        offset_string = ('OFFSET=		' +
                         ('%7.3f' % abs(north)) + ns_dir +
                         ('%7.3f' % abs(west)) + ew_dir) + '\n'
        with open(self.filename, 'r') as fp:
            newlines = []
            for x in fp:
                replacement = x
                (a,b,c) = x.partition('#')
                a = a.strip()
                words = a.split('=')
                if words[0].strip() == 'OFFSET':
                    replacement = offset_string
                newlines.append(replacement)

        with open(self.filename, 'w') as fp:
            fp.writelines(newlines)


                
    
