#!/usr/bin/python2.7

import sys
import os
import re                       # regular expressions
from tempfile import mkstemp
import argparse
import time

parser = argparse.ArgumentParser(description='Change ChartID in a strategy file')
parser.add_argument('-s', help='pathname of strategy file', action='store', nargs=1, required=True, dest="strat_file")
parser.add_argument('-c', help='chart or sequence ID to be inserted', action='store', nargs=1, required=True, dest="new_chart_ID")
cmd_args = parser.parse_args()

fd,filename = mkstemp(dir="/home/ASTRO/STRATEGIES", text=True)
copyfile = open(filename, "w")

print "opening ", cmd_args.strat_file[0]
in_file = open(cmd_args.strat_file[0], "r")

p = re.compile(r'^[\s]*CHART[\s]*=')

for line in in_file:
    m = p.match(line)
    if m: #  matched
        newline = line[:m.end()]+ cmd_args.new_chart_ID[0] + "    # " + time.asctime() + '\n'
    else:
        newline = line
    copyfile.write(newline)

copyfile.close()
       
os.rename(filename, cmd_args.strat_file[0])
print "New file written as ", filename, " and renamed to ", cmd_args.strat_file[0]

