import os
import math
import traceback
import statistics
import sys
import gi
from gi.repository import Gdk as gdk, Gtk as gtk, GLib
import cairo

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
from PYTHON_LIB.IMAGE_LIB import star
from PYTHON_LIB.IMAGE_LIB import filter as filter_module

import comp_analy
import SessionGlobal

def IsAUID(name):
    return (name[0:3].isdigit() and name[8:11].isdigit() and name[4:7].isalpha and
            name[3] == '-' and name[7] == '-' and len(name) == 11)

def StarInResult(starname, result):
    try:
        #print("StarInResult(", starname, ", ", result, ")")
        matches = [r for r in result if r['name'] == starname]
        return len(matches) >= 1
    except:
        print("StarInResult failed: starname = ", starname, " and result = ", result)
        raise

################################################################
## Key classes:
##   DataLine -- holds the info needed for a single line of the EFF report
##   DataStar -- holds the info needed for a single reportable star
##
##   ReportWindow -- the root window
##   GUILine      -- holds one ReportLine
##   ReportStar   -- holds one star
##   ReportFilter -- holds data for one star/filter combo
################################################################

def ListSubmissions():
    db_obj = SessionGlobal.db_obj
    db = SessionGlobal.db
    if 'submissions' not in db:
        print("ListSubmissions: <empty>")
        return
    submissions = db['submissions']
    starnames = [x['computed']['cat_name'] for x in submissions]
    starnames = set(starnames)
    for star in starnames:
        sub_lines = [x['computed'] for x in submissions if
                     x['computed']['cat_name'] == star]
        filterlist = [x['filter'] for x in sub_lines]
        print("ListSubmissions: ", star, filterlist)

class ReportTab(gtk.ScrolledWindow):
    class WidgetInfo:
        def __init__(self, filter):
            self.left_col = None          # integer column number (computed)
            self.filter = filter          # filter (canonical)
            self.reset_button = None      # Button
            self.use_computed_b = None    # RadioButton
            self.inhibit_report_b = None  # RadioButton 
            self.use_manual_b = None      # RadioButton
            self.comp_gl = None           # GuiLine
            self.override_gl = None       # GuiLine
            self.bottom_text_label = None # Label
            
    
    def __init__(self, parent):
        super().__init__()
        self.set_policy(gtk.PolicyType.NEVER,
                        gtk.PolicyType.ALWAYS)
        box = gtk.VBox(spacing=2)
        self.add(box)
        parent.append_page(self, gtk.Label(label="AAVSO Report"))
        SessionGlobal.notifier.register(requestor=self,
                                        variable="overall_summary",
                                        condition="value_change",
                                        callback=self.SetTarget,
                                        debug="ReportTab")
        SessionGlobal.notifier.register(requestor=self,
                                        variable="current_star",
                                        condition="value_change",
                                        callback=self.ClearScreen,
                                        debug="ReportTab")
        screen = gdk.Screen.get_default()
        provider = gtk.CssProvider()
        style_context = gtk.StyleContext()
        style_context.add_provider_for_screen(screen,
                                              provider,
                                              gtk.STYLE_PROVIDER_PRIORITY_APPLICATION)
        provider.load_from_data("#small {padding: 0;}".encode())
        mini_css = b"""
        #mini {
          font-size: 9px;
          min-height: 0px;
          min-width: 0px;
          padding-bottom: 0px;
          padding-top: 0px;
        }
        """
        provider.load_from_data(mini_css)

        self.star_selector = gtk.ComboBoxText()
        box.pack_start(self.star_selector, expand=False,fill=False,padding=0)
        self.star_selector.connect("changed", self.new_star_cb)

        self.grid = gtk.Grid()
        self.pack_start(self.grid, expand=False, fill=False, padding=0)
        col = 0
        self.widget_info = {}     # key is filter, value is WidgetInfo
        self.gui_lines_mode = {}  # key is filter, value is mode string
        self.astro_db_submission = {} # key is filter, value points to astro_db['submission']
        for filter in ['B', 'V', 'R', 'I']:
            wi = ReportTab.WidgetInfo(filter)
            self.widget_info[filter] = wi
            wi.left_col = col
            
            # TOP LABEL
            filter_label = gtk.Label()
            filter_label.set_markup('<span size="x-large">'+filter+'</span>')
            self.grid.attach(filter_label, col, 0, 2, 1)
        
            # TOP BOXES
            topbox = gtk.HBox(spacing=2)
            topleft = gtk.VBox(spacing=0)
            topbox.pack_start(topleft, expand=False, fill=False, padding=0)
            reset_button = gtk.Button.new_with_label("Reset\nto\ncomputed")
            wi.reset_button = reset_button
            topbox.pack_start(reset_button, expand=False, fill=False, padding=0)
            reset_button.connect("clicked", self.do_button_cb, (filter, "reset"))
            use_computed_b = gtk.RadioButton(label='Use computed')
            wi.use_computed_b = use_computed_b
            use_computed_b.connect("toggled", self.do_button_cb, (filter, "use_comp"))
            inhibit_report_b = gtk.RadioButton(label='Do not report', group=use_computed_b)
            wi.inhibit_report_b = inhibit_report_b
            inhibit_report_b.connect("toggled", self.do_button_cb, (filter, "inhibit"))
            use_manual_b = gtk.RadioButton(label='Use override', group=use_computed_b)
            wi.use_manual_b = use_manual_b
            use_manual_b.connect("toggled", self.do_button_cb, (filter, "override"))
            topleft.pack_start(use_computed_b, expand=False, fill=False, padding=0)
            topleft.pack_start(inhibit_report_b, expand=False, fill=False, padding=0)
            topleft.pack_start(use_manual_b, expand=False, fill=False, padding=0)

            self.grid.attach(topbox, col, 1, 2, 1)

            wi.bottom_text_label = gtk.Label()
            wi.bottom_text_label.set_line_wrap(True)
            wi.bottom_text_label.set_justify(gtk.Justification.LEFT)
            wi.bottom_text_label.set_halign(gtk.Align.START)
            wi.bottom_text_label.set_xalign(0.0)
            self.pack_start(wi.bottom_text_label, expand=False, fill=False, padding=5)

            col += 2

        self.dld_comp = {} # key is filter, value is DataLineDict
        self.dld_man = {}

    def ClearScreen(self, variable, condition, data):
        print("aavso_report.ClearScreen()")
        self.star_selector.remove_all()

        for (filter,wi) in self.widget_info.items():
            root_col = wi.left_col

            # This is a serious problem. Somehow we get multiple
            # widgets attached to the grid...
            for which_col in [0,1]:
                empty = False
                child = self.grid.get_child_at(root_col+which_col, 2)
                while child is not None:
                    child.destroy()
                    child = self.grid.get_child_at(root_col+which_col, 2)
            wi.comp_gl = None
            wi.override_gl = None

            wi.bottom_text_label.set_text("")
            
    # The only things loaded here are the radio buttons, the manual
    # DataLineDictionary, and keep a link to the master submission
    
    def ReloadSubmissions(self, filter):
        loaded = False
        if 'submissions' in SessionGlobal.db:
            existing_submissions = [x for x in
                                    SessionGlobal.db['submissions'] if
                                    x['name'] == self.selected_star]
            for s in existing_submissions:
                if filter != s['filter']:
                    continue

                self.astro_db_submission[filter] = s
                loaded = True
                if 'inhibit' in s and s['inhibit'] != 0:
                    self.widget_info[filter].inhibit_report_b.set_active(True)
                    self.gui_lines_mode[filter] = 'inhibit'
                elif 'use_override' in s and s['use_override'] != 0:
                    self.widget_info[filter].use_manual_b.set_active(True)
                    self.gui_lines_mode[filter] = 'override'
                else:
                    self.widget_info[filter].use_computed_b.set_active(True)
                    self.gui_lines_mode[filter] = 'use_comp'
                if 'override' in s and len(s['override']) > 0:
                    self.widget_info[filter].override_gl = GuiLine(filter,
                                                                   self,
                                                                   editable=True,
                                                                   dict=s['override'])
        if not loaded:
            self.widget_info[filter].use_computed_b.set_active(True)
            self.gui_lines_mode[filter] = 'use_comp'
            self.widget_info[filter].override_gl = None
            self.MergeExistingSubmission(self.selected_star)
                
    def SetTarget(self, variable, condition, data):
        # This is the field target, not necessarily the star being reported
        starname = SessionGlobal.current_star.name
        print("ReportTab: SetTarget(", starname, ")")
        self.cat_dict = dict([(cat_entry.name, cat_entry) for
                              cat_entry in comp_analy.overall_summary.catalog])

        self.star_selector.remove_all()

        # These are the reportable stars
        report_names = [name for (name,sd) in comp_analy.overall_summary.stars.items()
                        if sd.is_any_submit]
        if len(report_names) > 0:
            for starname in report_names:
                self.star_selector.append_text(starname)
                self.SetupAAVSOReport(starname)
            self.star_selector.set_active(0) # this will trigger a callback
        print("ListSubmissions from aavso_report.SetTarget():")
        ListSubmissions()

    # This takes what's on the screen and ensures that it gets written
    # into the JSON database.
    def MergeExistingSubmission(self, starname):
        print("MergeExistingSubmission(", starname, ")")
        print(self.widget_info.keys(), self.dld_comp.keys())
        for filter in self.widget_info.keys():
            if filter not in self.dld_comp:
                continue
            
            updated = False
            new_entry = False
            existing_submission = None # existing_submission is at the juid level
            if 'submissions' in SessionGlobal.db:
                existing_submission = next((x for x in
                                            SessionGlobal.db['submissions']
                                            if x['filter'] == filter and
                                            x['name'] == starname), None)
            if existing_submission is not None and 'computed' in existing_submission:
                # replace the old one
                #print("MergeExistingSubmission(): dld_comp = ",
                #      self.dld_comp)
                #print("MergeExistingSubmission(",filter,").inhibit = ",
                #      existing_submission['inhibit'])
                #if filter in self.dld_man:
                    #print("MergeExistingSubmission(): dld_man = ", self.dld_man[filter].value)
                existing_submission['computed'] = self.dld_comp[filter].value
                updated = True
                if (filter in self.dld_man and
                    self.dld_man[filter] is not None and
                    len(self.dld_man[filter].value) > 0):
                    #print("MergeExistingSubmission(): adding override")
                    #print("----> orig existing_submission = ", existing_submission)
                    #print("----> self.dld_man[filter].value = ",
                    #      self.dld_man[filter].value)
                    existing_submission['override'] = self.dld_man[filter].value
            if not updated:
                new_entry = True
                existing_submission = { 'juid': SessionGlobal.db_obj.GetNextJUID('submission'),
                                        'filter' : filter,
                                        'name' : starname,
                                        'computed' : self.dld_comp[filter].value,
                                        'inhibit' : 0,
                                        'use_override' : 0,
                                        'tstamp' : 0, # fixed below
                                       }
                #self.astro_db_submission[filter] = submission
                SessionGlobal.db_obj.UpdateTStamp(existing_submission)

            if 'submissions' not in SessionGlobal.db:
                SessionGlobal.db['submissions'] = []
            if new_entry:
                SessionGlobal.db['submissions'].append(existing_submission)
            #print("MergeExistringSubmission: SafeChange to ",
            #      SessionGlobal.db['submissions'])
            print("MergeExistingSubmissions: SafeChange for ",
                  starname, "(", filter, ")")
            astro_db.SafeSubmissionChange(SessionGlobal.ReloadReport,
                                          SessionGlobal.db['submissions'])
                
    def SetupAAVSOReport(self, starname):
        self.datalines = {} # key is filter, value is dataline
        if starname is not None:
            self.selected_star = starname
            self.analysis = comp_analy.overall_summary.analysis
            # Normally will find one "result" for each filter
            self.results = [x for x in self.analysis['results'] if x['name'] == starname]

            self.gui_lines_comp = {} # key is filter, value is GuiLine
            self.gui_lines_manual = {} # same thing
            
            for filter in ['B', 'V', 'R', 'I']:
                print("SetupAAVSOReport(",starname,"): ", filter)
                result = None
                for r in self.results:
                    profile = next((x for x in self.analysis['profiles'] if
                                    x['pnum'] == r['profile']), None)
                    if profile is None:
                        print("aavso_report: new_star_cb: profile missing")
                    else:
                        if filter == profile['filter']:
                            result = r
                            break
                if result == None:
                    continue

                print(".... fetching data for ", starname, " [",filter,"]")
                dataline = DataLine(SessionGlobal.db_obj, starname)
                self.datalines[filter] = dataline
                dataline.InitFromJSON(self.analysis,
                                      result,
                                      SessionGlobal.db['inst_mags'],
                                      self.cat_dict[starname],
                                      self.cat_dict)
                self.RefreshDLDComp(filter)
                self.ReloadSubmissions(filter)
        print("End of SetupAAVSOReport():")
        ListSubmissions()

    def new_star_cb(self, combo):
        starname = combo.get_active_text()
        self.SetupAAVSOReport(starname)

        for filter in ['B', 'V', 'R', 'I']:
            self.Refresh_color(filter)
        self.RefreshBottomLines()

        self.show_all()

    def do_button_cb(self, widget, value_pair):
        (filter,action) = value_pair
        print("do_button_cb: (filter,action) = ", (filter,action))
        if action in ['use_comp', 'inhibit', 'override'] and widget.get_active() == False:
            return
        if action == 'reset':
            self.gui_lines_manual[filter] = GuiLine(filter, self,
                                                    True, self.dld_comp[filter].value)
            self.dld_man[filter] = DataLineDict(self.datalines[filter])
            self.dld_man[filter].value = dict(self.dld_man[filter].value)
            self.UpdateSubmissionFromScreen(filter)
            self.MergeExistingSubmission(self.selected_star)
        elif action == 'use_comp':
            self.gui_lines_mode[filter] = 'use_comp'
            self.UpdateSubmissionFromScreen(filter)
            self.MergeExistingSubmission(self.selected_star)
        elif action == 'inhibit':
            self.gui_lines_mode[filter] = 'inhibit'
            self.UpdateSubmissionFromScreen(filter)
            self.MergeExistingSubmission(self.selected_star)
        elif action == 'override':
            self.gui_lines_mode[filter] = 'override'
            self.UpdateSubmissionFromScreen(filter)
            if filter not in self.gui_lines_manual:
                self.gui_lines_manual[filter] = GuiLine(filter, self,
                                                        True, self.dld_comp[filter].value)
                self.dld_man[filter] = DataLineDict(self.datalines[filter])
                self.dld_man[filter].value = dict(self.dld_man[filter].value)
            self.MergeExistingSubmission(self.selected_star)
        print("do_button_cb: final mode is ", self.gui_lines_mode[filter])
        return

    def UpdateSubmissionFromScreen(self, filter):
        # Updates based on mode
        mode = self.gui_lines_mode[filter]
        if mode == 'inhibit':
            self.astro_db_submission[filter]['inhibit'] = 1
        elif mode == 'use_comp':
            self.astro_db_submission[filter]['inhibit'] = 0
            self.astro_db_submission[filter]['use_override'] = 0
        elif mode == 'override':
            self.astro_db_submission[filter]['inhibit'] = 0
            self.astro_db_submission[filter]['use_override'] = 1
        else:
            print("UpdateSubmissionFromScreen(): bad mode: ", mode)
            raise ValueError

    def RefreshDLDComp(self, filter):
        self.dld_comp[filter] = DataLineDict(self.datalines[filter])

    def RebuildDLDFromText(self, filter):
        self.dld_man[filter].value = self.gui_lines_manual[filter].dict

    def Refresh_color(self, filter):
        wi = self.widget_info[filter]
        root_col = wi.left_col
        mode = self.gui_lines_mode[filter]

        man_child = self.grid.get_child_at(root_col+1, 2)
        comp_child = self.grid.get_child_at(root_col, 2)
        if man_child is not None:
            man_child.destroy()
        if comp_child is not None:
            comp_child.destroy()
        wi.comp_gl = None
        wi.override_gl = None

        # DATA COLUMNS
        if filter not in self.dld_comp:
            return
        this_column = GuiLine(filter, self,
                              editable=False,
                              dict=self.dld_comp[filter].value)

        self.gui_lines_comp[filter] = root_col
        self.grid.attach(this_column, root_col, 2, 1, 1)
        wi.comp_gl = this_column

        if mode == 'override':
            print("Refresh_color(",filter,") adding override lines.")
            this_column = GuiLine(filter, self,
                                  editable=True,
                                  dict=self.dld_man[filter].value)
            self.gui_lines_manual[filter] = this_column
            self.grid.attach(this_column, root_col+1, 2, 1, 1)
            wi.override_gl = this_column
        self.show_all()
        
    def RefreshBottomLines(self):
        for filter in ['B', 'V', 'R', 'I']:
            self.widget_info[filter].bottom_text_label.set_text("")
            
        for (filter, data) in self.dld_comp.items():
            mode = self.gui_lines_mode[filter]
            if mode == 'inhibit':
                line = ""
            elif mode == 'use_comp':
                line = data.ToString()
            else:
                line = self.dld_man[filter].ToString()

            line_label = self.widget_info[filter].bottom_text_label
            line_label.set_text(line)
            line_label.set_line_wrap(True)
            line_label.set_justify(gtk.Justification.LEFT)
            line_label.set_halign(gtk.Align.START)
            line_label.set_xalign(0.0)
            
class GuiLine(gtk.Grid):
    def __init__(self, filter, parent, editable, dict):
        super().__init__()
        self.filter = filter
        self.parent = parent
        self.dict = dict
        row = 0
        for (key,value) in dict.items():
            key_l = gtk.Label()
            #key_l.set_markup("<span size='x-small'>"+key+"</span>")
            key_l.set_text(key)
            key_l.set_name('mini') # assign special CSS
            if key == 'ensemble_members':
                value_e = gtk.Label()
                value = str(value).replace(',','\n')
                value_e.set_text(value)
            else:
                value_e = gtk.Entry()
                value_e.set_editable(editable)
                value_e.set_size_request(5,-1)
                value_e.set_width_chars(5)
                value_e.set_max_width_chars(5)
                if isinstance(value_e, str):
                    value_e.set_text(value)
                    #value_e.set_markup("<span size='x-small'>"+value+"</span>")
                else:
                    value_e.set_text(str(value))
                    #value_e.set_markup("<span size='x-small'>"+str(value)+"</span>")
                if editable:
                    value_e.connect("activate", self.value_cb, (key,filter))
            value_e.set_name('mini')
            self.attach(key_l, 0, row, 1, 1)
            self.attach(value_e, 1, row, 1, 1)
            row += 1

    # This handles entry of a manually-overridden value; update the
    # screen and update the 'submissions' section of astro_db.
    def value_cb(self, widget, value):
        print("GuiLine.value_cb(", value, ")")
        (variable,filter) = value
        #if variable in ['time', 'mag', 'mag_inst']:
        #    self.dict[variable] = float(widget.get_text())
        #elif variable in ['num_check_stars', 'group']:
        #    self.dict[variable] = int(widget.get_text())
        #elif widget.get_text() in ['ENSEMBLE','na']:
        #    self.dict[variable] = widget.get_text()
        if widget.get_text() == 'None':
            self.dict[variable] = None
        else:
            self.dict[variable] = widget.get_text()
        self.parent.RebuildDLDFromText(filter)
        astro_db.SafeSubmissionChange(SessionGlobal.ReloadReport,
                                      SessionGlobal.db['submissions'])


class DataLineDict:
    def __init__(self, dataline, json=None):
        if json is not None:
            self.value = json
            return
        
        ret = {}
        ret['technique'] = dataline.technique       # string
        ret['cat_name'] = dataline.cat_name         # string
        ret['report_name'] = dataline.report_name   # string or None
        ret['time'] = "{:.4f}".format(dataline.time) # float
        ret['mag'] = "{:.3f}".format(dataline.mag)   # float
        ret['mag_inst'] = "{:.3f}".format(dataline.mag_inst)   # float
        ret['nobs'] = str(dataline.nobs) if dataline.nobs is not None else None
        ret['filter'] = dataline.filter             # string
        ret['is_transformed'] = dataline.is_transformed # boolean
        ret['compname'] = dataline.compname         # string
        ret['comp_auid'] = dataline.comp_auid       # string
        ret['compmag_inst'] = ("{:.3f}".format(dataline.compmag_inst)
                               if isinstance(dataline.compmag_inst, float) else dataline.compmag_inst)
        ret['compmag_std'] = ("{:.3f}".format(dataline.compmag_std) if
                              isinstance(dataline.compmag_inst, float) else dataline.compmag_inst)
        ret['checkname'] = dataline.checkname       # string
        ret['check_auid'] = dataline.check_auid     # string
        if isinstance(dataline.checkmag_inst, float):
            ret['checkmag_inst'] = "{:.3f}".format(dataline.checkmag_inst)
        else:
            if dataline.checkmag_inst is None:
                ret['checkmag_inst'] = "na"
            else:
                ret['checkmag_inst'] = dataline.checkmag_inst
        if isinstance(dataline.checkmag_std, float):
            ret['checkmag_std'] = "{:.3f}".format(dataline.checkmag_std)
        else:
            if dataline.checkmag_std is None:
                ret['checkmag_std'] = 'na'
            else:
                ret['checkmag_std'] = dataline.checkmag_std
                
        ret['uncty'] = "{:.3f}".format(dataline.uncty)
        ret['uncty_src'] = dataline.uncty_src       # string
        ret['airmass'] = "{:.4f}".format(dataline.airmass)
        ret['chart'] = dataline.chart               # string
        ret['xform_del_color'] = "{:.3f}".format(dataline.xform_del_color)
        ret['xform_coef_name'] = dataline.xform_coef_name
        ret['xform_coef_value'] = "{:.3f}".format(dataline.xform_coef_value)
        ret['xform_adj_amount'] = "{:.3f}".format(dataline.xform_adj_amount)
        ret['xform_ref_color'] = "{:.3f}".format(dataline.xform_ref_color)
        ret['xform_ref_color_name'] = dataline.xform_ref_color_name
        ret['ensemble_members'] = dataline.ensemble_members # list of strings
        if dataline.ensemble_fitting is not None:
            ret['ensemble_fitting'] = "{:.3f}".format(dataline.ensemble_fitting)
        else:
            ret['ensemble_fitting'] = 'na'
        ret['check_rms'] = ("{:.3f}".format(dataline.check_rms)
                            if dataline.check_rms is not None else None)
        ret['num_check_stars'] = str(dataline.num_check_stars) # integer
        ret['group'] = str(dataline.group)                     # integer
        ret['comments'] = dataline.comments

        self.value = ret
        
    # This creates an AEFF line
    def ToString(self):
        raw_name = (self.value['cat_name'] if self.value['report_name'] ==
                    None else self.value['report_name']).upper()
        if 'GSC' in raw_name:
            name = raw_name
        else:
            name = raw_name.replace("-"," ")
        ret = name
        ret += ','
        ret += self.value['time']
        ret += ','
        ret += self.value['mag']
        ret += ','
        ret += self.value['uncty']
        ret += ','
        ret += self.value['filter'] # canonical filter name is okay
        ret += ','
        ret += "YES" if self.value['is_transformed'] else "NO"
        ret += ',STD,'
        ret += self.value['comp_auid']
        ret += ','
        ret += self.value['compmag_inst']
        ret += ','
        ret += self.value['check_auid']
        ret += ','
        ret += self.value['checkmag_inst']
        ret += ','
        ret += self.value['airmass']
        ret += ','
        ret += self.value['group']
        ret += ','
        ret += self.value['chart']
        ret += ','
        if self.value['technique'] != 'ENSEMBLE':
            if self.value['comments'] == None or len(self.value['comments']) == 0:
                ret += 'na'
            else:
                ret += self.value['comments']
        else:
            is_first = True
            for (key,var,value) in [
                    ('uncty_src',
                     'UNCTY',
                     self.value['uncty_src']),
                    ('mag_inst',
                     self.value['filter']+'MAGINS',
                     self.value['mag_inst']),
                    ('checkmag_std',
                     'CHECKMAG',
                     self.value['checkmag_std']),
                    ('xform_del_color',
                     'DEL_'+self.value['xform_ref_color_name'],
                     self.value['xform_del_color']),
                    ('xform_coef_value',
                     self.value['xform_coef_name'],
                     self.value['xform_coef_value']),
                    ('nobs','NOBS',self.value['nobs']),
                    ('xform_ref_color',
                     'REF_'+self.value['xform_ref_color_name'],
                     self.value['xform_ref_color']),
                    ('xform_adj_amount',
                     'XADJ', self.value['xform_adj_amount']),
                    ('ensemble',
                     'ENSEMBLE',
                     self.ConcatAUIDs(self.value['ensemble_members'])),
                    ('check_rms','KERR',self.value['check_rms']),
                    ('num_check_stars',
                     'NUMCHECKSTARS',
                     self.value['num_check_stars'])]:
                ret += self.AddNote(is_first, key, var, value)
                is_first = False
        return ret

    def ConcatAUIDs(self, auidlist): # auidlist is list of integers
        ret = ""
        is_first = True
        for auid in auidlist:
            if is_first:
                is_first = False
            else:
                ret += ','
            ret += str(auid)
        return ret

    def AddNote(self, is_first, key, var, value):
        if value is None:
            return ""
        ret = ""
        if not is_first:
            ret += '|'
        try:
            ret += (var + '=' + value)
        except:
            print("key = ", key, "var = ", var, "value = ", value)
            raise
        return ret

class DataLine:
    def __init__(self, db_obj, name):
        self.db_obj = db_obj
        self.cat_name = name
        self.calculated = True  # False if human-entered
        self.report_name = None # string
        self.time = None        # float
        self.mag = None         # float
        self.mag_inst = None    # float
        self.nobs = None        # integer or None
        self.filter = None      # string
        self.is_transformed = None # boolean
        self.compname = None       # string
        self.comp_auid = None      # string (might be "NA")
        self.compmag_inst = None   # float or string ("na")
        self.compmag_std = None    # float or string ("na")
        self.checkname = None
        self.check_auid = None
        self.checkmag_inst = None
        self.checkmag_std = None
        self.uncty = None       # float
        self.uncty_src = None   # string (e.g., 'STDDEV', 'Fitting', )
        self.airmass = None     # float or string ('na')
        self.chart = None       # WHERE DOES THIS COME FROM?????
        self.xform_del_color = None
        self.xform_coef_name = None
        self.xform_coef_value = None
        self.xform_adj_amount = None
        self.xform_ref_color = None
        self.xform_ref_color_name = None
        self.ensemble_members = None # string (list of AUID)
        self.ensemble_fitting = None # float (or None)
        self.check_rms = None        # float when reporting multiple checkstars
        self.num_check_stars = None  # int when reporting multiple checkstars
        self.group = None
        self.comments = ""

    # "analysis" -- reference to the entire list item in analyses for
    # this target
    # "result" -- This refers to a single standard mag item in "results"
    def InitFromJSON(self, analysis, result, inst_mags, cat_star, catalog):
        def GetInstMag(cat_name, sources): # 
            inst_mag_list = []
            for d in inst_mags:
                if d['exposure'] in sources:
                    measurements = d['measurements']
                    match = next((x for x in measurements if x['name'] == cat_name),None)
                    if match is not None:
                        inst_mag_list.append(match['imag'])
            if len(inst_mag_list) > 0:
                return statistics.mean(inst_mag_list)
            else:
                return None

        def GetChartID(sources):
            chartlist = set()
            for s in sources:
                image = self.db_obj.FindExposureByJUID(s)
                if image is not None and 'chart' in image:
                    chartlist.add(image['chart'])
            if len(chartlist) == 0:
                return None
            if len(chartlist) != 1:
                print("Multiple charts referenced: ", chartlist)
            return chartlist.pop()
                    
        profile_dict = dict([(x['pnum'],x) for x in analysis['profiles']])
        profile = profile_dict[result['profile']]
        #profile = next((x for x in analysis['profiles'] if x['pnum'] == result['profile']), None)

        self.analysis = analysis
        self.calculated = True
        self.report_name = self.cat_name if cat_star.report_name is None else cat_star.report_name
        self.time = profile['jd']
        self.mag = result['mag']
        self.mag_inst = GetInstMag(self.cat_name, profile['sources'])
        self.nobs = profile['numvals'] if 'numvals' in profile else 1
        self.chart = GetChartID(profile['sources'])
        self.filter = profile['filter']
        self.is_transformed = result['transformed']
        if analysis['technique'] == 'ENSEMBLE':
            self.technique = 'ENSEMBLE'
            self.compname = 'ENSEMBLE' 
            self.comp_auid = 'ENSEMBLE'
            self.compmag_inst = 'na'
            self.compmag_std = 'na'
            self.ensemble_members = [catalog[x].AUID for x in profile['comps']]
            self.ensemble_fitting = next((fit['stdev'] for
                                          (filter,fit) in analysis['ensemble_fit'].items()
                                          if filter == self.filter), None)
        else:
            self.technique = 'COMP'
            self.compname = profile['comps'][0]
            self.comp_auid = catalog[self.compname].AUID # Might be None
            self.ensemble_members = []

        checkfit = next((fit for (filter,fit) in analysis['check_fit'].items()
                         if filter == self.filter), None)
        if checkfit is not None:
            self.num_check_stars = len(checkfit['errs'])
            if 'rms' in checkfit:
                self.check_rms = checkfit['rms']
            else:
                self.check_rms = None
        if 'airmass' in profile:
            self.airmass = profile['airmass']
        else:
            self.airmass = 'na'
        self.uncty = result['uncty']
        self.uncty_src = result['uncty_src']
        self.group = result['group']

        ################################
        # Reportable check star
        ################################
        chosen_check = None
        checkrefs = [name for (name,cat) in catalog.items() if cat.IsCheck(self.filter) and
                     cat.is_ref and StarInResult(name, analysis['results']) and
                     cat.AUID not in self.ensemble_members]
        if len(checkrefs) == 1:
            chosen_check = checkrefs[0]
        elif len(checkrefs) > 1:
            # Pick one; what criteria to use?? Choose brightest.
            best_starname = checkrefs[0]
            best_mag = catalog[best_starname].ref_mag[self.filter]
            for c in checkrefs[1:]:
                starname = c
                (mag, uncty) = catalog[starname].ref_mag[self.filter]
                if mag < best_mag:
                    best_starname = starname
                    best_mag = mag
            chosen_check = best_starname
        else:
            # Need to pick a non-ref check
            checks = [name for (name,cat) in catalog.items() if cat.IsCheck(self.filter) and
                      StarInResult(name, analysis['results']) and
                      cat.AUID not in self.ensemble_members]
            best_starname = None
            best_mag = 99.9
            for c in checks:
                if self.filter not in catalog[c].ref_mag:
                    continue
                (mag, uncty) = catalog[c].ref_mag[self.filter]
                if mag < best_mag:
                    best_starname = c;
                    best_mag = mag
            chosen_check = best_starname
        cat = None
        measurement = None
        if chosen_check == None:
            self.checkname = 'na'
            self.check_auid = 'na'
            self.checkmag_inst='na'
            self.checkmag_std='na'
        else:
            self.checkname = chosen_check
            cat = catalog[self.checkname]
            self.check_auid = cat.AUID
            measurement = next((r for r in analysis['results']
                                if r['name'] == chosen_check and
                                profile_dict[r['profile']]['filter'] == self.filter),None)
            if measurement is not None:
                self.checkmag_std = measurement['mag']
            if 'sources' in profile:
                self.checkmag_inst = GetInstMag(self.checkname, profile['sources'])
            else:
                self.checkmag_inst = 'na'

        print("Checkmag: name=",chosen_check,"(",self.filter,
              "), analysis juid = ", self.analysis['juid'])
        if isinstance(self.checkmag_inst, str):
            print("Checkmag concern: ", chosen_check, "(",
                  self.filter, ")")
            if cat is not None:
                print("check_auid = ", cat.AUID)
            print("measurement = ", measurement)
            print("analysis juid = ", self.analysis['juid'])

        ################################
        # Color Transform Info
        ################################
        if 'xform_info' in result:
            info = result['xform_info']
            for (varname,value) in info.items():
                if 'DELTA' in varname:
                    self.xform_ref_color_name = varname[6:]
                    self.xform_del_color = value
                    #print("xform_ref_color_name: ", varname, self.xform_ref_color_name)
                elif varname[0] == 'T' and '_' in varname:
                    self.xform_coef_name = varname
                    self.xform_coef_value = value
                    
            self.xform_adj_amount = info['X_ADJ']
            self.xform_ref_color = None # the reference DELTA_BV value (float)

        if 'ref_color' in analysis:
            ref_color_data = analysis['ref_color']
            if self.xform_ref_color_name is not None and len(self.xform_ref_color_name) == 2:
                color1 = self.xform_ref_color_name[0]
                color2 = self.xform_ref_color_name[1]
                if color1 in ref_color_data and color2 in ref_color_data:
                    self.xform_ref_color = ref_color_data[color1]-ref_color_data[color2]
            
