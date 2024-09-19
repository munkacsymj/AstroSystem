import TextTab
import SessionGlobal
import gi
from gi.repository import Pango
import os
from gi.repository import Gtk as gtk

################################################################
##    class AnalysisTab
################################################################
class AnalysisTab:
    def __init__(self, parent_notebook, tab_label):
        self.tab_buffer = gtk.TextBuffer()
        self.tab_view = gtk.TextView()
        self.tab_view.set_buffer(self.tab_buffer)
        self.tab_view.set_editable(False)
        self.tab_view.modify_font(Pango.FontDescription("monospace"))
        self.tab_view.set_cursor_visible(False)
        self.tab_view.set_wrap_mode(gtk.WrapMode.CHAR)
        self.tab_sw = gtk.ScrolledWindow()
        self.tab_sw.add(self.tab_view)
        self.whole_box = gtk.VBox(spacing = 2)
        self.selected_analysis = None #holds single letter identifing
        #current filter

        self.analysis_top = gtk.HBox(spacing = 2)
        a_button = gtk.RadioButton(group = None, label = "B")
        a_button.connect("clicked", self.a_button_cb, "B")
        self.analysis_top.pack_start(a_button, padding=0, fill=False, expand=False)
        a_button = gtk.RadioButton(group = a_button, label = "V")
        a_button.connect("clicked", self.a_button_cb, "V")
        self.analysis_top.pack_start(a_button, padding=0, fill=False, expand=False)
        a_button = gtk.RadioButton(group = a_button, label = "R")
        a_button.connect("clicked", self.a_button_cb, "R")
        self.analysis_top.pack_start(a_button, padding=0, fill=False, expand=False)
        a_button = gtk.RadioButton(group = a_button, label = "I")
        a_button.connect("clicked", self.a_button_cb, "I")
        self.analysis_top.pack_start(a_button, padding=0, fill=False, expand=False)
        self.whole_box.pack_start(self.analysis_top, padding=0, fill=False, expand = False)

        self.whole_box.pack_start(self.tab_sw, padding=0, fill=True, expand=True)
        parent_notebook.append_page(self.whole_box, gtk.Label(tab_label))
        self.tab_view.show()
        self.tab_sw.show()

    def a_button_cb(self, widget, data = None):
        global current_star
        self.selected_analysis = data
        self.set_star(current_star.name)

    def set_star(self, star_name):
        if self.selected_analysis != None:
            filename = os.path.join(SessionGlobal.homedir, star_name + self.selected_analysis + "c.phot")
            exists = os.path.isfile(filename)
            #print "filename ", filename, " exists = ", exists
            if not exists:
                TextTab.SetFromFile(self.tab_buffer, "/dev/null")
            else:
                TextTab.SetFromFile(self.tab_buffer, filename)
        else:
            TextTab.SetFromFile(self.tab_buffer, "/dev/null")
        self.tab_view.show()
        self.tab_sw.show()
            
