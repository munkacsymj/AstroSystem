#!/usr/bin/env python3
# -*- coding: utf-8 -*-
#
#  test_headerbar_events.py
#
#  Copyright 2017 John Coppens <john@jcoppens.com>
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
#  MA 02110-1301, USA.
#
#


from gi.repository import Gtk, Gdk

class MainWindow(Gtk.Window):
    def __init__(self):
        super(MainWindow, self).__init__()
        self.connect("destroy", lambda x: Gtk.main_quit())
        self.set_default_size(200, -1)

        hdrbar = Gtk.HeaderBar(title = "Header bar")
        hdrbar.connect("button-press-event", self.hdrbar_button_press)

        drawing_area = Gtk.DrawingArea()
        drawing_area.set_size_request(30, 30)
        drawing_area.add_events(Gdk.EventMask.BUTTON_PRESS_MASK)
        drawing_area.connect("button-press-event", self.area_button_press)

        #frame = Gtk.Frame()
        #frame.add(drawing_area)

        hdrbar.pack_start(drawing_area)

        self.add(hdrbar)
        self.show_all()

    def area_button_press(self, btn, event):
        print("Button pressed on Drawing Area")
        return True

    def hdrbar_button_press(self, btn, event):
        print("Button pressed on Header Bar")
        return False


    def run(self):
        Gtk.main()


def main(args):
    mainwdw = MainWindow()
    mainwdw.run()

    return 0

if __name__ == '__main__':
    import sys
    sys.exit(main(sys.argv))
