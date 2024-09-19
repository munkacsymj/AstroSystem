import SessionGlobal
from gi.repository import Gtk
from gi.repository import Pango
import sys
import os
import threading
import datetime
import glob
import matplotlib.pyplot as plt
import matplotlib.dates as mdates
import matplotlib.figure
from matplotlib.backends.backend_gtk3agg import FigureCanvasGTK3Agg as FigureCanvas
from astropy.time import Time
from gi.repository import GObject
import pytz

################################################################
##     class FocusTab
################################################################

class FocusMeasurement:
    def __init__(self, input_line):
        # Mon Nov 28 19:52:46 2022: focus command returned 166310 (with status 0), focus_valid = true
        self.when = ASCII2Datetime(input_line[:24])
        words = input_line.split()
        self.value = int(words[8])

class BlurMeasurement:
    def __init__(self, jd, blur):
        self.when = JD2Datetime(jd)
        if blur == None:
            self.blur = None
        else:
            self.blur = float(blur)

class FocusSetting:
    def __init__(self, input_line):
        # Mon Nov 28 19:46:03 2022: focus: moving focuser to 183621 (183585 actual)
        self.when = ASCII2Datetime(input_line[:24])
        words = input_line.split()
        self.value = int(words[10][1:])

def JD2Datetime(jd):
    fits_value = Time(jd, format='jd').to_value('fits')
    fits_year = int(fits_value[0:4])
    fits_month = int(fits_value[5:7])
    fits_day = int(fits_value[8:10])
    hour = int(fits_value[11:13])
    minutes = int(fits_value[14:16])
    seconds = float(fits_value[17:])
    return datetime.datetime(fits_year,
                             fits_month,
                             fits_day,
                             hour,
                             minutes,
                             int(seconds),
                             tzinfo=datetime.timezone.utc)

def ASCII2Datetime(unix_string):
    # Mon Nov 28 19:52:46 2022
    unix_string = unix_string.strip()
    year = int(unix_string[20:])
    date = int(unix_string[8:10])
    hour = int(unix_string[11:13])
    minute = int(unix_string[14:16])
    second = int(unix_string[17:19])
    month_string = unix_string[4:7]
    month_dict = { 'Jan':1,
                   'Feb':2,
                   'Mar':3,
                   'Apr':4,
                   'May':5,
                   'Jun':6,
                   'Jul':7,
                   'Aug':8,
                   'Sep':9,
                   'Oct':10,
                   'Nov':11,
                   'Dec':12 }
    month = month_dict[month_string]
    return datetime.datetime(year, month, date, hour, minute, second, tzinfo=pytz.timezone('US/Eastern'))

global blur_task_finished
global blur_files_completed
blur_task_finished = False
blur_files_completed = 0

class FocusTab:
    def __init__(self, parent_notebook, tab_label):
        self.whole_box = Gtk.VBox(spacing=2)
        top_box = Gtk.VBox(spacing=2)
        self.shell_buffer = Gtk.TextBuffer()

        self.shell_view = Gtk.TextView()
        self.shell_view.set_buffer(self.shell_buffer)
        self.shell_view.set_editable(False);
        self.shell_view.modify_font(Pango.FontDescription("monospace"))
        self.shell_view.set_cursor_visible(False)
        self.shell_view.set_wrap_mode(Gtk.WrapMode.CHAR)
        self.shell_sw = Gtk.ScrolledWindow()
        self.shell_sw.add(self.shell_view)
        
        self.whole_box.pack_start(top_box, padding=0, fill=True, expand=True)
        self.whole_box.pack_start(self.shell_sw, padding=0, fill=True, expand=True)

        self.plot_figure = plt.figure()
        plt.ion()
        self.ax = self.plot_figure.add_subplot(111)
        top_box.add(FigureCanvas(self.plot_figure))

        parent_notebook.append_page(self.whole_box, Gtk.Label('Focus'))
        self.GetFocusData()
        self.DrawPlot()
        self.whole_box.show()

        GObject.timeout_add(5000, self.timeout)
        self.thread = threading.Thread(target=MeasureBlurWorker, daemon=True)
        self.thread.start()

    def timeout(self):
        global blur_task_finished, blur_files_completed
        print("timeout occurred.", blur_task_finished, blur_files_completed)
        if blur_task_finished:
            # now get blur data (Warning: exposure.blur might be 'None')
            for (target,t_data) in SessionGlobal.star_dictionary.items():
                for (filter,obs_data) in t_data.obs_seq.items():
                    for exposure in obs_data.exposures:
                        self.blur_data.append(BlurMeasurement(exposure.jd,
                                                              exposure.blur))
            self.ax2.plot( [x.when for x in self.blur_data],
                           [x.blur for x in self.blur_data],
                          '.k')
            self.plot_figure.canvas.draw()
            
            return False
        return True

    #def SetShellFile(self, filename):

    def GetFocusData(self):
        focus_filelist = glob.glob(os.path.join(SessionGlobal.homedir,
                                                "session_focus*.txt"))
        self.measurements = []
        self.focus_set_cmds = []
        self.blur_data = []

        for onefile in focus_filelist:
            with open(onefile, 'r') as fp:
                for oneline in fp.readlines():
                    if 'focus command returned' in oneline:
                        self.measurements.append(FocusMeasurement(oneline))
                    if 'focus: moving focuser to' in oneline:
                        self.focus_set_cmds.append(FocusSetting(oneline))

    def DrawPlot(self):
        #(self.main_plot, self.ax) = self.plot_figure.subplots()
        self.ax.set_ylabel("Focuser ticks")
        self.ax.set_xlabel("Session Time")
        
        
        self.ax.plot( [x.when for x in self.measurements],
                      [x.value for x in self.measurements],
                      'ro')
        self.ax.plot( [x.when for x in self.focus_set_cmds],
                      [x.value for x in self.focus_set_cmds],
                      'b-')

        hours = mdates.HourLocator()
        hoursFmt = mdates.DateFormatter('%H:00')
        self.ax.xaxis.set_major_locator(hours)
        self.ax.xaxis.set_major_formatter(hoursFmt)

        self.ax2 = self.ax.twinx()
        self.ax2.set_ylabel('Blur (gaussian)')
        self.ax2.plot( [x.when for x in self.blur_data],
                       [x.blur for x in self.blur_data],
                       ',k')

        #self.plot_figure.show()


def MeasureBlurWorker():
    global blur_task_finished
    global blur_files_completed
    for (target,t_data) in SessionGlobal.star_dictionary.items():
        for (filter,obs_data) in t_data.obs_seq.items():
            for exposure in obs_data.exposures:
                #print("BlurWorker: ", exposure.filename)
                blur_image = '/tmp/blur.fits'
                blur_data_file = '/tmp/blur.txt'
                command = "make_composite "
                command += (" -i " + exposure.filename)
                command += (" -o " + blur_image)
                retval = os.system(command)
                if retval != 0:
                    print("Command failed: ", command)
                command = "/home/mark/ASTRO/CURRENT/TOOLS/FOCUS_MODEL/analyze_composite "
                command += (" -i " + blur_image)
                command += (" > " + blur_data_file)
                retval = os.system(command)
                if retval != 0:
                    print("Command failed: ", command)
                f = open(blur_data_file, "r")
                all_blur_data = f.read()
                f.close()
                if 'gaussian:' in all_blur_data:
                    exposure.blur = float(all_blur_data.split()[1])
                else:
                    exposure.blur = None
                blur_files_completed += 1

    blur_task_finished = True
