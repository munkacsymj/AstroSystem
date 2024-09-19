#!/usr/bin/python3

import sys
import socket
import math
import re
import os.path
import gi
gi.require_version('Gtk', '3.0')
from gi.repository import Gtk as gtk
import logging

global mount

def main():
    gtk.main()

def program_shutdown(widget):
    exit()

class MountConnection:
    def __init__(self, mount_address, mount_port):
        self.sock_id = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock_id.connect((mount_address, mount_port))
        self.send(":U2#")

    def send(self, msg):
        totalsent = 0
        while totalsent < len(msg):
            sent = self.sock_id.send(bytes(msg[totalsent:], 'ascii'))
            if sent == 0:
                raise RuntimeError("socket connection broken")
            totalsent = totalsent + sent

    def recv_string(self):
        chunks = []
        quit = False
        while not quit:
            chunk = self.sock_id.recv(1)
            chunks.append(chunk)
            quit = (chunk == '#')
        return b''.join(chunks)

    def recv_char(self):
        return self.sock_id.recv(1)

class MountTab:
    def __init__(self, parent_notebook, tab_label):
        self.whole_box = gtk.VBox(spacing = 2)
        parent_notebook.append_page(self.whole_box, gtk.Label(tab_label))
        
        self.whole_box.show()

        self.elements = []

    def add_child(self, child):
        self.elements.append(child)

    def do_refresh(self):
        for tab in self.elements:
            tab.do_refresh()

    def content_vbox(self):
        return self.whole_box

class SimpleParam:
    def __init__(self, parent_mounttab, param_name, fetcher, fetcher_data, converter, converter_data, unit_string):
        self.simple_frame = gtk.Frame(label=param_name)
        self.param_label = gtk.Label("")
        self.simple_frame.add(self.param_label)
        parent_mounttab.content_vbox().pack_start(self.simple_frame, False, False, 0)
        self.simple_frame.show()

        self.fetcher_cb = fetcher
        self.converter_cb = converter
        self.units_string = unit_string
        self.data_for_fetcher = fetcher_data
        self.data_for_converter = converter_data
        self.my_name = param_name

    def do_refresh(self):
        raw_string = self.fetcher_cb(self.data_for_fetcher)
        if self.converter_cb == None:
            converted_string = raw_string
        else:
            converted_string = self.converter_cb(raw_string)
        self.param_label.set_text(converted_string + " " + self.units_string)
        self.param_label.show()
        print("completed refresh for ",self.my_name, ": ", converted_string)

class align_content:
    def __init__(self, parent_vbox):
        self.parent = parent_vbox
        

def StringFetcher(query_string):
    global mount
    mount.send(query_string)
    return mount.recv_string()

def OneCharFetcher(query_string):
    global mount
    mount.send(query_string)
    return mount.recv_char()

def StripHashChar(input_string):
    return input_string[:-1]

def ConvertMeridianSide(input_string):
    if input_string[0] == "1":
        return "Both sides of meridian allowed"
    if input_string[0] == "2":
        return "Only objects west of meridian allowed"
    if input_string[0] == "3":
        return "Only objects east of meridian allowed"
    return "Invalid response"
    
def ConvertGuidingStatus(input_string):
    if input_string[0] == "0":
        return "Not guiding"
    if input_string[0] == "1":
        return "Guiding in RA only"
    if input_string[0] == "2":
        return "Guiding in Declination only"
    if input_string[0] == "3":
        return "Guiding in both axes"
    return "Invalid response"
    
def ConvertActiveStatus(input_string):
    if input_string[0] == "0":
        return "Inactive"
    if input_string[0] == "1":
        return "Active"
    return "Invalid response"

def ConvertMountStatus(input_string):
    if input_string[0] == "0":
        return "Tracking"
    if input_string == "1#":
        return "Stopped"
    if input_string == "2#":
        return "Slewing to park position"
    if input_string == "3#":
        return "Unparking"
    if input_string == "4#":
        return "Slewing to home position"
    if input_string == "5#":
        return "Parked"
    if input_string == "6#":
        return "Slewing"
    if input_string == "7#":
        return "Stationary (tracking off)"
    if input_string == "8#":
        return "Low-temp motors inhibited"
    if input_string == "9#":
        return "Beyond Limits"
    if input_string == "10#":
        return "Satellite tracking"
    if input_string == "11#":
        return "Needs user intervention (see manual)"
    if input_string == "98#":
        return "Unknown status"
    if input_string == "99#":
        return "Error"
    return "Invalid response"
    
def do_quit(w, data):
    gtk.main_quit()

def do_park(data, menu_item):
    global mount
    print("do_park(",data,")")
    if data == 1:
        # execute a park command
        print( "execute: park command")
        mount.send(":hP#")
    if data == 0:
        # execute an unpark command
        print ("execute: unpark command")
        mount.send(":PO#")

def do_refresh(data, menu_item):
    # refresh all fields?
    global all_tabs
    for tab in all_tabs:
        tab.do_refresh()

if __name__ == "__main__":
    global mount
    global all_tabs

    mount = MountConnection("gm2000", 3490)

    root = gtk.Window.new(gtk.WindowType.TOPLEVEL)
    root.connect("destroy", lambda w,d: gtk.main_quit(), "WM destroy")
    root.set_title("Mount Monitor")
    root.set_size_request(600, 275)

    menu_items = (
        ( "/_File",        None,       None, 0, "<Branch>" ),
        ( "/_File/_Quit",  None,       do_quit, 0, None ),
        ( "/_View",        None,       None, 0, "<Branch>" ),
        ( "/_View/_Refresh", None,     do_refresh, 0, None ),
        ( "/_Park",        None,       None, 0, "<Branch>" ),
        ( "/_Park/_Park",  None,       do_park, 1, None ),
        ( "/_Park/_Unpark", None,      do_park, 0, None ),
        )


    item_factory = gtk.ItemFactory(gtk.MenuBar, "<main>", accel_group=None)
    item_factory.create_items(menu_items)
    #root.add_accel_group(accel_group)
    menu_bar = item_factory.get_widget("<main>")
    
    masterbox = gtk.VBox(homogeneous=False, spacing=3)
    root.add(masterbox)

    masterbox.pack_start(menu_bar, False, True, 0)

    book = gtk.Notebook()
    book.set_tab_pos(gtk.POS_TOP)
    masterbox.pack_start(book, True, True, 0)

    root.show()
    book.show()
    menu_bar.show()
    masterbox.show()

    # ALIGNMENT TAB
    alignment_tab = MountTab(book, "Alignment")
    alignment_tab.add_child(SimpleParam(alignment_tab,
                                        "Num Alignment Stars",
                                        StringFetcher,
                                        ":getalst#",
                                        StripHashChar,
                                        None,
                                        "(points)"))

    alignment_tab.add_child(SimpleParam(alignment_tab,
                                        "Meridian Sides Allowed",
                                        StringFetcher,
                                        ":GMF#",
                                        ConvertMeridianSide,
                                        None,
                                        ""))

    alignment_tab.add_child(SimpleParam(alignment_tab,
                                        "Guiding Status",
                                        StringFetcher,
                                        ":GMF#",
                                        ConvertGuidingStatus,
                                        None,
                                        ""))

    alignment_tab.add_child(SimpleParam(alignment_tab,
                                        "Dual-Axis Tracking",
                                        OneCharFetcher,
                                        ":Gdat#",
                                        ConvertActiveStatus,
                                        None,
                                        ""))

    alignment_tab.add_child(SimpleParam(alignment_tab,
                                        "Refraction Correction",
                                        OneCharFetcher,
                                        ":GREF#",
                                        ConvertActiveStatus,
                                        None,
                                        ""))

    # POINTING TAB
    pointing_tab = MountTab(book, "Pointing")
    pointing_tab.add_child(SimpleParam(pointing_tab,
                                       "Altitude",
                                       StringFetcher,
                                       ":GA#",
                                       StripHashChar,
                                       None,
                                       "(d:m:s)"))
    
    pointing_tab.add_child(SimpleParam(pointing_tab,
                                       "Azimuth",
                                       StringFetcher,
                                       ":GZ#",
                                       StripHashChar,
                                       None,
                                       "(d:m:s)"))

    pointing_tab.add_child(SimpleParam(pointing_tab,
                                       "Declination",
                                       StringFetcher,
                                       ":GD#",
                                       StripHashChar,
                                       None,
                                       "(dd:mm:ss)"))

    pointing_tab.add_child(SimpleParam(pointing_tab,
                                       "Right Ascension",
                                       StringFetcher,
                                       ":GR#",
                                       StripHashChar,
                                       None,
                                       "(hh:mm:ss)"))

    pointing_tab.add_child(SimpleParam(pointing_tab,
                                       "Mount Side",
                                       StringFetcher,
                                       ":pS#",
                                       StripHashChar,
                                       None,
                                       ""))
    
    pointing_tab.add_child(SimpleParam(pointing_tab,
                                       "Time Until Limit Reached",
                                       StringFetcher,
                                       ":Gmte#",
                                       StripHashChar,
                                       None,
                                       "(minutes)"))
    
    
    

    # NETWORK TAB
    network_tab = MountTab(book, "Network")
    network_tab.add_child(SimpleParam(network_tab,
                                      "Mount IP Addr",
                                      StringFetcher,
                                      ":GIP#",
                                      StripHashChar,
                                      None,
                                      ""))

    # TIME TAB
    time_tab = MountTab(book, "Time/Date")
    time_tab.add_child(SimpleParam(time_tab,
                                   "Julian Date",
                                   StringFetcher,
                                   ":GJD#",
                                   StripHashChar,
                                   None,
                                   ""))

    time_tab.add_child(SimpleParam(time_tab,
                                   "Local Time",
                                   StringFetcher,
                                   ":GL#",
                                   StripHashChar,
                                   None,
                                   "(hh:mm:ss)"))

    time_tab.add_child(SimpleParam(time_tab,
                                   "Sidereal Time",
                                   StringFetcher,
                                   ":GS#",
                                   StripHashChar,
                                   None,
                                   "(hh:mm:ss)"))


    # SITE TAB
    site_tab = MountTab(book, "Site")
    site_tab.add_child(SimpleParam(site_tab,
                                   "Site Elevation",
                                   StringFetcher,
                                   ":Gev#",
                                   StripHashChar,
                                   None,
                                   "(meters)"))

    site_tab.add_child(SimpleParam(site_tab,
                                   "Site Longitude",
                                   StringFetcher,
                                   ":Gg#",
                                   StripHashChar,
                                   None,
                                   "(dd:mm:ss)"))

    site_tab.add_child(SimpleParam(site_tab,
                                   "Site Latitude",
                                   StringFetcher,
                                   ":Gt#",
                                   StripHashChar,
                                   None,
                                   "(dd:mm:ss)"))

    # STATUS TAB
    status_tab = MountTab(book, "Status")
    status_tab.add_child(SimpleParam(status_tab,
                                     "Mount Status",
                                     StringFetcher,
                                     ":Gstat#",
                                     ConvertMountStatus,
                                     None,
                                     ""))
    
    status_tab.add_child(SimpleParam(status_tab,
                                     "Firmware Date",
                                     StringFetcher,
                                     ":GVD#",
                                     StripHashChar,
                                     None,
                                     ""))
    
    status_tab.add_child(SimpleParam(status_tab,
                                     "Firmware Number",
                                     StringFetcher,
                                     ":GVN#",
                                     StripHashChar,
                                     None,
                                     ""))
    
    
    # TRACKING TAB
    tracking_tab = MountTab(book, "Slew Rates & Tracking")
    tracking_tab.add_child(SimpleParam(tracking_tab,
                                       "Slew Rate",
                                       StringFetcher,
                                       ":GMs#",
                                       StripHashChar,
                                       None,
                                       "(deg/sec)"))
    
    tracking_tab.add_child(SimpleParam(tracking_tab,
                                       "RA Axis Position",
                                       StringFetcher,
                                       ":GaXa#",
                                       StripHashChar,
                                       None,
                                       "(deg)"))
    
    tracking_tab.add_child(SimpleParam(tracking_tab,
                                       "Meridian Limit (tracking)",
                                       StringFetcher,
                                       ":Glmt#",
                                       StripHashChar,
                                       None,
                                       "(deg beyond meridian?)"))
    
    pointing_tab.add_child(SimpleParam(tracking_tab,
                                       "Time Until Limit Reached",
                                       StringFetcher,
                                       ":Gmte#",
                                       StripHashChar,
                                       None,
                                       "(minutes)"))
    

    all_tabs = [alignment_tab, pointing_tab, network_tab, time_tab, site_tab, status_tab, tracking_tab]
        
    #do_refresh(None, None)
    main()
        
