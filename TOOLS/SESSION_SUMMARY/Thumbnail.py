import os
import sys
import gi
from gi.repository import Gtk as gtk
from gi.repository import Gdk
import FITSViewer
import SessionGlobal
import comp_analy

def ToolRoot(start):
    while True:
        (head,tail) = os.path.split(start)
        if tail == "TOOLS":
            return head
        elif tail == '':
            raise Exception("filepath does not contain TOOLS")
        else:
            start = head

sys.path.insert(1, ToolRoot(__file__))
from PYTHON_LIB.ASTRO_DB_LIB import astro_db, astro_directive
from PYTHON_LIB.IMAGE_LIB import filter as filter_module

################################################################
##    class ThumbnailPane
################################################################
class ThumbnailPane:
    def __init__(self):
        self.scrolled_win = gtk.ScrolledWindow()
        self.v_box = gtk.VBox(spacing=2)
        self.scrolled_win.add_with_viewport(self.v_box)
        self.tb_list = [] # list of children FilterPanes
        self.scrolled_win.show_all()
        SessionGlobal.notifier.register(requestor=self,
                                        variable="current_star",
                                        condition="value_change",
                                        callback=self.starchange_cb,
                                        debug="ThumbnailPane")

    def starchange_cb(self, variable, condition, data):
        self.set_star(SessionGlobal.current_star)

    def set_star(self, star):
        self.clear()
        self.star = star
        ThumbnailBox.Reset()
        for filter in ["Bc", "Vc", "Rc", "Ic"]:
            if filter in star.obs_seq:
                fp = FilterPane(self, star.obs_seq[filter])
                self.v_box.pack_start(fp.get_widget(), padding=0, fill=False, expand=False)
                self.tb_list.append(fp)
        
    def get_stack_selected_viewers(self, filter):
        filter = filter_module.to_canonical[filter]
        for filter_pane in self.tb_list:
            if filter_module.to_canonical[filter_pane.color_name] == filter:
                return filter_pane.get_stack_selected_viewers()
        print("get_stack_selected_viewers: missing color: ", filter)
        return None

    def get_stack_selected_images(self, filter):
        filter = filter_module.to_canonical[filter]
        for filter_pane in self.tb_list:
            if filter_module.to_canonical[filter_pane.color_name] == filter:
                return filter_pane.get_stack_selected_images()
        print("get_stack_selected_images: missing color: ", filter)
        return None

    def get_widget(self):
        return self.scrolled_win

    def clear(self):
        x = self.v_box.get_children()
        for box in x:
            self.v_box.remove(box)
        self.tb_list = []

    def set_black_white(self, black, white):
        for box in self.tb_list:
            box.set_black_white(black, white)
        
    def set_center(self, x, y):
        for box in self.tb_list:
            box.set_center(x, y)

    # This is invoked whenever any of the child analysis checkboxes changes state
    # (It doesn't matter which one, because we query the state of *all* of them.)
    def analyze_button_cb(self, widget, data=None):
        exclusions = [] # list of image/stack juids
        for fp in self.tb_list:
            # fp is FilterPane
            for tn in fp.sub_thumbs:
                # tn is ThumbnailBox
                if not tn.analy_button.get_active():
                    exclusions.append(tn.juid)
        astro_db.SafeDirectiveChange(SessionGlobal.RecomputeAllBVRI,
                                     comp_analy.overall_summary.directive_juid,
                                     "set_image_analy_exclusions",
                                     exclusions)

################################################################
##    class FilterPane
################################################################

class FilterPane:
    def __init__(self, parent, obs_seq):
        self.parent = parent
        self.color_name = obs_seq.filter
        self.filter_frame = gtk.Frame(label = "Filter: "+self.color_name)
        self.v_box = gtk.VBox(spacing=2)
        self.filter_frame.add(self.v_box)
        self.sub_thumbs = [] # list of children thumbnail panes
        count = 0
        for image in obs_seq.exposures:
            if count % 12 == 0:
                thumb_row = gtk.HBox(spacing=2)
                self.v_box.pack_start(thumb_row, padding=0, fill=True, expand=True)
            if count == 0:
                # Build the "select all" and "select none" buttons
                little_box = gtk.VBox(spacing=2)
                thumb_row.pack_start(little_box, padding=0, fill=False, expand=False)
                sel_b = gtk.Button("Select All")
                sel_b.connect("clicked", self.select_all_cb, True)
                little_box.pack_start(sel_b, fill=False, expand=False, padding=0)
                sel_b = gtk.Button("Select None")
                sel_b.connect("clicked", self.select_all_cb, False)
                little_box.pack_start(sel_b, fill=False, expand=False, padding=0)

                # and add exposure time
                self.exposure_button = gtk.Label(obs_seq.exposure_time + ' sec')
                little_box.pack_start(self.exposure_button, fill=False, expand=False, padding=0)
                
            count = count+1
            tb = ThumbnailBox(self)
            tb.set_fitsfile(image.filename)
            image.thumbnail_box = tb
            thumb_row.pack_start(tb.get_widget(), fill=False, expand=False, padding=0)
            self.sub_thumbs.append(tb)
        self.filter_frame.show_all()

    def select_all_cb(self, widget, data=None):
        # "data" is true for select and false for unselect
        for thumb in self.sub_thumbs:
            thumb.stack_button.set_active(data)
            thumb.analy_button.set_active(data)

    def get_stack_selected_viewers(self):
        viewer_list = []
        for tb in self.sub_thumbs:
            if tb.include_in_stack():
                viewer_list.append(tb.fits_win)
        return viewer_list

    def get_stack_selected_images(self):
        image_list = ""
        for tb in self.sub_thumbs:
            if tb.include_in_stack():
                image_list += tb.file_name
                image_list += ' '
        return image_list

    def get_widget(self):
        return self.filter_frame

    def set_black_white(self, black, white):
        for tb in self.sub_thumbs:
            tb.set_black_white(black, white)
            
    def set_center(self, x, y):
        for tb in self.sub_thumbs:
            tb.set_center(x, y)
                
################################################################
##    class FitsThumbnail (subclass of FitsViewer)
################################################################
class FitsThumbnail(FITSViewer.FitsViewer):
    def __init__(self, parent):
        FITSViewer.FitsViewer.__init__(self, "Thumbs")
        self.set_size(90, 90)

################################################################
##    class ThumbnailBox
################################################################
class ThumbnailBox:
    all_fits_windows = []

    def Reset():
        for win in ThumbnailBox.all_fits_windows:
            SessionGlobal.image_region.release_slave(win)
        ThumbnailBox.all_fits_windows = []
        
    def __init__(self, parent):
        self.parent = parent # parent is a FilterPane
        self.top_box = gtk.VBox(spacing=2)
        self.f_label = gtk.Label()
        self.fits_win = FitsThumbnail(None) # no parent?
        ThumbnailBox.all_fits_windows.append(self.fits_win)
        SessionGlobal.image_region.reset_view_on_click(self.fits_win)
        self.stack_button = gtk.CheckButton(label="Stack")
        self.analy_button = gtk.CheckButton(label='Analyze')
        self.analy_button.connect("toggled", self.parent.parent.analyze_button_cb, None)

        self.top_box.pack_start(self.f_label, fill=False, expand=False, padding=0)
        self.top_box.pack_start(self.fits_win.get_widget(), fill=False, expand=False, padding=0)
        self.top_box.pack_start(self.stack_button, fill=False, expand=False, padding=0)
        self.top_box.pack_start(self.analy_button, fill=False, expand=False, padding=0)

        #self.hide()

    def get_widget(self):
        return self.top_box

    def show(self):
        self.top_box.show_all()
        self.showing=True

    def hide(self):
        self.top_box.hide()
        self.showing=False

    def set_fitsfile(self, filename):
        self.fits_win.load_file(filename)
        self.stack_button.set_active(is_active=True)
        self.analy_button.set_active(is_active=True)
        self.f_label.set_text(os.path.basename(filename))
        if self.fits_win.any_saturated_pixels():
            print("Thumbnail saturated!")
            self.f_label.modify_fg(gtk.StateFlags.NORMAL, Gdk.color_parse("red"))
        else:
            #print("Thumbnail linear.")
            self.f_label.modify_fg(gtk.StateFlags.NORMAL, Gdk.color_parse("black"))

        self.file_name = filename
        self.juid = SessionGlobal.db_obj.GetJUIDForImage(filename)

    def include_in_stack(self):
        return self.stack_button.get_active()

    def set_black_white(self, black, white):
        self.fits_win.set_black_white(black, white)

    def set_center(self, x, y):
        self.fits_win.set_center(x, y)

