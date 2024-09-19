#!/usr/bin/python3
import math

slope = 0.010753

class Measurement:
    def __init__(self, tick, gaussian, when):
        self.tick = tick
        self.when = when
        self.gaussian = gaussian

f = open("/tmp/focus1.csv", "r")
all_measurements = []
for line in f.readlines():
    words = line.split(',')
    m = Measurement(int(words[0]), float(words[1]), int(words[2]))
    all_measurements.append(m)

class Model:
    def __init__(self, A, C, R):
        self.A = A
        self.C = C
        self.R = R

    def ErrSQ(self, measurement):
        x_offset = measurement.tick - (self.C+self.R*measurement.when)
        b = self.A / slope
        prediction = math.sqrt(self.A*self.A * (1+x_offset*x_offset/(b*b)))
        err = measurement.gaussian - prediction
        return err*err

    def Residuals(self, all_measurements):
        sum = 0.0
        for m in all_measurements:
            sum += self.ErrSQ(m)
        return sum

    def CalculateA(self, all_measurements):
        sum_a_sq = 0.0
        for m in all_measurements:
            a_sq = m.gaussian*m.gaussian - slope*slope*(m.tick-(self.C+self.R*m.when))**2
            sum_a_sq += a_sq
        self.A = math.sqrt(sum_a_sq/len(all_measurements))
        print("A = ", self.A)

t =  Model(0.95, 1280, 0.0)
print('ErrSQ = ', t.ErrSQ(all_measurements[0]))
        
#print([(x.tick, x.when, x.gaussian) for x in all_measurements])
points = []
for C in range(1220, 1320, 5):
    R = 0.0
    while R < 0.02:
        m = Model(0.0, C, R)
        m.CalculateA(all_measurements)
        points.append((m.A, m.C, m.R, m.Residuals(all_measurements)))
        #print('Q', m.A, m.C, m.R, m.Residuals(all_measurements))
        R += 0.0004

from mpl_toolkits.mplot3d import Axes3D
import numpy as np
import matplotlib.pyplot as plt
from scipy.interpolate import griddata as gd

x = [c for (a,c,r,e) in points]
y = [r for (a,c,r,e) in points]
z = [min(e,0.05) for (a,c,r,e) in points]
#z = [e for (a,c,r,e) in points]
v = z

xi,yi,zi=np.ogrid[min(x):max(x):100j, min(y):max(y):100j, min(z):max(z):100j]
x1 = xi.reshape(xi.shape[0],)
y1 = yi.reshape(yi.shape[1],)
z1 = zi.reshape(zi.shape[2],)
ar_len=len(x1)*len(y1)*len(z1)

X=np.arange(ar_len,dtype=float)
Y=np.arange(ar_len,dtype=float)
Z=np.arange(ar_len,dtype=float)
l=0
for i in range(0,len(x1)):
    for j in range(0,len(y1)):
        for k in range(0, len(z1)):
            X[l]=x1[i]
            Y[l]=y1[j]
            Z[l]=z1[k]
            l += 1

zi = gd((x,y), z, (x1[None,:], y1[:,None]), method='linear')

fig = plt.figure()
ax = fig.gca(projection='3d')

#sc=ax.scatter(X, Y, Z, c=V, cmap=plt.hot())
#surf = ax.contourf(x1, y1, zi, 15, cmap=plt.cm.jet)
surf = ax.plot_surface(x1, y1, zi, cmap=cm.coolwarm, linewidth=0, antialiased=False)
#plt.zlim(0.0, 0.4)

plt.show()


