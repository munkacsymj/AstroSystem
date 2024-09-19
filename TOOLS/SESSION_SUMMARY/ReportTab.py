import os
import SessionGlobal
from gi.repository import Gtk as gtk
from gi.repository import Pango
import report_db

################################################################
##    Class AAVSOReport
################################################################
class AAVSOReport:
    def __init__(self, parent_notebook, tab_label):
        self.filename = os.path.join(SessionGlobal.homedir, "aavso_report.db")

        self.tab_buffer = gtk.TextBuffer()
        self.tab_view = gtk.TextView()
        self.tab_view.set_buffer(self.tab_buffer)
        self.tab_view.set_editable(True)

        self.deleted_tag = self.tab_buffer.create_tag("deleted",background="gray")
        self.modified_tag = self.tab_buffer.create_tag("bold",weight=Pango.Weight.BOLD)
        self.conflict_tag = self.tab_buffer.create_tag("conflicted",foreground="red")

        self.tab_view.set_left_margin(5)
        self.tab_view.set_right_margin(5)
        self.tab_view.set_top_margin(5)
        self.tab_view.set_bottom_margin(5)
        
        self.tab_view.modify_font(Pango.FontDescription("mono 9"))
        self.tab_view.set_cursor_visible(True)
        self.tab_view.set_wrap_mode(gtk.WrapMode.NONE)
        self.tab_sw = gtk.ScrolledWindow()
        self.tab_sw.add(self.tab_view)
        self.whole_box = gtk.VBox(spacing = 2)

        self.button_area = gtk.HBox(spacing = 2)

        self.save_button = gtk.Button("Save")
        self.save_button.connect("clicked", self.execute_save)
        self.button_area.pack_start(self.save_button, fill=False, expand=False, padding=0)

        self.delete_entry_button = gtk.Button("Delete/Undelete Line")
        self.delete_entry_button.connect("clicked", self.execute_delete)
        self.button_area.pack_start(self.delete_entry_button, fill=False, expand=False, padding=0)

        self.conflict_label = gtk.Label("Conflict")
        self.button_area.pack_start(self.conflict_label, fill=False, expand=False, padding=0)

        self.whole_box.pack_start(self.button_area, fill=False, expand=False, padding=0)
        self.whole_box.pack_start(self.tab_sw, fill=True, expand=True, padding=0)
        parent_notebook.append_page(self.whole_box, gtk.Label(tab_label))

        self.report_db = report_db.ReportDB(self.filename)
        self.update_tab()

        #self.tab_view.connect("key-press-event", self.on_key_press_event)

        self.tab_view.show()
        self.tab_sw.show()

    def execute_delete(self, widget, event=None):
        print("execute_delete() invoked.")
        current_cursor_position = self.tab_buffer.get_iter_at_mark(self.tab_buffer.get_insert())
        offset = current_cursor_position.get_line_offset()
        line_start = current_cursor_position.copy()
        line_start.backward_chars(offset)
        line_end   = current_cursor_position.copy()
        line_end.forward_to_line_end()

        command = "/home/mark/ASTRO/CURRENT/TOOLS/BVRI/insert_report_lines.py -d "
        command += ('-i ' + os.path.join(SessionGlobal.homedir, "aavso_report.db"))
        command += (' -L "' + self.tab_buffer.get_text(line_start, line_end, True) + '"')
        print("Executing command:")
        print(command)
        os.system(command)
        self.update_tab()

    def execute_save(self, widget, event=None):
        print("execute_save() invoked.")

    def on_key_press_event(self, widget, event):
        print("KPE: ", event)
        
    def set_conflict_label(self, conflict_present):
        if conflict_present:
            self.conflict_label.set_markup('<span background="red">Conflict Present</span>')
        else:
            self.conflict_label.set_markup('<span background="green" foreground="white">No Conflicts</span>')

    def update_tab(self):
        orig_cursor_position = self.tab_buffer.get_iter_at_mark(self.tab_buffer.get_insert()).get_offset()
        print("orig_cursor_position = ", orig_cursor_position)
        full_text = ""
        deleted_line_list = []
        conflict_line_list = []
        changed_line_list = []
        line_number = 0
        self.report_db.refresh_from_file()
        for line in self.report_db.get_all_lines_and_annotations():
            flags, text = line
            if 'c' in flags:
                conflict_line_list.append(line_number)
            if 'm' in flags:
                changed_line_list.append(line_number)
            if 'd' in flags:
                deleted_line_list.append(line_number)
                
            # flatten into a string
            full_text = full_text + text + '\n'
            line_number += 1
        self.set_conflict_label(len(conflict_line_list) > 0)
        self.tab_buffer.set_text(full_text)
        self.set_tags(deleted_line_list, self.deleted_tag)
        self.set_tags(changed_line_list, self.modified_tag)
        self.set_tags(conflict_line_list, self.conflict_tag)
        cursor_iter = self.tab_buffer.get_iter_at_offset(orig_cursor_position)
        print("Setting cursor to iter offset ", cursor_iter.get_offset())
        self.tab_buffer.place_cursor(cursor_iter)
        self.tab_view.show()
        print("Cursor_visible = ", self.tab_view.get_cursor_visible())

    def set_tags(self, line_list, tag):
        start_iter = self.tab_buffer.get_start_iter()
        end_iter = self.tab_buffer.get_end_iter()
        for line in line_list:
            start_iter.set_line(line)
            end_iter.set_line(line+1)
            self.tab_buffer.apply_tag(tag, start_iter, end_iter)
        

