#!/usr/bin/python2.7

import sys
import math
import os
import shlex

err_filename = "/home/IMAGES/4-17-2017/m67_iraf.err"
exists = os.path.isfile

if not exists:
    print "Cannot open error file."
    exit

num_meas = {}
sum_err = {}
sum_err_sq = {}
sum_snr = {}
sum_snr_sq = {}
sum_square = {}
    
for c in ["B", "V", "R", "I", "BVRI"]:
    num_meas[c] = 0
    sum_err[c] = 0.0
    sum_err_sq[c] = 0.0
    sum_snr[c] = 0.0
    sum_snr_sq[c] = 0.0
    sum_square[c] = 0.0

err_file = open(err_filename, "r")
for line in err_file:
    words = shlex.split(line)
    val_err = float(words[0])
    val_recip_snr = float(words[1])
    val_color = words[2]

    sum_err[val_color] += val_err
    sum_err["BVRI"] += val_err
    sum_err_sq[val_color] += (val_err * val_err)
    sum_err_sq["BVRI"] += (val_err * val_err)
    sum_snr[val_color] += val_recip_snr
    sum_snr["BVRI"] += val_recip_snr
    sum_snr_sq[val_color] += (val_recip_snr * val_recip_snr)
    sum_snr_sq["BVRI"] += (val_recip_snr * val_recip_snr)
    sum_square[val_color] += (val_recip_snr * val_err)
    sum_square["BVRI"] += (val_recip_snr * val_err)
    num_meas[val_color] += 1
    num_meas["BVRI"] += 1

#    if val_color == "V":
#        print "x = ", val_recip_snr, ", y = ", val_err

for c in ["B", "V", "R", "I", "BVRI"]:
    n = num_meas[c]
    sum_x = sum_snr[c]
    sum_x2 = sum_snr_sq[c]
    sum_y = sum_err[c]
    sum_y2 = sum_err_sq[c]
    sum_xy = sum_square[c]

    alpha = (n * sum_xy - sum_x * sum_y)/(n * sum_x2 - sum_x * sum_x)
    beta = (sum_y * sum_x2 - sum_x * sum_xy)/(n * sum_x2 - sum_x * sum_x)
    r = (n * sum_xy - sum_x * sum_y)/((n * sum_x2 - sum_x * sum_x)*
                                      (n * sum_y2 - sum_y * sum_y))

    print "For ", c, ", y = ", alpha, " * (1/SNR) + ", beta
    print "r^2 = ", r*r
    
