#!/usr/bin/python3

import os
import sys
import math
import getopt

user_x = None
user_y = None

def Direction(amount):
    if amount < 0.0:
        return "CCW"
    else:
        return "CW"

def Solve(x1, y1, x2, y2, x_err, y_err):
    k2 = (y_err * x1 - x_err * y1)/(x1*y2 - x2*y1)
    if x1 != 0.0:
        k1 = (x_err - k2*x2)/x1
    else:
        k1 = (y_err - k2*y2)/y1
    return (k1, k2)

options, remainder = getopt.getopt(sys.argv[1:], 'x:y:')
for opt, arg in options:
    if opt in '-x':
        user_x = float(arg)
    elif opt in '-y':
        user_y = float(arg)

if user_x == None or user_y == None:
    print('Usage: screws -x x_error -y y_error')
    exit()

Xa = 11.0*math.sin(130*math.pi/180.0)
Ya = 11.0*math.cos(130*math.pi/180.0)
Xb = 11.0*math.sin(250*math.pi/180.0)
Yb = 11.0*math.cos(250*math.pi/180.0)
Xc = 11.0*math.sin(10*math.pi/180.0)
Yc = 11.0*math.cos(10*math.pi/180.0)

Ma, Mb = Solve(Xa, Ya, Xb, Yb, -user_x, -user_y);
print(f'A = {abs(Ma):.3f} {Direction(Ma)}, B = {abs(Mb):.3f} {Direction(Mb)}')

Mb, Mc = Solve(Xb, Yb, Xc, Yc, -user_x, -user_y);
print(f'B = {abs(Mb):.3f} {Direction(Mb)}, C = {abs(Mc):.3f} {Direction(Mc)}')

Ma, Mc = Solve(Xa, Ya, Xc, Yc, -user_x, -user_y);
print(f'A = {abs(Ma):.3f} {Direction(Ma)}, C = {abs(Mc):.3f} {Direction(Mc)}')


