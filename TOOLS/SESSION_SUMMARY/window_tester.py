#!/usr/bin/python3
import gi
from gi.repository import Gtk as gtk
from gi.repository import Gdk as gdk
import time

class MainApp:
    def __init__(self, popup):
        self.main = gtk.Window()
        self.button = gtk.Button(label="Push")
        self.button.connect("clicked", self.callback, None)
        self.main.add(self.button)
        self.main.show_all()
        self.popup = popup

    def callback(self, widget, data):
        print("pushed")
        self.popup.pwin.show_all()
        counter = 0
        while gtk.events_pending():
            counter += 1
            gtk.main_iteration()
            if not gtk.events_pending():
                time.sleep(0.1)
        print("counter = ", counter)
        
        time.sleep(5)
        print("events_pending = ", gtk.events_pending())
        counter = 0
        while gtk.events_pending():
            counter += 1
            gtk.main_iteration()
        print("counter = ", counter)

class Popup:
    def __init__(self):
        self.pwin = gtk.Window()
        self.pbutton = gtk.Button(label="running")
        self.pwin.add(self.pbutton)
        #self.pwin.show_all()
        self.pwin.hide()

p = Popup()
app = MainApp(p)
gtk.main()
