#!/usr/bin/python2.7

import sys
import math
import os
import shlex
import matplotlib.pyplot as plt

err_filename = "/home/IMAGES/3-12-2017/m67.err"
exists = os.path.isfile

if not exists:
    print "Cannot open error file."
    exit

num_meas = 0
sum_err = 0.0
sum_err_sq = 0.0
sum_snr = 0.0
sum_snr_sq = 0.0
sum_square = 0.0

all_data = []
plot_x = []
plot_y = []

err_file = open(err_filename, "r")
for line in err_file:
    words = shlex.split(line)
    val_err = float(words[0])
    val_recip_snr = float(words[1])
    val_color = words[2]

    all_data.append([val_err, val_recip_snr])

all_data.sort(key = lambda point: point[1])

for point in all_data:
    val_err = point[0]
    val_recip_snr = point[1]

    sum_err_sq += (val_err * val_err)
    num_meas += 1

    plot_x.append(1.0/val_recip_snr)
    rms = math.sqrt(sum_err_sq/num_meas)
    plot_y.append(rms)

plt.plot(plot_x, plot_y, 'r+')
axes_data = plt.axis()
plt.axis([axes_data[1], axes_data[0], axes_data[2], axes_data[3]])
plt.xlabel("SNR limit")
plt.ylabel("RMS Error (mag)")
plt.show()
