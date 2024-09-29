#!/usr/bin/python2.7
import math
import re
import sys

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print "Usage: bvri_summarize.py $D/aavso.report"
        sys.exit()

    bvri_file = open(str(sys.argv[1]), 'r')
    sum_err = 0.0
    num_entries = 0
    for line in bvri_file:
        words = line.split(",")
        if len(words) < 8:
            continue
        # color is in words[4]
        color = words[4]
        if color != "V":
            continue
        details = words[14]
        rms_search = re.search('CHKERRRMS=([0-9.]+)', details)
        if rms_search == None:
            #print "Line has no CHKERRRMS: ", details
            continue
        rms_value = rms_search.group(1)
        #print rms_value
        this_err = float(rms_value)
        sum_err += this_err # (this_err*this_err)
        num_entries += 1

    print "Average RMS error in ", str(sys.argv[1]), ": ", sum_err/num_entries
    

        
