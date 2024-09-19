from gi.repository import Gtk as gtk
import SessionGlobal
import os.path

import FITSViewer

class FITSTab:
    def __init__(self, image_select_widget):
        self.FITSbox = gtk.HBox()
        self.Auxbox = gtk.VBox()
        self.FITSbox.pack_start(self.Auxbox, padding=0, fill=False, expand=False)
        self.Auxbox.pack_start(image_select_widget, padding=0, fill=True, expand=True)
        self.starname_label = gtk.Label()
        self.viewer = FITSViewer.FitsViewer("FITS Image")
        self.viewer.set_size(1270, 1000)
        self.viewer.SetZoomToFit()

        self.magnifier = FITSViewer.FitsViewer("FITSImage_mag")
        self.magnifier.set_size(300, 300)
        self.magnifier.zoom_to(10)

        r_box = gtk.HBox()
        self.Auxbox.pack_start(r_box, padding=0, fill=False, expand=False)
        self.renderer = FITSViewer.RendererPane(self.viewer, r_box)
        self.Auxbox.pack_start(self.magnifier.get_widget(), padding=0, fill=False, expand=False)
        
        self.viewer.follow_renderer(self.magnifier)
        self.viewer.reset_view_on_click(self.magnifier)

        scroller = gtk.ScrolledWindow()
        scroller.add_with_viewport(self.viewer.get_widget())

        self.FITSbox.pack_start(scroller, expand=True, fill=True, padding=0)

    def get_widget(self):
        return self.FITSbox

    def load_file(self, filename):
        self.magnifier.load_file(filename)
        self.viewer.load_file(filename)

class ImageFileSelectPane:
    def __init__(self):
        self.mainwin = gtk.ScrolledWindow()
        self.mainbox = gtk.VBox()
        self.mainwin.add(self.mainbox)
        SessionGlobal.notifier.register(requestor=self,
                                        variable="current_star",
                                        condition="value_change",
                                        callback=self.starchange_cb,
                                        debug="ImageFileSelectPane")

    def starchange_cb(self, variable, condition, data):
        print("ImageFileSelectPane: starchange_cb() invoked.")
        self.delete_all_files()
        self.set_files(self.build_filelist())

    def build_filelist(self):
        obs = SessionGlobal.current_star.obs_seq
        filelist = {}
        for color in obs:
            filelist[color] = [x.filename for x in obs[color].exposures]
        return filelist
        
    def set_viewer(self, controlled_viewer):
        self.controlled_viewer = controlled_viewer # associated FITSTab

    def delete_all_files(self):
        for widget in self.mainbox.get_children():
            self.mainbox.remove(widget)
            #widget.delete()

    def set_files(self, filelist_dict):
        print("ImageFileSelectPane.set_files() with filelist: ", filelist_dict)
        self.delete_all_files()
        group = None

        for color in ["B","Bc","V","Vc","R","Rc","I","Ic"]:
            if color not in filelist_dict:
                print("Skipping color ", color)
                continue
            #print("Processing color ", color, " with raw filelist = ", filelist_dict[color])
            filelist_dict[color].sort()
            filelist = filelist_dict[color]
            color = color[0]
            #print("FITSPane.set_files processing color ", color)
            #print("filelist = ", filelist)
            self.mainbox.pack_start(gtk.Label(label=color), padding=0, fill=True, expand=False)
            for f in filelist:
                simple_name = os.path.basename(f)
                b = gtk.RadioButton(label=simple_name, group=group)
                b.connect("toggled", self.button_cb, f)
                self.mainbox.pack_start(b, padding=0, fill=True, expand=False)
                if group == None:
                    group = b
                    first_filename = f
        self.mainwin.show_all()
        if group != None:
            self.controlled_viewer.load_file(first_filename)
            
    def button_cb(self, widget, pathname):
        if widget.get_active():
            self.controlled_viewer.load_file(pathname)
        
    def get_widget(self):
        return self.mainwin
