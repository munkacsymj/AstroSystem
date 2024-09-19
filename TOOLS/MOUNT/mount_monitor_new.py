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
        #print("Sending message: ", msg)
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
            #print("Received '", chunk)
            chunks.append(chunk)
            quit = (chunk == b'#')
            #print("quit == ", quit)
        merged = b''.join(chunks)
        #print("recv_string() merged = ", merged)
        result = str(merged, "utf-8")
        #print("recv_string() returning ", result)
        return result

    def recv_char(self):
        return self.sock_id.recv(1)

class MountTab:
    def __init__(self, builder, tab_label):
        self.elements = [] # each element is a SimpleParam
        self.label = builder.get_object(tab_label)

    def add_child(self, child):
        self.elements.append(child)

    def do_refresh(self):
        integrated_string = ""
        for tab in self.elements:
            integrated_string += tab.do_refresh()
        self.label.set_text(integrated_string)
        self.label.show()

class SimpleParam:
    def __init__(self, parent_mounttab, param_name, fetcher, fetcher_data, converter, converter_data, unit_string):
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
        return (self.my_name + ": " + converted_string + " " + self.units_string + "\n")

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
    if isinstance(input_string, bytes):
        input_string = input_string.decode('UTF-8')
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
    if isinstance(input_string, bytes):
        input_string = input_string.decode('UTF-8')
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

def do_refresh():
    # refresh all fields?
    global all_tabs
    for tab in all_tabs:
        tab.do_refresh()

def MainQuit(button):
    print("MainQuit() invoked")
    gtk.main_quit()

handlers = {
    "MainQuit": MainQuit,
}

if __name__ == "__main__":
    global mount
    global all_tabs

    builder = gtk.Builder()
    builder.add_from_file("/home/mark/ASTRO/CURRENT/TOOLS/MOUNT/mount_monitor.glade")
    topwindow = builder.get_object("topwindow")
    builder.connect_signals(handlers)

    mount = MountConnection("gm2000", 3490)

    # ALIGNMENT TAB
    alignment_tab = MountTab(builder, "align_tab_label")
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

    alignment_tab.add_child(SimpleParam(alignment_tab,
                                        "Tracking Rate",
                                        StringFetcher,
                                        ":GT#",
                                        StripHashChar,
                                        None,
                                        "Hz, divide by 4 to get arcsec/sec"))

    # POINTING TAB
    pointing_tab = MountTab(builder, "pointing_tab_label")
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
    network_tab = MountTab(builder, "network_tab_label")
    network_tab.add_child(SimpleParam(network_tab,
                                      "Mount IP Addr",
                                      StringFetcher,
                                      ":GIP#",
                                      StripHashChar,
                                      None,
                                      ""))

    # TIME TAB
    time_tab = MountTab(builder, "datetime_tab_label")
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
    site_tab = MountTab(builder, "site_tab_label")
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
    status_tab = MountTab(builder, "status_tab_label")
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
    tracking_tab = MountTab(builder, "tracking_tab_label")
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
        
    do_refresh()
    topwindow.show_all()
    main()
        
