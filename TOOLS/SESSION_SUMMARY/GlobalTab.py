import TextTab
import gi
from gi.repository import Pango
from gi.repository import Gtk as gtk
import os

class GlobalTab:
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
        self.button_refresh = gtk.Button(label="Refresh")
        self.button_refresh.connect("clicked", self.refresh_cb, None)
        self.whole_box.pack_start(self.button_refresh, padding=0, fill=False, expand=False)
        self.whole_box.pack_start(self.tab_sw, padding=0, fill=True, expand=True)
        parent_notebook.append_page(self.whole_box, gtk.Label(tab_label))
        self.refresh()

    def refresh_cb(self, widget, data = None):
        self.refresh()

    def refresh(self):
        self.filename = "/tmp/global_summary.txt"
        command = "summarize_sessions"
        command += " -o "
        command += self.filename
        os.system(command)
        TextTab.SetFromFile(self.tab_buffer, self.filename)
        self.tab_view.show()
        self.tab_sw.show()

class GlobalSummary:
    def __init__(self):
        self.filename = "/tmp/global_summary.txt"
        os.system("touch " + self.filename)

    def set_tab(self, tab_ref):
        self.tab = tab_ref

    def refresh(self):
        command = "summarize_sessions"
        command += " -o "
        command += self.filename
        os.system(command)
        TextTab.SetFromFile(self.tab.tab_buffer, self.filename)
        
