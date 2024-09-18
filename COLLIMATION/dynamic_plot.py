#import matplotlib.pyplot as plt
import matplotlib.patches as patches
from matplotlib.figure import Figure
from matplotlib.backends.backend_gtk3cairo import FigureCanvasGTK3Cairo as FigureCanvas
import gi
gi.require_version('Gtk', '3.0')
from gi.repository import Gtk, GLib
import queue
import time

class PointToPlot:
    def __init__(self):
        self.type = None

    def SetPoint(self, x, y, series):
        self.type = "Point"
        self.x = x
        self.y = y
        self.series = series

    def SetCircle(self, x, y, radius, series):
        self.type = "Circle"
        self.x = x
        self.y = y
        self.radius = radius
        self.series = series

    def SetVector(self, x1, y1, x2, y2):
        self.type = "Vector"
        self.x1 = x1
        self.x2 = x2
        self.y1 = y1
        self.y2 = y2
        
class DynamicPlot():
    def __init__(self):
        self.figure = Figure()
        self.ax = self.figure.add_subplot(111)
        self.lines, = self.ax.plot([],[], 'o')
        self.ax.set_aspect('equal', anchor='C')
        self.ax.set_xlim(-1.0, 1.0)
        self.ax.set_ylim(-1.0, 1.0)
        self.ax.grid()
        self.xdata = []
        self.ydata = []
        self.lines.set_xdata(self.xdata)
        self.lines.set_ydata(self.ydata)
        self.queue = queue.Queue()

        self.canvas = FigureCanvas(self.figure)
        self.canvas.set_size_request(400, 400)
        
        print('DynamicPlot().__init__() completed.')

    def Canvas(self):
        return self.canvas

    def SafeAdd(self, point):
        self.queue.put(point)
        print('Point pushed onto queue.')
        GLib.idle_add(self.local_add_point)

    def local_add_point(self):
        print('dynamic_plot.local_add_point() invoked.')
        quit = False
        while not quit:
            try:
                new_item = self.queue.get(block=False)
                print('Point pulled off queue.')
                self.local_draw(new_item)
            except queue.Empty:
                quit = True
        
    def local_draw(self, point):
        if point.type == None:
            print('DynamicPlot: point with no type!')
        elif point.type == 'Point':
            self.xdata.append(point.x)
            self.ydata.append(point.y)
        elif point.type == 'Circle':
            circle = patches.Circle((point.x, point.y), radius=point.radius, fill=False)
            self.ax.add_patch(circle)
        elif point.type == 'Vector':
            vector = patches.Arrow(point.x1, point.y1, (point.x2-point.x1), (point.y2-point.y1))
            self.ax.add_patch(vector)
        else:
            print('DynamicPlot: invalid point type: ', point.type)
        self.ax.relim()
        self.ax.autoscale_view()
        print('calling flush_events()')
        self.lines.set_xdata(self.xdata)
        self.lines.set_ydata(self.ydata)
        self.figure.canvas.draw()
        self.figure.canvas.flush_events()
        print('len(xdata) = ', len(self.xdata))

