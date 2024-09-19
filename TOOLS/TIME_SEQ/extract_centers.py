#!/usr/bin/python3
import sys
import os
import statistics

# command line is list of image files
filelist = sys.argv[1:]
for onefile in filelist:
    command = "fits_keywords " + onefile + " > /tmp/keywords.txt"
    os.system(command)
    with open("/tmp/keywords.txt", "r") as fp:
        corner_ra = []
        corner_dec = []
        dtime = None
        exposure = None
        
        for oneline in fp:
            #print(oneline)
            if "WCS" in oneline and "WCSTYPE" not in oneline:
                words = oneline.split('=')
                keyword_stuff = words[0].split(' ')
                keyword = keyword_stuff[1]
                coord = keyword[5:]
                value = float(words[1].split()[0])
                #print(keyword, coord, value)
                if coord == "DEC":
                    corner_dec.append(value)
                else:
                    corner_ra.append(value)

            if "EXPOSURE" in oneline:
                words = oneline.split('=')
                exposure = float(words[1].split()[0])

            if "DATE-OBS" in oneline:
                words = oneline.split('=')
                value_string = words[1].split()[0].replace("'","")
                words = value_string.split('T')
                hms = words[1].split(':')
                dtime = float(hms[0]) + float(hms[1])/60.0 + float(hms[2])/3600.0

        print(corner_ra, corner_dec)
        print(dtime + (exposure/7200.0), statistics.mean(corner_ra), statistics.mean(corner_dec))
        
                

                
        
