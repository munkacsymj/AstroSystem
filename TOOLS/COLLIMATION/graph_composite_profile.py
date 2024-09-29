#!/usr/bin/env python

import sys
import matplotlib.pyplot as plt
import numpy as np
import time

plt.xlabel('pixels')
plt.ylabel('intensity')
#plt.ylim(0, 9)
plt.title('Composite Star Profile')
plt.grid(True)
plt.show(block=False)

#for line in sys.stdin:
while True:
    try:
        line = raw_input()
        #print "...received string ", line

    except EOFError:
        time.sleep(4)
        break
    
    all_words = line.split()
    keyword = all_words[0]

    #print "keyword = ", keyword

    if keyword == "point":
        x_value = float(all_words[1])
        y_value = float(all_words[2])

        plt.plot(x_value, y_value, "ro")
        #plt.show(block=False)
        #plt.draw()

    elif keyword == "title":
        plt.title(line.replace("title", "", 1))

    elif keyword == "show":
        plt.show(block=False)
        plt.draw()

    elif keyword == "quit":
        break

    else:
        print "Invalid keyword in focus_plot.py: ", keyword

plt.show(block=True)
 
