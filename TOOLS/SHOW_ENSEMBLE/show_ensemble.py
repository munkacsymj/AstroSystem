#!/usr/bin/python3
import sys
import gi
gi.require_version('Gtk', '3.0')
from gi.repository import Gtk as gtk

import os

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
from PYTHON_LIB.ASTRO_DB_LIB import astro_db
from PYTHON_LIB.IMAGE_LIB import star as star_mod

catalog = {}
all_seq_stars = []
all_comp_stars = []
all_ens_stars = []
all_check_stars = []
all_ref_stars = []

class FileChooserWindow:
    def __init__(self, parent):
        dialog = gtk.FileChooserDialog(
            title="Please choose a catalog file", parent=parent, action=gtk.FileChooserAction.OPEN
        )
        dialog.add_buttons(
            gtk.STOCK_CANCEL,
            gtk.ResponseType.CANCEL,
            gtk.STOCK_OPEN,
            gtk.ResponseType.OK,
        )
        dialog.set_current_folder('/home/ASTRO/CATALOGS')

        #self.add_filters(dialog)

        response = dialog.run()
        if response == gtk.ResponseType.OK:
            print("Open clicked")
            print("File selected: " + dialog.get_filename())
            self.filename = dialog.get_filename()
        elif response == gtk.ResponseType.CANCEL:
            print("Cancel clicked")
            self.filename = None

        dialog.destroy()

    def add_filters(self, dialog):
        filter_text = gtk.FileFilter()
        filter_text.set_name("Catalog files")
        #filter_text.add_pattern("+([^.~])")
        filter_text.add_pattern("*([a-z])")
        dialog.add_filter(filter_text)

        filter_any = gtk.FileFilter()
        filter_any.set_name("Any files")
        filter_any.add_pattern("*")
        dialog.add_filter(filter_any)

class SummaryBox(gtk.Frame):
    def __init__(self):
        super().__init__()
        self.grid = None
        self.Refresh()

    def Refresh(self):
        global catalog, all_seq_stars, all_comp_stars, all_ens_stars, all_check_stars, all_ref_stars
        if self.grid is not None:
            self.grid.destroy()
        self.grid = gtk.Grid()
        self.add(self.grid)

        #Setup row 0 of the grid (column labels)
        #l0 = gtk.Label(label='starname')
        #self.grid.attach(l0, 0, 1, 1, 1)
        l0 = gtk.Label(label='Photometry Avail')
        l0.set_alignment(0.5, 0.5)
        self.grid.attach(l0, 1, 0, 4, 1)
        l0 = gtk.Label(label='B')
        l0.set_width_chars(6)
        self.grid.attach(l0, 1, 1, 1, 1)
        l0 = gtk.Label(label='V')
        l0.set_width_chars(6)
        self.grid.attach(l0, 2, 1, 1, 1)
        l0 = gtk.Label(label='R')
        l0.set_width_chars(6)
        self.grid.attach(l0, 3, 1, 1, 1)
        l0 = gtk.Label(label='I')
        l0.set_width_chars(6)
        self.grid.attach(l0, 4, 1, 1, 1)
        l0 = gtk.Label(label="Comp")
        l0.set_width_chars(6)
        self.grid.attach(l0, 5, 1, 1, 1)
        l0 = gtk.Label(label="Ens")
        l0.set_width_chars(6)
        self.grid.attach(l0, 6, 1, 1, 1)
        l0 = gtk.Label(label='Check')
        l0.set_width_chars(6)
        self.grid.attach(l0, 7, 1, 1, 1)
        l0 = gtk.Label(label='Check\nRef')
        l0.set_width_chars(6)
        self.grid.attach(l0, 8, 0, 1, 2)
        
        row = 2
        for starname in all_seq_stars:
            cat = catalog[starname]
            l1 = gtk.Label(label=starname)
            l1.set_width_chars(20)
            self.grid.attach(l1, 0, row, 1, 1)

            b0 = gtk.CheckButton()
            b0.set_active('B' in cat.ref_mag)
            self.grid.attach(b0, 1, row, 1, 1)

            b0 = gtk.CheckButton()
            b0.set_active('V' in cat.ref_mag)
            self.grid.attach(b0, 2, row, 1, 1)

            b0 = gtk.CheckButton()
            b0.set_active('R' in cat.ref_mag)
            self.grid.attach(b0, 3, row, 1, 1)

            b0 = gtk.CheckButton()
            b0.set_active('I' in cat.ref_mag)
            self.grid.attach(b0, 4, row, 1, 1)

            b0 = gtk.CheckButton()
            b0.set_active(starname in all_comp_stars)
            self.grid.attach(b0, 5, row, 1, 1)

            b0 = gtk.CheckButton()
            b0.set_active(starname in all_ens_stars)
            self.grid.attach(b0, 6, row, 1, 1)

            b0 = gtk.CheckButton()
            b0.set_active(starname in all_check_stars)
            self.grid.attach(b0, 7, row, 1, 1)

            b0 = gtk.CheckButton()
            b0.set_active(starname in all_ref_stars)
            self.grid.attach(b0, 8, row, 1, 1)

            row += 1

        self.show_all()
        print("SummaryBox finished Refresh().")

class AllCheckBox(gtk.Frame):
    def __init__(self):
        super().__init__()
        self.set_label("Ensemble")
        self.grid = None
        self.Refresh()

    def Refresh(self):
        global catalog, all_seq_stars, all_comp_stars, all_ens_stars, all_check_stars, all_ref_stars
        if self.grid is not None:
            self.grid.destroy()
        self.grid = gtk.Grid()
        self.add(self.grid)

        #Setup row 0 of the grid (column labels)

        row = 0
        l0 = gtk.Label(label='Ensemble:')
        self.grid.attach(l0, 0, 0, 2, 1)
        l1 = gtk.Label(label='B')
        l1.set_width_chars(6)
        self.grid.attach(l1, 2, 0, 1, 1)
        l1 = gtk.Label(label='V')
        l1.set_width_chars(6)
        self.grid.attach(l1, 3, 0, 1, 1)
        l1 = gtk.Label(label='R')
        l1.set_width_chars(6)
        self.grid.attach(l1, 4, 0, 1, 1)
        l1 = gtk.Label(label='I')
        l1.set_width_chars(6)
        self.grid.attach(l1, 5, 0, 1, 1)

        row += 1
        for starname in all_ens_stars:
            cat = catalog[starname]
            l1 = gtk.Label(label=starname)
            self.grid.attach(l1, 0, row, 1, 1)

            b0 = gtk.CheckButton()
            b0.set_active(cat.IsEnsemble('B'))
            self.grid.attach(b0, 2, row, 1, 1)

            b0 = gtk.CheckButton()
            b0.set_active(cat.IsEnsemble('V'))
            self.grid.attach(b0, 3, row, 1, 1)

            b0 = gtk.CheckButton()
            b0.set_active(cat.IsEnsemble('R'))
            self.grid.attach(b0, 4, row, 1, 1)

            b0 = gtk.CheckButton()
            b0.set_active(cat.IsEnsemble('I'))
            self.grid.attach(b0, 5, row, 1, 1)

            row += 1

        l0 = gtk.Label(label='Ref Check:')
        self.grid.attach(l0, 0, row, 2, 1)

        row += 1
        for starname in all_ref_stars:
            cat = catalog[starname]
            l1 = gtk.Label(label=starname)
            l1.set_width_chars(20)
            self.grid.attach(l1, 0, row, 2, 1)

            b0 = gtk.CheckButton()
            b0.set_active(cat.is_ref)
            self.grid.attach(b0, 2, row, 1, 1)

            b0 = gtk.CheckButton()
            b0.set_active(cat.is_ref)
            self.grid.attach(b0, 3, row, 1, 1)

            b0 = gtk.CheckButton()
            b0.set_active(cat.is_ref)
            self.grid.attach(b0, 4, row, 1, 1)

            b0 = gtk.CheckButton()
            b0.set_active(cat.is_ref)
            self.grid.attach(b0, 5, row, 1, 1)

            row += 1

        l0 = gtk.Label(label='Check Stars:')
        self.grid.attach(l0, 0, row, 2, 1)
        row += 1
        for starname in all_check_stars:
            cat = catalog[starname]
            l1 = gtk.Label(label=starname)
            self.grid.attach(l1, 0, row, 1, 1)

            b0 = gtk.CheckButton()
            b0.set_active(cat.IsCheck('B'))
            self.grid.attach(b0, 2, row, 1, 1)

            b0 = gtk.CheckButton()
            b0.set_active(cat.IsCheck('V'))
            self.grid.attach(b0, 3, row, 1, 1)

            b0 = gtk.CheckButton()
            b0.set_active(cat.IsCheck('R'))
            self.grid.attach(b0, 4, row, 1, 1)

            b0 = gtk.CheckButton()
            b0.set_active(cat.IsCheck('I'))
            self.grid.attach(b0, 5, row, 1, 1)

            row += 1

        self.show_all()
        print("AllCheckBox finished Refresh().")


class CompBox(gtk.Frame):
    def __init__(self):
        super().__init__()
        self.set_label("Single Comp")
        self.grid = None
        self.Refresh()

    def Refresh(self):
        global catalog, all_seq_stars, all_comp_stars, all_ens_stars, all_check_stars, all_ref_stars
        if self.grid is not None:
            self.grid.destroy()
        self.grid = gtk.Grid()
        self.add(self.grid)

        #Setup row 0 of the grid (column labels)

        row = 0
        l0 = gtk.Label(label='Comp:')
        self.grid.attach(l0, 0, 0, 2, 1)
        l1 = gtk.Label(label=str(all_comp_stars))
        self.grid.attach(l1, 2, 0, 4, 1)

        row = 1
        l0 = gtk.Label(label='Ref Check:')
        self.grid.attach(l0, 0, 1, 2, 1)
        l1 = gtk.Label(label='B')
        self.grid.attach(l1, 2, 1, 1, 1)
        l1 = gtk.Label(label='V')
        self.grid.attach(l1, 3, 1, 1, 1)
        l1 = gtk.Label(label='R')
        self.grid.attach(l1, 4, 1, 1, 1)
        l1 = gtk.Label(label='I')
        self.grid.attach(l1, 5, 1, 1, 1)

        row = 2
        for starname in all_ref_stars:
            cat = catalog[starname]
            l1 = gtk.Label(label=starname)
            l1.set_width_chars(20)
            self.grid.attach(l1, 0, row, 2, 1)

            b0 = gtk.CheckButton()
            b0.set_active(cat.is_ref)
            self.grid.attach(b0, 2, row, 1, 1)

            b0 = gtk.CheckButton()
            b0.set_active(cat.is_ref)
            self.grid.attach(b0, 3, row, 1, 1)

            b0 = gtk.CheckButton()
            b0.set_active(cat.is_ref)
            self.grid.attach(b0, 4, row, 1, 1)

            b0 = gtk.CheckButton()
            b0.set_active(cat.is_ref)
            self.grid.attach(b0, 5, row, 1, 1)

            row += 1

        l0 = gtk.Label(label='Check Stars:')
        self.grid.attach(l0, 0, row, 2, 1)
        row += 1
        for starname in all_check_stars:
            cat = catalog[starname]
            l1 = gtk.Label(label=starname)
            self.grid.attach(l1, 0, row, 1, 1)

            b0 = gtk.CheckButton()
            b0.set_active(cat.IsCheck('B'))
            self.grid.attach(b0, 2, row, 1, 1)

            b0 = gtk.CheckButton()
            b0.set_active(cat.IsCheck('V'))
            self.grid.attach(b0, 3, row, 1, 1)

            b0 = gtk.CheckButton()
            b0.set_active(cat.IsCheck('R'))
            self.grid.attach(b0, 4, row, 1, 1)

            b0 = gtk.CheckButton()
            b0.set_active(cat.IsCheck('I'))
            self.grid.attach(b0, 5, row, 1, 1)

            row += 1

        self.show_all()
        print("CompBox finished Refresh().")

summary_box = None
comp_box = None
check_box = None

catalog_name = None

def SetupCatChooser(widget, parent_win):
    fc = FileChooserWindow(parent_win)
    if fc is not None:
        global catalog, all_seq_stars, all_comp_stars, all_ens_stars, all_check_stars, all_ref_stars
        global summary_box, comp_box, check_box
        global catalog_name
        catalog_name = os.path.basename(fc.filename)

        ReLoad(widget, None)

def ReLoad(widget, unused):
    global catalog, all_seq_stars, all_comp_stars, all_ens_stars, all_check_stars, all_ref_stars
    catalog_list = star_mod.ReadCatalog(catalog_name)
    catalog = dict([(dict.name, dict) for dict in catalog_list])
    all_seq_stars = [x.name for x in catalog_list if len(x.ref_mag) > 0]
    all_comp_stars = [x.name for x in catalog_list if x.is_comp_candidate]
    all_ens_stars = [x.name for x in catalog_list if x.is_ensemble_all_filters or
                     len(x.ensemble_filters) > 0]
    all_check_stars = [x.name for x in catalog_list if x.is_check_all_filters or
                       len(x.check_filters) > 0]
    all_ref_stars = [x.name for x in catalog_list if x.is_ref]
        
    summary_box.Refresh()
    comp_box.Refresh()
    check_box.Refresh()

def main():
    global summary_box, comp_box, check_box
    win = gtk.Window()
    win.connect("destroy", gtk.main_quit)

    new_file_button = gtk.Button.new_with_label("New Catalog")
    new_file_button.connect("clicked", SetupCatChooser, win)
    hbox = gtk.HBox()
    hbox.pack_start(new_file_button, fill=False, expand=False, padding=2)
    reload_button = gtk.Button.new_with_label("Reload")
    reload_button.connect("clicked", ReLoad, None)
    hbox.pack_start(reload_button, fill=False, expand=False, padding=2)
    
    vbox = gtk.VBox()
    win.add(vbox)
    vbox.pack_start(hbox, fill=False, expand=False, padding=2)

    hbox = gtk.HBox()
    vbox.pack_start(hbox, fill=True, expand=True, padding=2)
    summary_box = SummaryBox()
    hbox.pack_start(summary_box, fill=True, expand=True, padding=2)

    comp_box = CompBox()
    vbox = gtk.VBox()
    hbox.pack_start(vbox, fill=True, expand=True, padding=2)
    vbox.pack_start(comp_box, fill=True, expand=True, padding=2)
    check_box = AllCheckBox()
    vbox.pack_start(check_box, fill=True, expand=True, padding=2)

    win.show_all()
    gtk.main()

if __name__ == "__main__":
    main()
    
