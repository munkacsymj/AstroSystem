#!/usr/bin/python3

import sys
import os
import matplotlib.pyplot as plt
import numpy as np

A = 0.0
R = 0.0
B = 0.0

def hyperbola(x_values):
    xr_values = R - x_values
    y_values = A*np.sqrt(1 + np.multiply(xr_values, xr_values)/(B*B))
    return y_values

def GetHyperbola(logfilename):
    global A, R, B
    shellfilename = logfilename.replace('.log','.shell')
    fp = open(shellfilename, "r")
    all_hyp = [ x for x in fp.readlines() if ', R = ' in x ]

    final_hyp = all_hyp[-1]
    ## final_hyp looks like: 'A = 0.050000, R = 802.386613'
    words = final_hyp.split(' ')
    print('final_hyp = ', final_hyp)
    print(words)
    if len(words) == 6:
        A = float(words[2].replace(',',''))
        B = A*64.0
        R = float(words[5].replace(',',''))
        print('Found A = ', A, ', B = ', B, ', R = ', R)
    else:
        A = -1.0
        R = -1.0
    fp.close()

def ProcessLog(logfilename):
    fp = open(logfilename, "r")
    all_points = [ x for x in fp.readlines() if ', blur = ' in x ]
    
    ## lines in all_points look like:
    ## "/home/IMAGES/4-11-2020/image177.fits: ticks = 984, blur = 2.724,"
    x_list = [ float(w.split(' ')[3].replace(',','')) for w in all_points ]
    y_list = [ float(w.split(' ')[6].replace(',','')) for w in all_points ]

    numpy_x = np.array(x_list)
    numpy_y = np.array(y_list)

    plt.plot(numpy_x, numpy_y, 'ro')
    GetHyperbola(logfilename)
    print('Using A = ', A, ', B = ', B, ', R = ', R)
    xmin, xmax = plt.xlim()
    focus_settings = np.arange(xmin, xmax, 1)
    hyp, = plt.plot(focus_settings, hyperbola(focus_settings))
    plt.xlabel('focuser position (ticks)')
    plt.ylabel('star size')
    plt.ylim(0, 9)
    plt.xlim(xmin, xmax)
    plt.title('Focus Data')
    plt.grid(True)
    plt.ion()
    plt.show()

ProcessLog(sys.argv[1])
plt.show(block=True)
    
    
    
