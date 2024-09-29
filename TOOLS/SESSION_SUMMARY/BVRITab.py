import julian
import time
import datetime
import webbrowser
import SessionGlobal
import gi
from gi.repository import Pango
from gi.repository import Gtk as gtk
from gi.repository import Gdk as gdk
import os
import sys
import TextTab
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
##        class BVRIProgress
################################################################
class ColorButton:
    def on_draw(self, widget, cr, data):
        color_name = None
        if self.state == 0:
            cr.set_source_rgb(0.5, 0.5, 0.5)
        elif self.state == 1:
            cr.set_source_rgb(1.0, 1.0, 0.0)
        else:
            cr.set_source_rgb(0.0, 1.0, 0.0)
            
        width = widget.get_allocated_width()
        height = widget.get_allocated_height()
        context = widget.get_style_context()
        #gtk.render_background(context, cr, 0, 0, width, height)
        cr.rectangle(0, 0, width, height)
        cr.fill()
        
    def __init__(self):
        self.button = gtk.Button()
        self.da = gtk.DrawingArea()
        self.da.set_size_request(24, 24)
        self.button.add(self.da)
        self.SetState(0) # not started
        self.da.connect("draw", self.on_draw, None)
        self.button.show()

    def SetState(self, state):
        self.state = state
        self.da.queue_draw()
        #gtk.main_iteration()
        while gtk.events_pending():
            gtk.main_iteration()
            if not gtk.events_pending():
                time.sleep(0.1)
        
class BVRIProgress:
    def __init__(self):
        self._popup = gtk.Window()
        self._popup.set_position(gtk.WindowPosition.CENTER)
        self._popup.set_keep_above(True)
        self._grid = gtk.Grid()
        self.b_column = gtk.Label(label='B')
        self.v_column = gtk.Label(label='V')
        self.r_column = gtk.Label(label='R')
        self.i_column = gtk.Label(label='I')

        self.b_find = ColorButton()
        self.v_find = ColorButton()
        self.r_find = ColorButton()
        self.i_find = ColorButton()
        self.b_match = ColorButton()
        self.v_match = ColorButton()
        self.r_match = ColorButton()
        self.i_match = ColorButton()
        self.b_phot = ColorButton()
        self.v_phot = ColorButton()
        self.r_phot = ColorButton()
        self.i_phot = ColorButton()
        self._find = gtk.Label(label='Find Stars')
        self._match = gtk.Label(label='Star Match')
        self._phot = gtk.Label(label='Photometry')
        self._analyze = gtk.Label(label='Analyze')
        self.bvri_analyze = ColorButton()
        self._pretty = gtk.Label(label='BVRI_Pretty')
        self.bvri_pretty = ColorButton()
        self._report = gtk.Label(label='Report')
        self.bvri_report = ColorButton()

        self._grid.attach(self.b_column, 1, 0, 1, 1)
        self._grid.attach(self.v_column, 2, 0, 1, 1)
        self._grid.attach(self.r_column, 3, 0, 1, 1)
        self._grid.attach(self.i_column, 4, 0, 1, 1)

        self._grid.attach(self._find, 0, 1, 1, 1)
        self._grid.attach(self._match, 0, 2, 1, 1)
        self._grid.attach(self._phot, 0, 3, 1, 1)
        self._grid.attach(self._analyze, 0, 4, 1, 1)
        self._grid.attach(self._pretty, 0, 5, 1, 1)
        self._grid.attach(self._report, 0, 6, 1, 1)

        self._grid.attach(self.b_find.button, 1, 1, 1, 1)
        self._grid.attach(self.v_find.button, 2, 1, 1, 1)
        self._grid.attach(self.r_find.button, 3, 1, 1, 1)
        self._grid.attach(self.i_find.button, 4, 1, 1, 1)
        self._grid.attach(self.b_match.button, 1, 2, 1, 1)
        self._grid.attach(self.v_match.button, 2, 2, 1, 1)
        self._grid.attach(self.r_match.button, 3, 2, 1, 1)
        self._grid.attach(self.i_match.button, 4, 2, 1, 1)
        self._grid.attach(self.b_phot.button, 1, 3, 1, 1)
        self._grid.attach(self.v_phot.button, 2, 3, 1, 1)
        self._grid.attach(self.r_phot.button, 3, 3, 1, 1)
        self._grid.attach(self.i_phot.button, 4, 3, 1, 1)

        self._grid.attach(self.bvri_analyze.button, 1, 4, 4, 1)
        self._grid.attach(self.bvri_pretty.button, 1, 6, 4, 1)
        self._grid.attach(self.bvri_report.button, 1, 5, 4, 1)

        self._popup.add(self._grid)
        self._popup.show_all()
        print("Prior to entry, events_pending = ", gtk.events_pending())
        while gtk.events_pending():
            gtk.main_iteration()
            if not gtk.events_pending():
                time.sleep(0.1)
        print("BVRIProgress class init completed.")

    def Finished(self):
        self._popup.destroy()

    def ShowProgress(self, task, state):
        if task == "find_b":
            self.b_find.SetState(state)
        elif task == "find_v":
            self.v_find.SetState(state)
        elif task == "find_r":
            self.r_find.SetState(state)
        elif task == "find_i":
            self.i_find.SetState(state)
        elif task == "match_b":
            self.b_match.SetState(state)
        elif task == "match_v":
            self.v_match.SetState(state)
        elif task == "match_r":
            self.r_match.SetState(state)
        elif task == "match_i":
            self.i_match.SetState(state)
        elif task == "phot_b":
            self.b_phot.SetState(state)
        elif task == "phot_v":
            self.v_phot.SetState(state)
        elif task == "phot_r":
            self.r_phot.SetState(state)
        elif task == "phot_i":
            self.i_phot.SetState(state)
        elif task == "analyze":
            self.bvri_analyze.SetState(state)
        elif task == "pretty":
            self.bvri_pretty.SetState(state)
        elif task == "report":
            self.bvri_report.SetState(state)
        else:
            print("ShowProgress: unknown task: ", task)
            
        
################################################################
##    class BVRITab
################################################################
class BVRITab:
    def __init__(self, parent_notebook, tab_label):
        self.ignore_callback = False 
        self.tab_buffer = gtk.TextBuffer()
        self.tab_view = gtk.TextView()
        self.tab_view.set_buffer(self.tab_buffer)
        self.tab_view.set_editable(False)
        self.tab_view.modify_font(Pango.FontDescription("mono 9"))
        self.tab_view.set_cursor_visible(False)
        self.tab_view.set_wrap_mode(gtk.WrapMode.CHAR)
        self.tab_sw = gtk.ScrolledWindow()
        self.tab_sw.add(self.tab_view)
        self.whole_box = gtk.VBox(spacing = 2)
        self.selected_analysis = None #holds single letter identifing
        self.mode_selected = { 'B' : 'Stacked',
                               'V' : 'Stacked',
                               'R' : 'Stacked',
                               'IR' : 'Stacked' }
        self.filter_included = { 'B' : True,
                                 'V' : True,
                                 'R' : True,
                                 'IR' : True }
        #current filter

        self.tab_buffer.create_tag("submit", background="pink")
        self.tab_buffer.create_tag("check", background="yellow")
        self.tab_buffer.create_tag("ensemble", background='palegreen')
        self.tab_buffer.create_tag("comp", background='dodgerblue')

        self.analysis_top = gtk.HBox(spacing = 2)

        # Blue box
        vb = gtk.VBox(spacing = 2)
        self.analysis_top.pack_start(vb, padding=0, fill=False, expand=False)
        lvb = gtk.Label('B')
        vb.pack_start(lvb, fill = False, expand=False, padding=0)

        self.exclude_blue = gtk.CheckButton(label='Exclude')
        self.exclude_blue.connect('toggled', self.a_button_cb, ["B", "Exclude"])
        vb.pack_start(self.exclude_blue, fill=False, expand=False, padding=0)
        
        # V (Visual) box
        vb = gtk.VBox(spacing = 2)
        self.analysis_top.pack_start(vb, fill=False, expand=False, padding=0)
        lvb = gtk.Label('V')
        vb.pack_start(lvb, fill = False, expand=False, padding=0)

        self.exclude_green = gtk.CheckButton(label='Exclude')
        self.exclude_green.connect('toggled', self.a_button_cb, ["V", "Exclude"])
        vb.pack_start(self.exclude_green, fill=False, expand=False, padding=0)
        
        # Red box
        vb = gtk.VBox(spacing = 2)
        self.analysis_top.pack_start(vb, fill=False, expand=False, padding=0)
        lvb = gtk.Label('R')
        vb.pack_start(lvb, fill = False, expand=False, padding=0)

        self.exclude_red = gtk.CheckButton(label='Exclude')
        self.exclude_red.connect('toggled', self.a_button_cb, ["R", "Exclude"])
        vb.pack_start(self.exclude_red, fill=False, expand=False, padding=0)

        # IR box
        vb = gtk.VBox(spacing = 2)
        self.analysis_top.pack_start(vb, fill=False, expand=False, padding=0)
        lvb = gtk.Label('IR')
        vb.pack_start(lvb, fill = False, expand=False, padding=0)

        self.exclude_ir = gtk.CheckButton(label='Exclude')
        self.exclude_ir.connect('toggled', self.a_button_cb, ["I", "Exclude"])
        vb.pack_start(self.exclude_ir, fill=False, expand=False, padding=0)

        vb = gtk.VBox(spacing = 2)
        
        # Refresh button
        rb = gtk.Button("Refresh")
        rb.connect("clicked", self.refresh_file_cb)
        
        # Request Light Curve button
        self.lightcurve_b = gtk.Button("Show Light Curve")
        self.lightcurve_b.connect("clicked", self.show_lightcurve);

        vb.pack_start(self.lightcurve_b, fill=True, expand=False, padding=0)
        vb.pack_start(rb, fill=True, expand=False, padding=0)
        self.analysis_top.pack_start(vb, fill=True, expand=False, padding=0)

        vb = gtk.VBox(spacing = 2)
        hb = gtk.HBox(spacing = 2)

        # Analyze as a Standard Field (i.e., no comp star)
        self.no_comp_b = gtk.CheckButton("Standard Field")
        hb.pack_start(self.no_comp_b, fill=False, expand=False, padding=0)

        # Analyze using ensemble comparison star
        self.do_ensemble_b = gtk.CheckButton("Use Ensemble")
        hb.pack_start(self.do_ensemble_b, fill=False, expand=False, padding=0)
        self.do_ensemble_b.set_active(True)
        self.do_ensemble_b.connect("toggled", self.ensemble_toggled)

        # Inhibit color transform button
        self.inhibit_transform_b = gtk.CheckButton("Inhibit\nTransforms")
        hb.pack_start(self.inhibit_transform_b, fill=False, expand=False, padding=0)

        self.whole_box.pack_start(self.analysis_top, fill=True, expand = False, padding=0)
        self.whole_box.pack_start(gtk.Separator(), fill=True, expand=False, padding=0)

        run_button = gtk.Button("Run Analysis")
        run_button.connect("clicked", self.run_button_cb)

        vb.pack_start(hb, fill=True, expand = False, padding=0)
        vb.pack_start(run_button, fill=True, expand=False, padding=0)
        self.analysis_top.pack_start(vb, fill=True, expand=False, padding=0)

        self.whole_box.pack_start(self.tab_sw, fill=True, expand=True, padding=0)
        parent_notebook.append_page(self.whole_box, gtk.Label(tab_label))
        self.tab_view.show()
        self.tab_sw.show()

    def ensemble_toggled(self, widget, data=None):
        if self.ignore_callback:
            return
        astro_db.SafeDirectiveChange(SessionGlobal.RecomputeAllBVRI,
                                     comp_analy.overall_summary.directive_juid,
                                     'set_use_ensemble',
                                     self.do_ensemble_b.get_active())

    def QueryUseEnsemble(self):
        return self.do_ensemble_b.get_active()

    def show_lightcurve(self, widget, data=None):
        def make_clean_url_string(s):
            return s.replace('-','%20')

        def today_jd():
            now = datetime.datetime.today()
            return int(julian.to_jd(now, fmt='jd'))
        
        starname = SessionGlobal.current_star.name
        end_jd = today_jd()
        start_jd = end_jd-700
        url_string = ('https://www.aavso.org/LCGv2/static.htm?' +
                      'DateFormat=Calendar&RequestedBands=B,V,R,I&Grid=true' +
                      '&view=api.delim&ident=' + make_clean_url_string(starname) +
                      '&fromjd=' + str(start_jd) + '&tojd=' + str(end_jd) +
                      '&delimiter=@@@')
        print(url_string)
        webbrowser.open(url_string)

    def delete_bvri_entry(self, widget, data=None):
        starname = SessionGlobal.current_star.name        

        os.system("delete_bvri_entry -n " + starname + " -i " +
                  os.path.join(SessionGlobal.homedir, "bvri.db"))
        message = gtk.MessageDialog(type=gtk.MESSAGE_ERROR,
                                    flags=gtk.DIALOG_MODAL,
                                    buttons=gtk.BUTTONS_OK)
        message.set_markup("Deleted entry from BVRI database for " + starname)
        message.run()
        message.destroy()
        
    def set_error_tab(self, errors_tab):
        self.errors_tab = errors_tab

    def refresh_file_cb(self, widget, data=None):
        self.refresh_file()

    def run_button_cb(self, widget, data=None):
        old_way = False
        if old_way:
            progress = BVRIProgress()
        
            print("Run Button clicked")
            print("Using:")
            print("   B: ", self.mode_selected['B'])
            print("   V: ", self.mode_selected['V'])
            print("   R: ", self.mode_selected['R'])
            print("   I: ", self.mode_selected['IR'])

            if (self.mode_selected['B'] != self.mode_selected['V'] or
                self.mode_selected['B'] != self.mode_selected['R'] or
                self.mode_selected['B'] != self.mode_selected['IR']):
                message = gtk.MessageDialog(type=gtk.MESSAGE_ERROR,
                                            flags=gtk.DIALOG_MODAL,
                                            buttons=gtk.BUTTONS_OK)
                message.set_markup("Cannot currently mix stacked and multiple image choices")
                message.run()
                message.destroy()
            else:
                if self.mode_selected['B'] == "Single":
                    # Single images (NOTE THAT THIS IS PRETTY THOROUGHLY BROKEN!)
                    dark_name = " -d " + os.path.join(SessionGlobal.homedir, "dark" +
                                                      str(int(0.5+image_region.main_image.get_exposure_time()))
                                                      + ".fits")
                    flat_name = " -s " + os.path.join(domedir, "flat_" + data + "c.fits")
                    images_list = (SessionGlobal.thumbs.get_stack_selected_images("Bc")
                                   + ' ' +
                                   SessionGlobal.thumbs.get_stack_selected_images("Vc")
                                   + ' ' +
                                   SessionGlobal.thumbs.get_stack_selected_images("Rc")
                                   + ' ' +
                                   SessionGlobal.thumbs.get_stack_selected_images("Ic")
                                   + ' ')
                else: # stacked images
                    dark_name = " " # already dark-subtracted
                    flat_name = " " # already flat-fielded
                    images_list = " "
                    for c in ['B', 'V', 'R', 'I']:
                        this_image = os.path.join(SessionGlobal.homedir,
                                                  SessionGlobal.current_star.name +
                                                  '_' + c + ".fits")
                        if os.access(this_image, os.R_OK) == False:
                            continue # skip this file
                        find_cmd = "find_stars -i " + this_image
                        progress.ShowProgress("find_"+c.lower(), 1)
                        os.system(find_cmd)
                        progress.ShowProgress("find_"+c.lower(), 2)
                        match_cmd = "star_match -e -f -h -b -n " + SessionGlobal.current_star.name
                        match_cmd += (" -i " + this_image)
                        progress.ShowProgress("match_"+c.lower(), 1)
                        os.system(match_cmd)
                        progress.ShowProgress("match_"+c.lower(), 2)
                        phot_cmd = "photometry -i " + this_image
                        progress.ShowProgress("phot_"+c.lower(), 1)
                        os.system(phot_cmd)
                        progress.ShowProgress("phot_"+c.lower(), 2)
                    
                        alt_color = c
                        if alt_color == "I":
                            alt_color = "IR"
                        if self.filter_included[alt_color]:
                            images_list += (this_image + ' ')
                    
                analyze_command = "analyze_bvri -e "
                if self.inhibit_transform_b.get_active():
                    analyze_command += " -t "
                if self.no_comp_b.get_active():
                    analyze_command += " -c "
                analyze_command += dark_name
                analyze_command += flat_name
                analyze_command += (" -n " + SessionGlobal.current_star.name + ' ')
                analyze_command += images_list
                print("Executing: ", analyze_command)
                progress.ShowProgress("analyze", 1)
                os.system(analyze_command)
                progress.ShowProgress("analyze", 2)

                answer_file = os.path.join(SessionGlobal.homedir,
                                           SessionGlobal.current_star.name + "_bvri.phot")

                pretty_print = "bvri_pretty "
                pretty_print += (" -n " + SessionGlobal.current_star.name + ' ')
                pretty_print += (" -i " + os.path.join(SessionGlobal.homedir, "bvri.db") + ' ')
                pretty_print += (" -e " + os.path.join(SessionGlobal.homedir,
                                                   SessionGlobal.current_star.name + ".err"))
                pretty_print += (" -o " + answer_file + ' ')
                print("Executing: ", pretty_print)
                report_line_file = "/tmp/_bvri_report.txt"
                bvri_report = "bvri_report -H "
                bvri_report += (" -n " + SessionGlobal.current_star.name + ' ')
                bvri_report += (" -i " + os.path.join(SessionGlobal.homedir, "bvri.db") + ' ')
                bvri_report += (" -o " + report_line_file)
                progress.ShowProgress("report", 1)
                os.system(bvri_report)
                update_report = "/home/mark/ASTRO/CURRENT/TOOLS/BVRI/insert_report_lines.py -a "
                update_report += (" -i " + os.path.join(SessionGlobal.homedir, "aavso_report.db") + ' ')
                update_report += (" -f " + report_line_file)
                print("Invoking:")
                print(update_report)
                os.system(update_report)
                progress.ShowProgress("report", 2)
                SessionGlobal.text_tabs.report_tab.update_tab()
            
                progress.ShowProgress("pretty", 1)
                os.system(pretty_print)
                progress.ShowProgress("pretty", 2)
                self.refresh_file()
                self.errors_tab.set_star()
                progress.Finished()
        else:
            # The new way
            command = '../ANALYZER/analyzer '
            command += ' -t ' + SessionGlobal.current_star.name + ' '
            command += ' -d ' + SessionGlobal.homedir
            command += ' > /tmp/analyzer.out 2>&1'
            print("Executing: ", command)
            os.system(command)

            command = '../ANALYZER/do_bvri.py '
            command += ' -t ' + SessionGlobal.current_star.name + ' '
            command += ' -d ' + SessionGlobal.homedir
            command += ' > /tmp/do_bvri.out 2>&1'
            print("Executing: ", command)
            os.system(command)

            SessionGlobal.RecomputeAllBVRI()

    def SetEnsembleAndExcludeButton(self):
        actual = comp_analy.overall_summary.use_ensemble
        if hasattr(comp_analy.overall_summary, 'directive'):
            excluded_colors = comp_analy.overall_summary.directive.ColorsToExclude()
        else:
            excluded_colors = []
        exclusion_info = [ (self.exclude_green, 'V'),
                           (self.exclude_blue, 'B'),
                           (self.exclude_red, 'R'),
                           (self.exclude_ir, 'I') ]
        self.ignore_callback = True
        for (b,f) in exclusion_info:
            exclude = filter_module.to_canonical[f] in excluded_colors
            b.set_active(exclude)
        self.do_ensemble_b.set_active(actual)
        self.ignore_callback = False

    def set_tags(self):
        start_iter = None
        for x in comp_analy.overall_summary.stars.values():
            if x.text_tag is not None:
                if start_iter is None:
                    start_iter = self.tab_buffer.get_start_iter()
                    start_iter.forward_chars(comp_analy.overall_summary.header_chars)
                begin_iter = start_iter.copy()
                begin_iter.forward_lines(x.line_no)
                end_iter = begin_iter.copy()
                end_iter.forward_lines(1)
                self.tab_buffer.apply_tag_by_name(x.text_tag, begin_iter, end_iter)
    
    def refresh_file_quiet(self):
        self.SetEnsembleAndExcludeButton()
        b = self.tab_buffer
        b.delete(b.get_start_iter(),b.get_end_iter())
        b.insert_markup(b.get_start_iter(),
                        comp_analy.overall_summary.GetDisplayString(),
                        -1)
        #self.tab_buffer.set_markup(comp_analy.overall_summary.GetDisplayString())
        self.set_tags()
        self.tab_view.show()
        self.tab_sw.show()
        return

        answer_file = os.path.join(SessionGlobal.homedir,
                                   SessionGlobal.current_star.name + "_bvri.phot")
        exists = os.path.isfile(answer_file)
        if exists:
            TextTab.SetFromFile(self.tab_buffer, answer_file)
        else:
            TextTab.SetFromFile(self.tab_buffer, "/dev/null")

    def refresh_file(self):
        self.SetEnsembleAndExcludeButton()
        b = self.tab_buffer
        b.delete(b.get_start_iter(),b.get_end_iter())
        b.insert_markup(b.get_start_iter(),
                        comp_analy.overall_summary.GetDisplayString(),
                        -1)
        #self.tab_buffer.set_markup(comp_analy.overall_summary.GetDisplayString())
        self.set_tags()
        self.tab_view.show()
        self.tab_sw.show()
        return
    
        answer_file = os.path.join(SessionGlobal.homedir,
                                   SessionGlobal.current_star.name + "_bvri.phot")
        exists = os.path.isfile(answer_file)
        if not exists:
            TextTab.SetFromFile(self.tab_buffer, "/dev/null")
            message = gtk.MessageDialog(type=gtk.MESSAGE_ERROR,
                                        flags=gtk.DIALOG_MODAL,
                                        buttons=gtk.BUTTONS_OK)
            message.set_markup("Cannot read pretty BVRI file.")
            message.run()
            message.destroy()
        else:
            TextTab.SetFromFile(self.tab_buffer, answer_file)
        self.tab_view.show()
        self.tab_sw.show()

    def a_button_cb(self, widget, data = None):
        if data[1] == "Exclude":
            if self.ignore_callback:
                return
            # data[0] is filter name
            self.filter_included[data[0]] = not widget.get_active()
            exclusion_info = [ (self.exclude_green, 'V'),
                               (self.exclude_blue, 'B'),
                               (self.exclude_red, 'R'),
                               (self.exclude_ir, 'I') ]
            exclusions = [x[1] for x in exclusion_info if x[0].get_active()]
            astro_db.SafeDirectiveChange(SessionGlobal.RecomputeAllBVRI,
                                         comp_analy.overall_summary.directive_juid,
                                         'set_color_exclusions',
                                         exclusions)
            
        else:
            print("ERROR: a_button_cb() invalid reason: ", data[1])

    def set_star(self, star_name):
        if self.selected_analysis != None:
            filename = os.path.join(SessionGlobal.homedir,
                                    SessionGlobal.star_name + self.selected_analysis + "c.phot")
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
        self.refresh_file()

