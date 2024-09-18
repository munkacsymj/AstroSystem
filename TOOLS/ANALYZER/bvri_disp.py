#!/usr/bin/python3

import sys
import gi
gi.require_version('Gtk', '3.0')
from gi.repository import Gtk as gtk
from gi.repository import Pango

sys.path.insert(1, '/home/mark/ASTRO/CURRENT')
from PYTHON_LIB.IMAGE_LIB import star
from PYTHON_LIB.ASTRO_DB_LIB import astro_db

def main():
    gtk.main()

def program_shutdown(widget):
    exit()

root_dir = '/home/IMAGES/5-30-2023'

class BVRITab:
    def __init__(self, parent_notebook):
        global root_dir
        self.tab_buffer = gtk.TextBuffer()
        self.tab_view = gtk.TextView()
        self.tab_view.set_buffer(self.tab_buffer)
        self.tab_view.set_editable(False)
        self.tab_view.modify_font(Pango.FontDescription("monospace"))
        self.tab_view.set_cursor_visible(False)
        self.tab_view.set_wrap_mode(gtk.WrapMode.CHAR)
        self.tab_sw = gtk.ScrolledWindow()
        self.tab_sw.add(self.tab_view)

        parent_notebook.append_page(self.tab_sw, gtk.Label(label="BVRI"))
        self.tab_view.show()
        self.tab_sw.show()

        self.db_obj = astro_db.AstroDB(root_dir)
        self.db = self.db_obj.GetData()
        self.target = None
        self.catalog = None

    def SetTarget(self, target):
        self.catalog = star.ReadCatalog(target)
        tgt_set = [x for x in self.db['sets'] if x['stype'] == 'TARGET' and x['target'] == target]
        if len(tgt_set) != 1:
            if len(tgt_set) == 0:
                print("DoBVRI: target ", target, " not found.")
                sys.exit(-2)
            print("DoBVRI: target ", target, " had multiple matches?????")
            sys.exit(-2)
        # The set of 'input' to the target will be the BVRI set
        bvri_set = tgt_set[0]['input']
        if len(bvri_set) != 1:
            if len(bvri_set) == 0:
                print("SetTarget: bvri_set for ", target, " not found.")
                sys.exit(-2)
            print("SetTarget: bvri_set for ", target, " had multiple matches???")
            sys.exit(-2)
        self.analysis = next((x for x in self.db['analyses'] if x['source'] == bvri_set[0]), None)
        print("bvri_set = ", bvri_set)
        #print("analysis = ", self.analysis)
        print(next(iter(self.analysis)))
        print([x for x in self.analysis])

        all_disp_stars = {}
        star.add_analysis(self.analysis, all_disp_stars, self.catalog)

        #self.buf_lines = [BVRILine(x) for x in self.analysis['results']]

        buf_string = ''
        #for x in self.buf_lines:
        #    buf_string += x.TextLine()

        for x in all_disp_stars.values():
            buf_string += x.ToDispString() + '\n'

        self.tab_buffer.set_text(buf_string)
        self.tab_sw.show_all()

class BVRILine:
    def __init__(self, result_struct):
        self.name = result_struct['name']
        self.profile_num = result_struct['profile']

    def TextLine(self):
        return self.name + "  " + str(self.profile_num) + '\n'
        

if __name__ == "__main__":
    win = gtk.Window()
    win.connect("destroy", gtk.main_quit)

    notebook = gtk.Notebook()
    win.add(notebook)

    bvri_tab = BVRITab(notebook)
    bvri_tab.SetTarget('rt-lyr')

    win.show_all()

    main()
    
