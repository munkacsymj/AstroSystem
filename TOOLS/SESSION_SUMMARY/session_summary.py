#!/usr/bin/python3 -u 

################################################################
##   DEBUGGING: GTK_DEBUG=interactive ./session_summary_old_v2.py
################################################################

import pdb
import sys, getopt
import shlex
import re
import numpy as np
import os.path
import gi
gi.require_version('Gtk', '3.0')
from gi.repository import Gtk as gtk
import tkinter
from tkinter import *
import pprint
from astropy.io import fits
from astropy.time import Time

## Local Modules
import Chart
import Thumbnail
import SessionTreeView
import AnalysisTab
import BVRITab
import compTab
#import ReportTab
import FITSViewer
import FITSTab
import StackPane
import StackTab
import TextTab
import ErrTab
import GlobalTab
import FocusTab
import SessionStarView
import comp_analy
import aavso_report
import history
#import AAVSOReport

import SessionGlobal

sys.path.insert(1, '/home/mark/ASTRO/CURRENT')
from PYTHON_LIB.ASTRO_DB_LIB import astro_db

#
#  TargetStar, ObsSeq, Exposure
#
class TargetStar:
    def __init__(self, starname):
        self.name = starname
        self.obs_seq = {}
        self.designation = ''

class ObsSeq:
    def __init__(self, parent_star):
        self.parent = parent_star
        self.filter = 'unknown'
        self.exposures = []
        self.stackfilename = 'unknown'
        self.stacked_image = None
        self.exposure_time = None

class Exposure:
    def __init__(self, parent_seq, pathname, focus_ticks, blur, timestamp):
        self.parent = parent_seq
        self.filename = pathname
        self.focus_ticks = focus_ticks
        self.jd = timestamp
        self.blur = None
        self.thumbnail_box = None
        self.juid = SessionGlobal.db_obj.GetJUIDForImage(self.filename)

def Fetch_Image_Metadata(image_filename, target_obj):
    #try:
        hdulist = fits.open(image_filename, ignore_missing_end=True)
        hdu_num = FITSViewer.ImageHDUNumber(hdulist)
        filtername = hdulist[hdu_num].header['FILTER']
        if filtername not in target_obj.obs_seq:
            target_obj.obs_seq[filtername] = ObsSeq(target_obj)
            target_obj.obs_seq[filtername].filter = filtername

            exposure_time = hdulist[hdu_num].header['EXPOSURE']
            target_obj.obs_seq[filtername].exposure_time = str(exposure_time)

        focus_ticks = hdulist[hdu_num].header['FOCUS']
        timestamp = hdulist[hdu_num].header['DATE-OBS']
        blur = None
        
        this_image = Exposure(target_obj.obs_seq[filtername],
                              image_filename,
                              focus_ticks,
                              blur,
                              Time(timestamp, format='fits', scale='utc').jd)
        target_obj.obs_seq[filtername].exposures.append(this_image)
        hdulist.close()
    #except:
        #print("Fetch_Image_Metadata failed fetching from ", image_filename)

def Initialize_Summary_from_csv(daily_dir, csv_summary_file):
    csv_file = open(csv_summary_file, 'r')
    for line in csv_file:
        words = line.split(',')
        ThisStar = TargetStar(words[0]) # e.g., "w-aur"
        SessionGlobal.all_targets.append(ThisStar)
        SessionGlobal.star_dictionary[ThisStar.name] = ThisStar

        ThisStar.designation = words[1] # e.g., "0546+15C"
        image_numbers = words[2].split(' - ') # "170 - 181"

        for n in range(int(image_numbers[0]), int(image_numbers[1])+1):
            image_filename = os.path.join(SessionGlobal.homedir,
                                          'image' + "{0:03d}".format(n) + '.fits')
            #print( ThisStar.name, ": fetching ", image_filename)
            Fetch_Image_Metadata(image_filename, ThisStar)

        # We always create a placeholder for a stacked image (including
        # inserting an Image() for it), even if it doesn't exist yet.
        for filter in ThisStar.obs_seq:
            stackfile = ThisStar.obs_seq[filter].filter + ".fits"
            ThisStar.obs_seq[filter].stackfilename = stackfile
            stackfilepath = os.path.join(SessionGlobal.homedir, stackfile)
            ThisStar.obs_seq[filter].stacked_image = \
                    Exposure(ThisStar.obs_seq[filter], stackfilepath)
    csv_file.close()
    
    
def Initialize_Summary_from_FITS(daily_dir):
    global aavso_base, csv_summary_file
    command = "fits_keywords " + os.path.join(SessionGlobal.homedir, "image*.fits")
    command += " | egrep 'PURPOSE|OBJECT' "
    command += " > /tmp/imagelist.txt"
    os.system(command)
    last_image = ""
    current_purpose = None
    current_object = None
    imagelist_file = open("/tmp/imagelist.txt", "r")
    for line in imagelist_file:
        words = line.split()
        this_image = words[0]
        this_keyword = words[1]
        this_value = words[3]

        # Now trim the three words we extracted
        image_filename = this_image.replace(":", "")
        this_value = this_value.replace("'", "")
        this_value = this_value.replace(" ", "")

        if image_filename != last_image:
            current_purpose = None
            current_object = None
            last_image = image_filename
        if this_keyword == "PURPOSE":
            current_purpose = this_value
        if this_keyword == "OBJECT":
            current_object = this_value

        if current_object != None and '(Quick)' in current_object:
            current_object = current_object[:-7]

        #print("purpose=", current_purpose, ", object=", current_object, ", ", image_filename)
        if current_purpose == "PHOTOMETRY" and current_object != None:
            # Remember this image!
            if current_object not in SessionGlobal.star_dictionary:
                #print "Found target object ", current_object
                ThisStar = TargetStar(current_object)
                SessionGlobal.all_targets.append(ThisStar)
                SessionGlobal.star_dictionary[ThisStar.name] = ThisStar

                ThisStar.designation = None # used to be able to fetch this
            else:
                ThisStar = SessionGlobal.star_dictionary[current_object]
            Fetch_Image_Metadata(image_filename, ThisStar)
    imagelist_file.close()

def main():
    gtk.main()

class TextTabs:
    global aavso_base, csv_summary_file
    global log_summary_file, shell_summary_file

    def __init__(self):
        self.book = gtk.Notebook()
        self.book.set_tab_pos(gtk.PositionType.TOP)

        #self.summary_tab = TextTabFromFile(self.book, "Summary", global_summary.filename, goto_button=False)
        #global_summary.set_tab(self.summary_tab)
        #global_summary.refresh()
        SessionGlobal.global_summary = GlobalTab.GlobalTab(self.book, "Date Summary")
        self.global_tab = SessionGlobal.global_summary

        self.log_tab = TextTab.TextTabFromFile(self.book, "Log", log_summary_file, goto_button=True)
        self.shell_tab = TextTab.TextTabFromFile(self.book, "Shell", shell_summary_file, goto_button=True)
        self.strategy_tab = TextTab.TextTabFromFile(self.book, "Strategy", None)
        self.catalog_tab = TextTab.TextTabFromFile(self.book, "Catalog", None)
        #self.report_tab = ReportTab.AAVSOReport(self.book, "AAVSO Report") 
        #self.analy_tab = AnalysisTab.AnalysisTab(self.book, "Analysis")
        self.bvri_tab = BVRITab.BVRITab(self.book, "BVRI")
        self.comp_tab = compTab.CompTAB(self.book, "Fitting")
        self.err_tab = ErrTab.ErrTab(self.book, "Errors")
        self.focus_tab = FocusTab.FocusTab(self.book, "Focus")
        self.bvri_tab.set_error_tab(self.err_tab)
        #self.report_tab = AAVSOReport.AAVSOReport(self.book, "Report")
        self.report_tab = aavso_report.ReportTab(self.book)

        comp_analy.overall_summary.SetBVRITab(self.bvri_tab)

        self.prior_series_selected = None
        self.prior_star_selected = None
        
        self.book.show()

        SessionGlobal.notifier.register(requestor=self,
                                        variable="current_star",
                                        condition="value_change",
                                        callback=self.starchange_cb,
                                        debug="text_tabs")
        
    def notebook(self):
        return self.book

    def starchange_cb(self, variable, condition, data):
        self.update(SessionGlobal.current_star)

    def update(self, star_selected):
        #if series_selected != self.prior_series_selected:
        #    self.prior_series_selected = series_selected
        #    photfile = star_selected.name + series_selected.filter + ".phot"
        #    SetFromFile(self.analy_tab.tab_buffer, \
        #                os.path.join(homedir, photfile))
            
        if star_selected != self.prior_star_selected:
            self.prior_star_selected = star_selected
            TextTab.SetFromFile(self.strategy_tab.tab_buffer, "/home/ASTRO/STRATEGIES/" + star_selected.name + ".str")
            TextTab.SetFromFile(self.catalog_tab.tab_buffer, "/home/ASTRO/CATALOGS/" + star_selected.name)
            #self.analy_tab.set_star(star_selected.name)
            self.err_tab.set_star()
            comp_analy.overall_summary.SetTarget(star_selected.name)
            self.bvri_tab.refresh_file_quiet()
            self.comp_tab.SetTarget(star_selected.name)

STD_FORMAT = '%(asctime)s | %(levelname)1.1s | %(filename)s:%(lineno)d (%(funcName)s) | %(message)s'

# This is the upper-right quad of the right-hand screen
class ImagePane:
    def __init__(self):
        self.mainwin = gtk.VBox(spacing = 2)
        self.top_win = gtk.HBox(spacing = 2)
        self.main_image = FITSViewer.FitsViewer("RawWin")
        self.renderer = FITSViewer.RendererPane(self.main_image)

        self.mainwin.pack_start(self.top_win, fill=True, expand=True, padding=0)
        self.top_win.pack_start(self.main_image.get_widget(),
                                expand=False, fill=False, padding = 0)

        self.widget_area = gtk.VBox(spacing=2)

        self.stat_box = gtk.HBox(spacing=2)
        self.stat_box.set_homogeneous(True)

        self.label_max = gtk.Label(label='Max = ')
        self.label_max.set_justify(gtk.Justification.LEFT)
        self.label_min = gtk.Label(label='Min = ')
        self.label_min.set_justify(gtk.Justification.LEFT)
        self.label_median = gtk.Label(label='Median = ')
        self.label_median.set_justify(gtk.Justification.LEFT)

        self.stat_box.pack_start(self.label_max, fill=False, expand=False, padding=0)
        self.stat_box.pack_start(self.label_min, fill=False, expand=False, padding=0)
        self.stat_box.pack_start(self.label_median, fill=False, expand=False, padding=0)
        self.widget_area.pack_start(self.stat_box, fill=False,
                                    expand=False, padding=0)

        self.widget_area.pack_start(self.renderer.widget(), fill=False,
                                    expand=False, padding=0)

        self.magnifier = FITSViewer.FitsViewer("RawWin.Magnifier")
        self.magnifier.set_size(200, 200)
        self.magnifier.zoom_to(10)
        self.attach_slave_renderer(self.magnifier)
        self.attach_slave_click(self.magnifier)

        self.auto_set_button = gtk.Button(label="Auto Set")
        self.auto_set_button.connect("clicked", self.auto_set_button_cb, None)
        self.widget_area.pack_start(self.auto_set_button, fill=False, expand=False, padding=0)
        
        self.widget_area.pack_start(self.magnifier.get_widget(), fill=False, expand=False, padding=0)
        self.dark_subtract_button = gtk.CheckButton(label="Dark Subtract")
        self.widget_area.pack_start(self.dark_subtract_button, fill=False, expand=False, padding=0)
        self.dark_file_label = gtk.Label(label='Darkfile: ')
        self.dark_file_label.set_justify(gtk.Justification.LEFT)
        self.widget_area.pack_start(self.dark_file_label, fill=False, expand=False, padding=0)
        self.dark_select_button = gtk.Button(label="Dark Select")
        self.widget_area.pack_start(self.dark_select_button, fill=False, expand=False, padding=0)
        self.dark_select_button.connect("clicked", self.select_dark_cb, None)

        self.top_win.pack_start(self.widget_area, fill=False, expand=False, padding=0)

        self.mainwin.show_all()

    def release_slave(self, slave_to_release):
        self.main_image.release_slave(slave_to_release)

    def attach_slave_renderer(self, new_slave):
        self.main_image.follow_renderer(new_slave)

    def attach_slave_click(self, new_slave):
        self.main_image.reset_view_on_click(new_slave)

    def select_dark_cb(self, widget, data=None):
        chooser = gtk.FileChooserDialog(title="Select Dark", action=gtk.FILE_CHOOSER_ACTION_OPEN,
                                        buttons=(gtk.STOCK_CANCEL, gtk.RESPONSE_CANCEL, gtk.STOCK_OPEN,gtk.RESPONSE_OK))
        chooser.set_current_folder(SessionGlobal.homedir)
        chooser.set_filename("dark30.fits")
        filter = gtk.FileFilter()
        filter.set_name("FITS files")
        filter.add_pattern("*.fits")
        chooser.add_filter(filter)
        response = chooser.run()
        if response == gtk.RESPONSE_OK:
            self.darkfilename = chooser.get_filename()
        chooser.destroy()
        self.update_dark()

    def update_dark(self):
        global current_dark_file
        current_dark_file = self.darkfilename
        self.dark_file_label.set_text('Darkfile: '+self.darkfilename)

        self.main_image.set_darkfile(current_dark_file)

    def auto_set_button_cb(self, widget, data=None):
        print("auto_set_button_cb()")
        #self.main_image.fitsimage.enable_autocuts('on')
        #self.main_image.fitsimage.auto_levels()
        #self.update_white_black()

    def set_magnifier_xy(self, x, y):
        self.magnifier.set_center(x, y)

    def get_widget(self):
        return self.mainwin

    def set_images(self, primary_image, thumb_list):
        self.main_image.load_file(primary_image)
        self.magnifier.zoom_to(10)
        self.magnifier.load_file(primary_image)

        (min_data_val, max_data_val, median_data_val) = self.main_image.get_statistics()
        self.label_min.set_text("Min = " + str(min_data_val))
        self.label_max.set_text("Max = " + str(max_data_val))
        self.label_median.set_text("Median = " + str(median_data_val))

        #self.update_white_black()
        
################################################################
## main_image_click_cb is not used any more
################################################################
def main_image_click_cb(widget, event):
    #print "main_image_click_cb()"
    #print widget
    #print event
    #print event.x, event.y
    SessionGlobal.image_region.set_magnifier_xy(event.x, event.y)
    SessionGlobal.thumbs.set_center(event.x, event.y)
    
def do_startup():
    global aavso_base, csv_summary_file
    global log_summary_file, shell_summary_file
    
    astro_db.Init()
    datedir = None
    initialize_from_csv = False
    try:
        opts, args = getopt.getopt(sys.argv[1:], "ad:", ["","dir="])
    except getopt.GetoptError:
        print('usage: session_summary.py [-a] [-d 9-2-2022]')
        sys.exit(2)
    for opt,arg in opts:
        if opt in ("-d", "--dir"):
            datedir = arg
            if not os.path.isdir(datedir):
                datedir = os.path.join('/home/IMAGES', datedir)
                if not os.path.isdir(datedir):
                    print("Cannot find directory associated with ", arg)
                    quit()
            SessionGlobal.homedir = datedir
        elif opt == "-a":
            initialize_from_csv = True

    if datedir == None:
        fcd = gtk.FileChooserDialog(title="Session Summary (Select session folder)",
                                    action = gtk.FileChooserAction.SELECT_FOLDER)
        fcd.add_buttons(gtk.STOCK_CANCEL, gtk.ResponseType.CANCEL,
                        gtk.STOCK_OK, gtk.ResponseType.OK)

        fcd.set_default_response(gtk.ResponseType.OK)
        fcd.set_current_folder("/home/IMAGES")
        response = fcd.run()
        if response == gtk.ResponseType.OK:
            SessionGlobal.homedir = fcd.get_filename()
        else:
            print('Closed. No directory selected.')
            sys.exit(2)
        fcd.destroy()
            
    csv_summary_file = os.path.join(SessionGlobal.homedir, "aavso.csv")
    log_summary_file = os.path.join(SessionGlobal.homedir, "session.log")
    shell_summary_file = os.path.join(SessionGlobal.homedir, "session.shell")

    history.BeginStartup(datedir)

    SessionGlobal.db_obj = astro_db.AstroDB(SessionGlobal.homedir)
    SessionGlobal.db = SessionGlobal.db_obj.GetData()

    print("Initializing from ", SessionGlobal.homedir)

    if not initialize_from_csv:
        # Default initialization uses PURPOSE/OBJECT in FITS files 
        Initialize_Summary_from_FITS(SessionGlobal.homedir)
    else:
        Initialize_Summary_from_csv(SessionGlobal.homedir, csv_summary_file)

    comp_analy.overall_summary = comp_analy.OverallSummary(None) # or None
    history.WaitForStartupComplete()
        
def RebuildReport(widget):
    command = "/home/mark/ASTRO/CURRENT/TOOLS/BVRI/create_aavso_report.py "
    command += "-i "
    command += os.path.join(SessionGlobal.homedir, "aavso_report.db")
    command += " -o "
    command += os.path.join(SessionGlobal.homedir, "aavso.report.txt")
    os.system(command)

    command = "/home/mark/ASTRO/CURRENT/TOOLS/OBS_RECORD/import_bvri "
    command += "-d "
    command += SessionGlobal.homedir
    os.system(command)
    
    SessionGlobal.global_summary.refresh()

def program_shutdown(widget):
    print("***Completed work in directory ", SessionGlobal.homedir)
    exit()

def get_descendant(widget, child_name, level, doPrint=False):
    if widget is not None:
        if doPrint:
            print("-"*level + " :: " + widget.get_name())
        else:
            if doPrint:  print("-"*level + "None")
            return
    if hasattr(widget, 'get_children') and callable(getattr(widget, 'get_children')):
        children = widget.get_children()
        # /*** For each child ***/
        for child in children:
            if child is not None:
                get_descendant(child, child_name,level+1,doPrint) # //search the child

if __name__ == "__main__":

    do_startup()

    SessionGlobal.root = gtk.Window.new(gtk.WindowType.TOPLEVEL)
    SessionGlobal.root_r = gtk.Window.new(gtk.WindowType.TOPLEVEL)

    screen = SessionGlobal.root.get_screen()
    monitors = []
    nmons = screen.get_n_monitors()
    if nmons >= 2:
        SessionGlobal.root.move(0,0)
        geom = screen.get_monitor_geometry(0)
        SessionGlobal.root_r.move(geom.width, 0)

    #root_menu = Menu(root)
    #root.config(menu=root_menu)
    #file_menu = Menu(root_menu)
    #file_menu.add_command(label="Exit", command=program_shutdown)
    #root_menu.add_cascade(label="File", menu=file_menu)
    
    # Create content of left window/screen

    print("Ready to create masterbox")
    ################################
    # Left Window Top-level Structure
    ################################
    masterbox = gtk.HBox(homogeneous=False, spacing=3)
    mainbox_l = gtk.VBox(homogeneous=False, spacing=1)
    masterbox.pack_start(mainbox_l, expand=False, fill=False, padding=1)
    menubox = gtk.HBox(homogeneous=False, spacing=3)
    mainbox_l.pack_start(menubox, expand=False, fill=False, padding=1)

    master_quit = gtk.Button(label="Exit")
    master_quit.connect("clicked", program_shutdown)
    menubox.pack_start(master_quit, expand=False, fill=False, padding=3)

    master_build_report = gtk.Button(label="Rebuild Report")
    master_build_report.connect("clicked", RebuildReport)
    menubox.pack_start(master_build_report, expand=False, fill=False, padding=3)
    
    SessionGlobal.root.add(masterbox)
    
    ################################
    # Top-level left and right boxes
    ################################
    mainbox_r = gtk.HBox(homogeneous=False, spacing=3)
    SessionGlobal.root_r.add(mainbox_r)
    print("right root size = ", SessionGlobal.root_r.get_size())
    
    ################################
    # Left Side
    ################################
    #Initialize_Summary('/home/IMAGES/1-27-2016', 'aavso.csv',
    #'session.log')
    print("Initializing ChartWindow()")
    SessionGlobal.chart_window = Chart.ChartWindow()
    mainbox_l.pack_start(SessionGlobal.chart_window.widget(), expand=False, fill=False, padding=0)

    ################################
    # Left monitor: text tabs
    ################################
    print("Creating TextTabs.")
    # Create and initialize the text tabs on the left window (logs, shell, photometry)
    SessionGlobal.text_tabs = TextTabs()
    masterbox.pack_start(SessionGlobal.text_tabs.notebook(), expand=True, fill=True, padding=0)

    ################################
    # Right Side Top-level Structure
    ################################

    ################################
    # List of stars
    ################################
    print("Ready to create scrolled_tree.")
    scrolled_tree = gtk.ScrolledWindow()
    scrolled_tree.set_policy(gtk.PolicyType.NEVER,
                             gtk.PolicyType.ALWAYS)
    tvexample = SessionStarView.SessionStarView(None)
    print("Creation of SessionStarView completed.")
    scrolled_tree.add(tvexample.window())
    mainbox_r.pack_start(scrolled_tree, expand=False, fill=False, padding=0)

    ################################
    # Right side: notebook
    ################################
    imageselectornotebook = gtk.Notebook()
    mainbox_r.pack_start(imageselectornotebook, expand=True, fill=True, padding=0)
    imageselectornotebook.set_tab_pos(gtk.PositionType.TOP)

    ################################
    # Tab #1: scrolled_image
    ################################
    scrolled_image = gtk.ScrolledWindow()
    scrolled_image.set_policy(gtk.PolicyType.NEVER,
                              gtk.PolicyType.ALWAYS)
    image_select_pane = FITSTab.ImageFileSelectPane()
    raw_viewer = FITSTab.FITSTab(image_select_pane.get_widget())
    image_select_pane.set_viewer(raw_viewer)
    scrolled_image.add(raw_viewer.get_widget())
    SessionGlobal.image_region = raw_viewer.viewer

    ################################
    # Tab #2: scrolled_stackimage
    ################################
    scrolled_stackimage = StackTab.StackTab()
    
    ################################
    # Tab #3: thumbnails
    ################################
    thumbnail_vert_slice = gtk.VBox(homogeneous=False, spacing=0)
    SessionGlobal.thumbs = Thumbnail.ThumbnailPane()
    thumbnail_vert_slice.pack_start(SessionGlobal.thumbs.get_widget(),
                                    expand=True, fill=True, padding=2)

    imageselectornotebook.append_page(scrolled_image, gtk.Label(label="Single Image"))
    imageselectornotebook.append_page(scrolled_stackimage.get_widget(),
                                      gtk.Label(label="Stacked Image"))
    imageselectornotebook.append_page(thumbnail_vert_slice, gtk.Label(label="Thumbnails"))

    print("right root size = ", SessionGlobal.root_r.get_size())
    
    # on the far right, create a vertical box that will have the
    # "image_region" on top and the "stack_region" on the bottom
    #imagebox = gtk.VBox(homogeneous=False, spacing=3)
    #mainbox_r.pack_end(imagebox, expand=False, fill=False, padding=0)
    
    print("right root size = ", SessionGlobal.root_r.get_size())

    # Create and initialize the full-frame display window (upper
    # right) that shows the currently-selected 512x512 image.
    #print("Creating ImagePane().")
    #SessionGlobal.image_region = ImagePane()
    #imagebox.pack_start(SessionGlobal.image_region.get_widget(), expand=False,
    #                    fill=False, padding=2)

    # Put a separator between the image_region and the stack_region
    #sep_1 = gtk.HSeparator()
    #imagebox.pack_start(sep_1, False, True, 1)
    #sep_1.show()

    # Create the stacking area
    #print("Creating StackPane.")
    #SessionGlobal.stacker = StackPane.StackPane()
    #imagebox.pack_start(SessionGlobal.stacker.get_widget(), False, True, 1)

    SessionGlobal.root_r.set_size_request(1910, 1053)
    print("right root size = ", SessionGlobal.root_r.get_size())

    masterbox.show_all()
    SessionGlobal.root_r.show_all()
    SessionGlobal.root.show_all()
    SessionGlobal.root.maximize()
    SessionGlobal.root_r.fullscreen()
    
    SessionGlobal.notifier.trigger(trigger_source=None,
                                   variable="current_star",
                                   condition="value_change")

    print("right root size = ", SessionGlobal.root_r.get_size())
    print("Entering gtk.main()")

    #get_descendant(SessionGlobal.root_r, "", 1, doPrint=True)

    #pdb.set_trace()
    gtk.main()
        
    
