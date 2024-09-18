#!/usr/bin/python3

import os
import sys
import math
import threading
import time
import gi
gi.require_version('Gtk', '3.0')
from gi.repository import Gtk, GLib
from matplotlib import pyplot as plt
import statistics
import dynamic_plot
import coordinates

#plt.ion()                       # turn on interactive mode

# The three adjusting screws are 120-deg apart. This is where the 10,
# 130, 250 degree reference comes from. The "4.3" is a magic number
# that seems to describe how much the doughnuts get offset by one turn
# of any one of those adjusting screws.
Xa = 4.3*math.sin(130*math.pi/180.0)
Ya = 4.3*math.cos(130*math.pi/180.0)
Xb = 4.3*math.sin(250*math.pi/180.0)
Yb = 4.3*math.cos(250*math.pi/180.0)
Xc = 4.3*math.sin(10*math.pi/180.0)
Yc = 4.3*math.cos(10*math.pi/180.0)

# Take multiple measurements and return a "bounding circle" that
# captures the RMS consensus of the measurement set.
def RMSBoundingCircle(xy_points):
    p = list(zip(*xy_points))
    avg_x = statistics.mean(p[0])
    avg_y = statistics.mean(p[1])
    offsets = [(x-avg_x, y-avg_y) for (x,y) in xy_points]
    distances_sq = [(x*x + y*y) for (x,y) in offsets]
    rms = math.sqrt(statistics.mean(distances_sq))
    return (avg_x, avg_y, rms)
    
def Direction(amount):
    if amount < 0.0:
        return "CCW"
    else:
        return "CW"

# Multithreaded. This thread does the actual measuring. The other
# thread handles the user interface
class Work:
    def __init__(self, console):
        self.console = console
        self.series = 0
        print('Starting work thread.')
        self.plot = dynamic_plot.DynamicPlot()

        self.thread = threading.Thread(target=self.DoWork)
        self.thread.start()

    # Wait for the resume button to be pressed
    def WaitForResume(self):
        self.console.buffer.Log('Press Resume to continue.\n')
        GLib.idle_add(self.console.resume.set_sensitive, True)
        #self.console.resume.set_sensitive(True)
        self.console.resume_event.wait()
        self.console.resume_event.clear()
        GLib.idle_add(self.console.resume.set_sensitive, False)
        #self.console.resume.set_sensitive(False)
        return False # means don't quit yet

    def DoWork(self):
        print('DoWork is starting...')
        self.console.buffer.Log('Collimate has started.\n')
        self.nom_exposure_time = 1.0

        self.DoCollimate()

        self.WaitForResume()
        self.console.buffer.Log('Collimate is done.\n')

    def DoCollimate(self):
        print('DoCollimate() invoked.')
        quit = False
        # Go through one cycle of collimation adjustment...
        while (not quit):
            # center the star
            self.CenterBlob()
            # adjust focus so the star is the appropriate (bloated) size
            self.Defocus()
            # get the camera exposure time right
            self.AdjustExposure()
            # Make exposures and measure the coma aberration
            err = self.MeasureComa()
            # Translate the coma measurement into adjustment screw changes
            self.PromptScrewAdjust(err)
            # Wait for the user to adjust the screws and then press "resume"
            quit = self.WaitForResume()
            if not quit:
                # Adjust pointing of the telescope, since the
                # telescope needs to be flopped on its side in order
                # to get to the secondary mirror adjusting
                # screws. Once the screws have been adjusted, need to
                # point the telescope back to the target star.
                self.SlewBackToPriorLocation()

    # Adjust telescope pointing
    def SlewBackToPriorLocation(self):
        dec_deg = self.target_loc.dec_rad*180/math.pi
        ra_hours = self.target_loc.ra_rad*12.0/math.pi
        command = 'goto ' + coordinates.float2sexagesimal(dec_deg) + ' ' + coordinates.float2sexagesimal(ra_hours)
        print('executing ', command)
        os.system(command)

    # Make an exposure
    def DoExposure(self, upper=None, lower=None, left=None, right=None):
        # quit if requested to do so...
        if self.console.quit_event.isSet():
            GLib.idle_add(Gtk.main_quit)
            self.thread.exit()
        if upper == None or lower == None or left == None or right == None:
            os.system('expose -t ' + str(self.nom_exposure_time) + ' > /tmp/exp_filename.txt')
        else:
            command = ('expose -t ' + str(self.nom_exposure_time) +
                       ' -u ' + str(upper) +
                       ' -b ' + str(lower) +
                       ' -l ' + str(left) +
                       ' -r ' + str(right) +
                       ' > /tmp/exp_filename.txt')
            print('Executing ', command)
            os.system(command)
        with open('/tmp/exp_filename.txt', 'r') as fp:
            filename = fp.readline().strip()

        if self.console.quit_event.isSet():
            GLib.idle_add(Gtk.main_quit)
            sys.exit()
        return filename

    # Do a sub-frame exposure with a defined bounding box
    # (Don't need an entire image if all we want is a single star.)
    def DoBoxExposure(self):
        return self.DoExposure(upper = self.box_upper,
                               lower = self.box_lower,
                               left = self.box_left,
                               right = self.box_right)

    # Scope already pointing at a focus_star
    def CenterBlob(self):
        print('CenterBlob() invoked.')
        self.console.buffer.Log('Centering star.\n')
        quit = False
        while (not quit):
            image_filename = self.DoExposure()
            os.system('find_blob -i ' + image_filename + '> /tmp/blob.txt')
            with open('/tmp/blob.txt', 'r') as fp:
                result = fp.readline()
                if not 'RESULT' in result:
                    print('find_blob command failed.')
                    return False
                if 'INVALID' in result:
                    print('find_blob failed. Retrying...')
                    self.console.buffer.Log('Find_blob failed. Retrying...\n')
                    return self.CenterBlob()
            words = result.split()
            x = float(words[1])
            y = float(words[2])
            # (X,Y) is in pixel coordinates. Need to convert to
            # astronomical coordinates in order to move the telescope
            del_x = 256-x
            del_y = 256-y
            pixels_per_arcmin = 60/1.52
            x_arcmin = del_x/pixels_per_arcmin
            y_arcmin = del_y/pixels_per_arcmin

            # within one arcminute of center is good enough
            if (x_arcmin < 1.0 and y_arcmin < 1.0):
                quit = True
            else:
                # (not sure this works if flipped)
                command = 'move ' + ('%.3f' % x_arcmin) + 'E ' + ('%.3f' % y_arcmin) + 'N'
                print('Issuing: ' + command)
                os.system(command)

        # Define the bounding box for subsequent (sub)images.
        y = 512 - y
        self.box_left = int(x-30)
        self.box_right = int(x+30)
        self.box_upper = int(y+30)
        self.box_lower = int(y-30)
        print('Centering complete.\n')
        self.console.buffer.Log('Centering complete.\n')
        self.target_loc = self.get_dec_ra(image_filename)

    # Find out where the telescope is currently pointed. Easiest way
    # is to pull it from the data stored with an image.
    def get_dec_ra(self, image_filename):
        command = 'fits_keywords ' + image_filename + ' > /tmp/keywords.txt'
        print('Issuing ', command)
        os.system(command)
        with open('/tmp/keywords.txt', 'r') as fp:
            skip = len(image_filename)
            all_lines = fp.readlines()
            for line in all_lines:
                line = line[skip+1:]
                if 'DEC_NOM' in line or 'RA_NOM' in line:
                    words = line.split('=')
                    subwords = words[1].strip().split(' ')
                    print('subwords = ', subwords)
                    value = subwords[0].replace("'","").strip()
                    if 'DEC_NOM' in words[0]:
                        dec_string = value
                    if 'RA_NOM' in words[0]:
                        ra_string = value
        print('Extracted Dec/RA = ', dec_string, ra_string)
        return coordinates.DecRA(coordinates.sexagesimal2float(dec_string)*math.pi/180.0,
                                 coordinates.sexagesimal2float(ra_string)*math.pi/12.0)
                    
    # Someday I really need to write this.
    def Defocus(self):
        self.console.buffer.Log('Skipping focus adjustment.\n')

    # Adjust exposure time so that peak pixel value is somewhere
    # between 10,000 and 50,000
    def AdjustExposure(self):
        print('Starting AdjustExposure()')
        filename = self.DoBoxExposure()
        print('Box exposure completed: ', filename)
        os.system('image_statistics -o /tmp/stats.txt -i ' + filename)
        stat_values = {}
        with open('/tmp/stats.txt', 'r') as fp:
            for line in fp:
                words = line.split('=')
                stat_values[words[0].strip()] = float(words[1].strip())
        if not 'MAX' in stat_values:
            print('collimate: image_statistics missing MAX')
        else:
            peak = stat_values['MAX']
            if peak < 10000 or peak > 50000:
                # Adjust
                self.nom_exposure_time *= (30000/peak)
                if self.nom_exposure_time > 10.0:
                    print('Star too faint.')
                    Gtk.main_quit()
                self.AdjustExposure()
        self.console.buffer.Log('Exposure time set. Using ' +
                                ('%.1f' % self.nom_exposure_time) + ' seconds.\n')

    # Use "find_match" external program to measure stellar doughnut
    def MeasureComa(self):
        num_measurements = 0
        sum_coll_err_x = 0.0
        sum_coll_err_y = 0.0
        sum_blur = 0.0
        points = []
        self.series += 1
        # We calculate the average of 4 measurements
        while num_measurements < 4:
            filename = self.DoBoxExposure()
            os.system('/home/mark/ASTRO/CURRENT/TOOLS/COLLIMATION/find_match -i ' +
                      filename +
                      ' > /tmp/blur.txt 2>&1')
            answer_line = None
            with open('/tmp/blur.txt', 'r') as fp:
                for line in fp:
                    if 'AnswerBlur' in line:
                        answer_line = line
                        break
            if answer_line == None:
                print('Collimate: find_match missing AnswerBlur')
            else:
                # Three values come back: amount of defocus ("blur"),
                # and an (X,Y) doughnut offset amount (in pixels).
                words = answer_line.split()
                blur = float(words[1].replace(',',''))
                if 'NaN' in answer_line or blur <= 0.0:
                    self.console.buffer.Log('find_match did not converge.\n')
                else:
                    coll_err_x = float(words[4].replace(',',''))
                    coll_err_y = float(words[7].replace(',',''))
                    self.console.buffer.Log('Measured collimation err = (' +
                                            str(coll_err_x) + ', ' +
                                            str(coll_err_y) + ')\n')
                    sum_coll_err_x += coll_err_x
                    sum_coll_err_y += coll_err_y
                    sum_blur += blur
                    (x,y) = (coll_err_x/blur, coll_err_y/blur)
                    p = dynamic_plot.PointToPlot()
                    p.SetPoint(x, y, 1)
                    points.append((x,y))
                    self.plot.SafeAdd(p)
                    num_measurements += 1
        # Now do the averaging...
        avg_blur = sum_blur/num_measurements
        avg_err_x = (sum_coll_err_x/num_measurements)/avg_blur
        avg_err_y = (sum_coll_err_y/num_measurements)/avg_blur
        self.console.buffer.Log('Average collimation err = (' +
                                ('%.3f' % avg_err_x) + ', ' +
                                ('%.3f' % avg_err_y) + ')\n')
        p = dynamic_plot.PointToPlot()
        (c_x, c_y, rmsradius) = RMSBoundingCircle(points)
        p.SetCircle(c_x, c_y, rmsradius, 1)
        self.plot.SafeAdd(p)
        return (avg_err_x, avg_err_y)

    # Express an (X,Y) amount as a blend of adjustments to two of the
    # three adjusting screws
    def Solve(self, x1, y1, x2, y2, x_err, y_err):
        k2 = (y_err * x1 - x_err * y1)/(x1*y2 - x2*y1)
        if x1 != 0.0:
            k1 = (x_err - k2*x2)/x1
        else:
            k1 = (y_err - k2*y2)/y1
        return (k1, k2)

    def PromptScrewAdjust(self, err):
        (user_x, user_y) = err
        
        Ma, Mb = self.Solve(Xa, Ya, Xb, Yb, -user_x, -user_y);
        self.console.buffer.Log(f'A = {abs(Ma):.3f} {Direction(Ma)}, B = {abs(Mb):.3f} {Direction(Mb)}\n')

        Mb, Mc = self.Solve(Xb, Yb, Xc, Yc, -user_x, -user_y);
        self.console.buffer.Log(f'B = {abs(Mb):.3f} {Direction(Mb)}, C = {abs(Mc):.3f} {Direction(Mc)}\n')

        Ma, Mc = self.Solve(Xa, Ya, Xc, Yc, -user_x, -user_y);
        self.console.buffer.Log(f'A = {abs(Ma):.3f} {Direction(Ma)}, C = {abs(Mc):.3f} {Direction(Mc)}\n')
        

class ConsoleBuffer(Gtk.TextBuffer):
    def __init__(self):
        Gtk.TextBuffer.__init__(self)
        self.current_text = ''

    def Log(self, string):
        print('Log: new log string = ', string)
        self.current_text += string
        GLib.idle_add(self.set_text, self.current_text)
        #self.set_text(self.current_text)

class ConsoleWindow(Gtk.Window):
    def __init__(self):
        super(ConsoleWindow, self).__init__()
        self.set_title('Collimate')
        #self.set_default_size(800,800)

        self.whole_box = Gtk.HBox(spacing = 2)
        self.left_box = Gtk.VBox(spacing=2)

        self.scroll = Gtk.ScrolledWindow()
        self.scroll.set_min_content_width(400)
        self.scroll.set_min_content_height(500)
        self.buffer = ConsoleBuffer()
        self.view = Gtk.TextView()

        self.view.set_buffer(self.buffer)
        self.view.set_editable(False)
        self.view.set_cursor_visible(False)
        self.scroll.add(self.view)

        self.resume = Gtk.Button('Resume')
        self.resume.set_sensitive(False)
        self.resume.connect('clicked', self.DoResume)
        self.resume_event = threading.Event()
        quit_b = Gtk.Button('Abort')
        quit_b.connect('clicked', self.DoAbort)
        self.quit_event = threading.Event()
        self.left_box.pack_start(quit_b, padding=0, fill=False, expand=False)
        self.left_box.pack_start(self.resume, padding=0, fill=False, expand=False)

        self.left_box.pack_start(self.scroll, padding=0, fill=True, expand=True)
        self.whole_box.pack_start(self.left_box, padding=0, fill=False, expand=False)
        self.rightside = Gtk.Frame()

        self.whole_box.pack_start(self.rightside, padding=0, fill=False, expand=False)

        self.add(self.whole_box)

    def DoAbort(self, arg):
        self.quit_event.set()

    def DoResume(self, arg):
        self.resume_event.set()

win = ConsoleWindow()
win.connect("destroy", Gtk.main_quit)
work = Work(win)
win.rightside.add(work.plot.Canvas())
win.show_all()
print('Entering Gtk.main()')
Gtk.main()


