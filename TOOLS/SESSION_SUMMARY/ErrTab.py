import SessionGlobal
import shlex
import os
import gi
from gi.repository import Gtk
import sys
import math
import matplotlib.pyplot as plt
import matplotlib.figure
from matplotlib.backends.backend_gtk3agg import FigureCanvasGTK3Agg as FigureCanvas


################################################################
##    class ErrTab
################################################################

class ErrTab:
    def __init__(self, parent_notebook, tab_label):
        self.whole_box = Gtk.VBox(spacing = 2)
        color_boxes = Gtk.HBox(spacing = 1)
        color_boxes.set_border_width(5)
        self.whole_box.pack_start(color_boxes, fill=False, expand=False, padding=0)
        self.color_box = {}
        self.zero_point = {}
        self.do_plot_normalized = False

        this_color_box = ColorErrorBox('B')
        self.color_box['B'] = this_color_box
        color_boxes.pack_start(this_color_box.window(), fill=False, expand=False, padding=0)
        color_boxes.pack_start(Gtk.VSeparator(), fill=False, expand=True, padding=5)
        this_color_box = ColorErrorBox('V')
        self.color_box['V'] = this_color_box
        color_boxes.pack_start(this_color_box.window(), fill=False, expand=False, padding=0)
        color_boxes.pack_start(Gtk.VSeparator(), fill=False, expand=True, padding=5)
        this_color_box = ColorErrorBox('R')
        self.color_box['R'] = this_color_box
        color_boxes.pack_start(this_color_box.window(), fill=False, expand=False, padding=0)
        color_boxes.pack_start(Gtk.VSeparator(), fill=False, expand=True, padding=5)
        this_color_box = ColorErrorBox('I')
        self.color_box['I'] = this_color_box
        color_boxes.pack_start(this_color_box.window(), fill=False, expand=False, padding=5)

        self.whole_box.pack_start(Gtk.HSeparator(), fill=False, expand=False, padding=3)

        self.plot_normalized_button = Gtk.CheckButton(label="Plot Normalized")
        self.plot_normalized_button.connect("toggled", self.plot_normalized_callback, None)
        self.whole_box.pack_start(self.plot_normalized_button, fill=False, expand=False, padding=0)

        #self.star_label = Gtk.Label('X')
        #self.whole_box.pack_start(self.star_label, fill=False, expand=False)

        self.plot_figure = plt.figure()
        
        # This should NOT be commented out. The line needs to be
        # there, but I'm having trouble with the "include" of the
        # module up at the top.
        self.whole_box.add(FigureCanvas(self.plot_figure))
        self.main_plot = None
        
        parent_notebook.append_page(self.whole_box, Gtk.Label(tab_label))
        self.whole_box.show()

    def plot_normalized_callback(self, button, value):
        self.do_plot_normalized = self.plot_normalized_button.get_active();
        self.refresh_err_plot()

    def set_star(self):
        self.refresh_err_file()
        self.refresh_err_plot()

    def refresh_err_plot(self):
        # get rid of any existing plot
        if self.main_plot != None:
            self.main_plot.cla()
        else:
            self.main_plot = self.plot_figure.add_subplot(1,1,1)
            self.main_plot.set_ylabel("Check Star Error")
            self.main_plot.set_xlabel("1/SNR")

        self.main_plot.plot([0.0, 0.15], [0.0, 0.15], color="gray", marker=None, linestyle='solid')
        self.main_plot.plot([0.0, 0.15], [0.0, -0.15], color="gray", marker=None, linestyle='solid')
        print("Plotting ", len(self.raw_error), " points.")
        for point in self.raw_error:
            fmt_code = "black"
            if point[2] == 'B':
                fmt_code = "blue"
            if point[2] == 'V':
                fmt_code = "green"
            if point[2] == 'R':
                fmt_code = "red"
            if point[2] == 'I':
                fmt_code = "orange"
            err_value = point[0]
            if self.do_plot_normalized:
                err_value -= self.zero_point[point[2]]
            self.main_plot.plot(point[1], err_value, color=fmt_code, marker='o')
        self.plot_figure.canvas.draw()

    # refresh_err_file() reads the error data from the input file and
    # updates the text block at the top of the tab. It does not
    # perform the plotting, and never needs to be re-run as a result
    # of changing plot normalization.
    def refresh_err_file(self):
        self.err_filename = os.path.join(SessionGlobal.homedir,
                                         (SessionGlobal.current_star.name + ".err"))
        exists = os.path.isfile(self.err_filename)
        self.raw_error = []
        if exists:
            err_file = open(self.err_filename, 'r')
            sum_err = 0.0
            for line in err_file:
                words = shlex.split(line)
                #words = line.split(' ')
                # words[0] = error
                # words[1] = 1/SNR (statistical error)
                # words[2] = one-letter filter name
                # words[3] = star.common_name
                self.raw_error.append([ float(words[0]), float(words[1]), words[2], words[3] ])
                sum_err += float(words[1])
        if len(self.raw_error) > 0:
            self.err_average = sum_err/len(self.raw_error)
            print("Average error = ", self.err_average)
            for c in ["B", "V", "R", "I"]:
                self.color_box[c].SetError(self.raw_error)
                self.zero_point[c] = self.color_box[c].zero()
        else:
            self.err_average = 0.0
            print("Average error = 0.0 (no points)")
            for c in ["B", "V", "R", "I"]:
                self.color_box[c].SetError(None)
                self.zero_point[c] = self.color_box[c].zero()

################################################################
##    class ColorErrorBox
################################################################
class ColorErrorBox:
    def __init__(self, color_letter):
        self.my_box = Gtk.VBox(spacing = 1)
        self.my_color = color_letter
        # Top Label
        label = Gtk.Label(color_letter)
        self.my_box.pack_start(label, fill=True, expand=False, padding=0)
        # Raw Error
        self.raw_error_l = Gtk.Label("Raw Error =")
        self.my_box.pack_start(self.raw_error_l, fill=False, expand=False, padding=0)
        # Zero Offset
        self.error_offset_l = Gtk.Label("Zero offset =")
        self.my_box.pack_start(self.error_offset_l, fill=False, expand=False, padding=0)
        # Normalized Error
        self.norm_error_l = Gtk.Label("Re-normalized error =")
        self.my_box.pack_start(self.norm_error_l, fill=False, expand=False, padding=0)
        self.my_box.show()
        self.error_offset = 0.0

    def zero(self):
        return self.error_offset

    def window(self):
        return self.my_box

    def SetError(self, data):
        if data == None:
            self.raw_error_l.set_text("Raw Error = ")
            self.error_offset_l.set_text("Zero offset = ")
            self.norm_error_l.set_text("Re-normalized error = ")
            self.error_offset = 0.0
        else:
            culled_data = []
            raw_error = 0.0
            raw_average=0.0
            renorm_error = 0.0
            for point in data:
                if point[2] == self.my_color:
                    culled_data.append(point)
                    this_error = point[0]
                    raw_error += (this_error * this_error)
                    print("Raw point " + self.my_color + str(this_error))
            if len(culled_data) > 0:
                raw_error = math.sqrt(raw_error/len(culled_data))
                
                for point in culled_data:
                    raw_average += point[0]
                raw_average = (raw_average/len(culled_data))
                sum_norm_error = 0.0
                for point in culled_data:
                    this_error = point[0] - raw_average
                    sum_norm_error += (this_error * this_error)
                renorm_error = math.sqrt(sum_norm_error/len(culled_data))

            self.error_offset = raw_average
            self.raw_error_l.set_text("Raw Error = " + ("%7.4f" % raw_error))
            self.error_offset_l.set_text("Zero offset = " + ("%7.4f" % raw_average))
            self.norm_error_l.set_text("Re-normalized error = " + ("%7.4f" % renorm_error))
        self.my_box.show()

