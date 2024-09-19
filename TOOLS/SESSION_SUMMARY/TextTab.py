import re
import gtk
import gi
import os
from gi.repository import Pango
from gi.repository import Gtk as gtk
import SessionGlobal

################################################################
##    SetFromFile()
##        This is a helper function used in many text tabs
################################################################

def SetFromFile(text_buffer, filename, lookup_dictionary=None):
    f = open(filename, "r")
    str = f.read()
    text_buffer.set_text(str)

    if lookup_dictionary != None:
        # scan the text file, looking for that image name to appear. 
        # Now scan the text buffer
        all_images = re.finditer("image\d\d\d\.fits", str)
        for image_match in all_images:
            image_name = image_match.group()
            lookup_dictionary[image_name] = image_match.start()
    f.close()
    #print "Loaded ", text_buffer.get_line_count(), "lines with ", text_buffer.get_char_count(), "chars."

################################################################
##    class TextTabFromFile
##        This class is used for the shell file, the logfile,
##        and (maybe?) for the Analysis tab and (maybe?) for the BVRI
##        tab as well
################################################################
class TextTabFromFile:
    def __init__(self, parent_notebook, tab_label, filename, goto_button=False):
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
        if goto_button:
            self.button_goto = gtk.Button(label="Jump To Selected Image")
            self.button_goto.connect("clicked", self.do_jump_cb, None)
            self.whole_box.pack_start(self.button_goto, padding=0, fill=False, expand=False)
        self.whole_box.pack_start(self.tab_sw, padding=0, fill=True, expand=True)
        parent_notebook.append_page(self.whole_box, gtk.Label(tab_label))
        if filename != None:
            if goto_button:
                self.lookup = {}
                SetFromFile(self.tab_buffer, filename, lookup_dictionary=self.lookup)
            else:
                SetFromFile(self.tab_buffer, filename)
        self.tab_view.show()
        self.tab_sw.show()

    def do_jump_cb(self, widget, data=None):
        #global current_image_name
        target_loc = self.lookup[os.path.basename(SessionGlobal.current_image_name)]
        if target_loc == None:
            return
        iter = self.tab_buffer.get_iter_at_offset(target_loc)
        self.tab_view.scroll_to_iter(iter, 0.0, False, 0.5, 0.5)

