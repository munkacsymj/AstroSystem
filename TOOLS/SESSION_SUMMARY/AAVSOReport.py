import re
import gtk
import gi
import os
from gi.repository import Pango
from gi.repository import Gtk as gtk
import SessionGlobal

##
## An AAVSOReport is built from a BVRI_DB, which
## itself is made up of a sequence of BVRIRecords.
##


################################################################
## Class BVRIRecord
################################################################

class BVRIRecord:
    def __init__(self, db, linelist):
        self.db = db
        self.raw_elements = linelist
        self.target = self.find_element("TARGET")
        self.starname = self.find_element("STARNAME")
        self.filter = self.find_element("FILTER")

        if self.target == None or self.starname == None or self.filter == None:
            print("Invalid BVRI Record:")
            for ll in linelist:
                print(ll)
            return

        status_value = self.find_element("STATUS")
        if status_value == None:
            self.status = 0
        else:
            self.status = int(status_value)
        self.display_string = self.to_display()

    def to_display(self):
        # STARNAME(target_star) Filter, TRMAG, MAGERR, airmass=x.xxx
        magnitude = self.find_element("TRMAG")
        if magnitude == None:
            magnitude = self.find_element("RAWMAG")
        if magnitude == None:
            magnitude = ''
        mag_err = self.find_element("MAGERR")
        if mag_err == None:
            mag_err = ''
        airmass = self.find_element("AIRMASS")
        if airmass == None:
            airmass = ''

        return (self.starname + '(' + self.target + ') ' +
                self.filter + ', ' + magnitude + ", " +
                mag_err + ", " + airmass)
    
    def split_into_words(self, element):
        pattern = '\\[(.*)\\]\\[(.*)\\]\\[(.*)\\]'
        p = re.compile(pattern)
        r = p.match(element)
        if r == None:
            print("BVRIRecord: invalid element: ", element)
            return (None, None, None)
        else:
            return r.group(1, 2, 3)

    def find_element(self, keyword):
        for s in self.raw_elements:
            if '['+keyword+']' in s:
                (keyword,typecode,value) = self.split_into_words(s)
                return value
        return None

    def toggle_exclusion(self, exclude):
        if exclude:
            self.status = 1
        else:
            self.status = 0

        command = "update_bvri_db -i " + self.db.filename + " -n " + self.target + " -s " + self.starname + " -f " + self.filter + '[STATUS][I][' + str(self.status) + ']'
        print("Executing: ", command)
        os.system(command)
        self.db.refresh()

################################################################
##    Class AAVSOReport
################################################################
class AAVSOReport:
    def __init__(self, parent_notebook, tab_label):
        self.filename = os.path.join(SessionGlobal.homedir, "bvri.db")

        self.grid = gtk.Grid()
        self.grid.set_column_homogeneous(True)

        self.tab_sw = gtk.ScrolledWindow()
        self.tab_sw.add(self.grid)
        self.whole_box = gtk.VBox(spacing = 2)
        self.whole_box.pack_start(self.tab_sw, padding=0, fill=True, expand=True)
        parent_notebook.append_page(self.whole_box, gtk.Label(tab_label))

        self.parse_file()

    def parse_file(self):
        if os.path.isfile(self.filename):
            self.whole_db = BVRI_DB(self.filename)
            current_row = 0
            for one_record in self.whole_db.get_records():
                show_string = one_record.to_display()
                exclude_button = gtk.CheckButton(label="Exclude")
                exclude_button.connect("toggled", AAVSOReport.exclude_cb, one_record)
                exclude_button.set_active(one_record.status)
                string_label = gtk.Label(show_string)
                self.grid.attach(exclude_button, 0, current_row, 1, 1)
                self.grid.attach(string_label, 1, current_row, 1, 1)
                current_row += 1
        else:
            # Set entire tab to something empty
            self.grid.foreach(self.grid.remove)

    def exclude_cb(self, widget, data):
        print("exclude_cb() invoked.")
        data.toggle_exclusion(widget.get_active())

################################################################
##    Class BVRI_DB
################################################################            
class BVRI_DB:
    def __init__(self, filename):
        self.filename = filename
        self.refresh()

    def refresh(self):
        f = open(self.filename, 'r')
        self.data = [] # holds all the elements

        ## read everything (file is never particularly long)
        current_record = []
        reject_record = False
        
        for one_line in f:
            if '[' not in one_line:
                continue
            
            if '[ERRORS]' in one_line:
                reject_record = True
                
            if '[RECORD]' in one_line:
                if reject_record == False and len(current_record) > 2:
                    self.data.append(BVRIRecord(self, current_record))
                current_record = []
                reject_record = False
            else:
                current_record.append(one_line)
        if reject_record == False and len(current_record) > 2:
            self.data.append(BVRIRecord(self, current_record))
        
        f.close()

    def get_records(self):
        return self.data
        
