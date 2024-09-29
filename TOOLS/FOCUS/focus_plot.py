#!/usr/bin/python2.7

import sys
import matplotlib.pyplot as plt
import numpy as np
import time

def hyperbola(x_values, param_A, param_B, param_R):
    xr_values = param_R - x_values
    y_values = param_A*np.sqrt(1 + np.multiply(xr_values, xr_values)/(param_B*param_B))
    return y_values

param_a = 0.0
param_b = 0.0
param_r = 0.0

all_x = []
all_y = []

def do_plot_curve():
    global focus_settings
    global curve_is_showing
    global hyp
    global param_a, param_b, param_r
    
    #xmin, xmax = plt.xlim()
    xmin = min(min(all_x), param_r)
    xmax = max(max(all_x), param_r)
    focus_settings = np.arange(xmin, xmax, 1)

    if curve_is_showing:
        hyp.remove()

    hyp, = plt.plot(focus_settings, hyperbola(focus_settings, param_a, param_b, param_r))
    curve_is_showing = True
    plt.ion()
    plt.show()
    plt.pause(0.001)

plt.xlabel('focuser position (ticks)')
plt.ylabel('star size')
plt.ylim(0, 20)
plt.title('Focus Data')
plt.grid(True)
plt.show(block=False)

curve_is_showing = False

#for line in sys.stdin:
while True:
    try:
        line = raw_input()
        print "...received string ", line

    except EOFError:
        time.sleep(4)
        break
    
    all_words = line.split()
    keyword = all_words[0]

    print "keyword = ", keyword

    if keyword == "point":
        x_value = float(all_words[1])
        y_value = float(all_words[2])

        all_x.append(x_value)
        all_y.append(y_value)
        plt.xlim(min(all_x)-1, max(all_x)+1)
        plt.ylim(0.0, max(all_y)+1)

        plt.plot(x_value, y_value, "ro")
        do_plot_curve()
        plt.ion()
        plt.show()
        plt.pause(0.001)

    elif keyword == "curve":
        param_a = float(all_words[1])
        param_b = float(all_words[2])
        param_r = float(all_words[3])

        do_plot_curve()

    elif keyword == "quit":
        break

    else:
        print "Invalid keyword in focus_plot.py: ", keyword

plt.show(block=True)
 
