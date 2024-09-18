#!/usr/bin/python3

import sys
import matplotlib.pyplot as plt
import numpy as np
import time
import signal
import threading
from matplotlib.animation import FuncAnimation

def hyperbola(x_values, param_A, param_B, param_R):
    xr_values = param_R - x_values
    y_values = param_A*np.sqrt(1 + np.multiply(xr_values, xr_values)/(param_B*param_B))
    return y_values

curve_avail = False
param_a = 0.0
param_b = 0.0
param_r = 0.0

all_x = []
all_y = []

curve_is_showing = False
num_curves = 0

def thread_function(thread_name):
    global param_a, param_b, param_r
    global all_x, all_y
    global num_curves, curve_avail
    while True:
        try:
            print ("...thread waiting for data\n")
            newline = input()
            print ("...received string ", newline, '\n')

        except EOFError:
            break

        all_words = newline.split()
        keyword = all_words[0]

        print ("keyword = ", keyword)

        if keyword == "point":
            x_value = float(all_words[1])
            y_value = float(all_words[2])

            all_x.append(x_value)
            all_y.append(y_value)

        elif keyword == "curve":
            param_a = float(all_words[1])
            param_b = float(all_words[2])
            param_r = float(all_words[3])

            num_curves += 1
            curve_avail = True

        elif keyword == "quit":
            break

        else:
            print ("Invalid keyword in focus_plot.py: ", keyword)
        
def do_plot_curve():
    global focus_settings
    global curve_is_showing
    global hyp
    global param_a, param_b, param_r
    global curve_avail

    print("curve_avail = ", curve_avail)
    if not curve_avail:
        return
    
    #xmin, xmax = plt.xlim()
    xmin = min(min(all_x), param_r)
    xmax = max(max(all_x), param_r)
    focus_settings = np.arange(xmin, xmax, 1)

    if curve_is_showing:
        hyp.remove()

    print("Adding curve plot.")
    hyp, = plt.plot(focus_settings, hyperbola(focus_settings, param_a, param_b, param_r))
    curve_is_showing = True
    #plt.ion()
    #plt.show()
    #plt.pause(0.001)

plt.xlabel('focuser position (ticks)')
plt.ylabel('star size')
plt.ylim(0, 20)
plt.title('Focus Data')
plt.grid(True)
#plt.show(block=False)

def MakePlot():
    plt.cla() # clear axes
    if len(all_x) > 0:
        plt.xlim(min(all_x)-1, max(all_x)+1)
        plt.ylim(0.0, max(all_y)+1)
    plt.plot(all_x, all_y, 'ro')
    do_plot_curve()
    plt.xlabel('focuser position (ticks)')
    plt.ylabel('star size (pixels)')
    plt.grid(True)
    #plt.ion()
    #plt.show()

prior_points = 0
prior_curves = 0
def animate(i):
    global prior_points, prior_curves
    global all_x, all_y
    global num_curves
    if len(all_x) > prior_points:
        prior_points = len(all_x)
        print("animate(): generating new plot.")
        MakePlot()
    #print("prior_curves = ", prior_curves, ", num_curves = ", num_curves)
    if prior_curves < num_curves:
        prior_curves = num_curves
        #print("animate(): generating new curve.")
        do_plot_curve()

#MakePlot()
ani = FuncAnimation(plt.gcf(), animate, interval=200)
t = threading.Thread(target=thread_function, args=(1,))
t.start()

plt.show()


 
