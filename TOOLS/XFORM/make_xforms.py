#!/usr/bin/python3

import gi
gi.require_version('Gtk', '3.0')
from gi.repository import Gtk as gtk
import matplotlib.pyplot as plt
import matplotlib.figure
from matplotlib.backends.backend_gtk3agg import FigureCanvasGTK3Agg as FigureCanvas

import getopt
import sys
import os
import glob

import context
import target
import xforms
import coefficients_json

sys.path.insert(1, '/home/mark/ASTRO/CURRENT')
from PYTHON_LIB.ASTRO_DB_LIB import astro_db

class XFMainWin(gtk.Window):
    def __init__(self):
        super().__init__(title="Transformation Coefficients")
        self.connect("destroy", gtk.main_quit)
        self.set_default_size(700,700)
        self.set_border_width(3)

        self.notebook = gtk.Notebook()
        self.add(self.notebook)

        self.coef_page = CoefTab(self)
        self.notebook.append_page(self.coef_page, gtk.Label(label="Coefficient"))

        #self.merge_page = MergeTab(self)
        #self.notebook.append_page(self.merge_page, gtk.Label(label="Merge"))

        self.show_all()

    def FinishSetup(self):
        self.coef_page.FinishSetup()

class CoefTab(gtk.HBox):
    def __init__(self, parent):
        super().__init__()
        self.parent = parent    # the XFMainWin
        self.db_obj = None
        self.db = None

        self.mininotebook = gtk.Notebook()
        self.pack_start(self.mininotebook, False, False, 3)

        self.session_selector = SessionTab(self)
        self.mininotebook.append_page(self.session_selector, gtk.Label(label="Sessions"))

        self.target_selector = TargetTab(self)
        self.mininotebook.append_page(self.target_selector, gtk.Label(label="Targets"))

        right_box = gtk.VBox()
        self.pack_start(right_box, True, True, 3)
        top_box = gtk.HBox()
        right_box.pack_start(top_box, False, False, 3)
        self.coefficient_selector = CoefficientSelector(self)
        top_box.pack_start(self.coefficient_selector, False, False, 3)
        button_box = gtk.VBox()
        top_box.pack_start(button_box, False, False, 3)

        self.regression_graph = RegressionGraph(self)
        right_box.pack_start(self.regression_graph, True, True, 3)

    def FinishSetup(self):
        self.target_selector.FinishSetup()
        self.coefficient_selector.FinishSetup()
        self.session_selector.FinishSetup() # initiates a directory change event

    def ChangeDir(self, filename):
        print("ChangeDir starting: ", filename)
        self.RefreshDB(filename)
        self.target_selector.Refresh()
        self.regression_graph.Refresh()
        print("ChangeDir finished.")

    def ChangeCoef(self, coef_name):
        self.regression_graph.ChangeCoef(coef_name)

    def ChangeTargets(self, target_list):
        self.regression_graph.ChangeTargets(target_list)

    def GetSelectedTargets(self):
        return self.target_selector.GetSelectedTargets()

    def GetSelectedCoef(self):
        return self.coefficient_selector.GetSelectedCoef()

    def RefreshDB(self, filename):
        print("RefreshDB: loading json structure from ", filename)
        context.rootdir = os.path.dirname(filename)
        astro_db_filename = filename
        if not os.access(astro_db_filename, os.R_OK):
            print("Cannot open ", astro_db_filename)
            sys.exit(-2)

        context.db_obj = astro_db.AstroDB(context.rootdir)
        context.db = context.db_obj.GetData()

        if 'inst_mags' not in context.db:
            print("Photometry not available in ", astro_db_filename)
            sys.exit(-2)

        target.BuildTargetLists() # this populates context.target_list
        print("RefreshDB finished. context.target_list = ", context.target_list)
        
class TargetTab(gtk.ScrolledWindow):
    def __init__(self, parent):
        super().__init__()
        self.set_policy(gtk.PolicyType.NEVER,
                        gtk.PolicyType.ALWAYS)
        box = gtk.VBox()
        self.box = box
        self.add(box)
        self.parent = parent
        self.selected_targets = []       # list of strings (target names)
        self.target_buttons = []         # holds pairs: (button, target_name)
        set_button = gtk.Button.new_with_label("Set All")
        clear_button = gtk.Button.new_with_label("Clear All")
        self.bulk_operation_pending = False
        button_box = gtk.HBox()
        button_box.pack_start(clear_button, False, False, 1)
        button_box.pack_start(set_button, False, False, 1)
        box.pack_start(button_box, False, False, 1)

        set_button.connect("clicked", self.SetClear_cb, True)
        clear_button.connect("clicked", self.SetClear_cb, False)

    def FinishSetup(self):
        self.Refresh()

    def SetClear_cb(self, button, set_active_request):
        print("SetClear_cb() starting (",set_active_request,")")
        self.bulk_operation_pending = True
        for (t_button, tgt) in self.target_buttons:
            t_button.set_active(set_active_request)
        self.bulk_operation_pending = False
        self.selected_targets = [tgt for (button,tgt) in self.target_buttons if button.get_active()]
        self.parent.ChangeTargets(self.selected_targets)
        print("SetClear_cb() finished.")

    def Refresh(self):
        print("TargetTab.Refresh() starting")
        for b in self.target_buttons:
            b[0].destroy()
        self.target_buttons = []

        z = list(context.target_list.keys())
        z.sort()
        target_list = z
        print("TargetTab.Refresh(): target_list is ", target_list)
        for t in target_list:
            button = gtk.CheckButton(label=t)
            button.connect("toggled", self.ChangeTgt_cb, t)
            self.box.pack_start(button, False, False, 0)
            self.target_buttons.append((button,t))
        self.show_all()
        
    def GetSelectedTargets(self):
        self.selected_targets = [b[1] for b in self.target_buttons if b[0].get_active()]
        return self.selected_targets

    def ChangeTgt_cb(self, button, filename):
        if not self.bulk_operation_pending:
            self.selected_targets = [b[1] for b in self.target_buttons if b[0].get_active()]
            self.parent.ChangeTargets(self.selected_targets)

class SessionTab(gtk.ScrolledWindow):
    def __init__(self, parent):
        super().__init__()
        self.set_policy(gtk.PolicyType.NEVER,
                        gtk.PolicyType.ALWAYS)
        box = gtk.VBox()
        self.add(box)
        self.parent = parent
        self.selected_session = None

        group = None
        print("Starting scan for astro_db.json files.")
        files = glob.glob('/home/IMAGES/*/astro_db.json')
        # reverse sort by date (most recent first)
        def SortKey(filename):
            dir_path = os.path.dirname(filename)
            date_dir = os.path.basename(dir_path)
            date_parts = date_dir.split('-')
            return (31*int(date_parts[0])+int(date_parts[1])+366*(int(date_parts[2])-2000))
        files.sort(key=SortKey,reverse=True)
        for file in files:
            longdir = os.path.dirname(file)
            date_dir = os.path.basename(longdir)
            button = gtk.RadioButton.new_with_label_from_widget(group, date_dir)
            button.connect("toggled", self.ChangeDir_cb, file)
            box.pack_start(button, False, False, 0)
            if group is None:
                self.selected_session = file
                group = button

    def ChangeDir_cb(self, button, filename):
        if button.get_active():
            self.selected_session = filename
            self.parent.ChangeDir(filename)

    def FinishSetup(self):
        self.parent.ChangeDir(self.selected_session)

class RegressionGraph(gtk.VBox):
    def __init__(self, parent):
        super().__init__()
        self.parent = parent
        #self.pack_start(gtk.Label(label="Graph Goes Here"), True, True, 3)
        self.plot_figure = plt.figure()
        self.add(FigureCanvas(self.plot_figure))
        self.main_plot = None

    def ChangeDir(self, filename):
        print("RegressionGraph: ChangeDir(", filename, ")")
        self.Refresh()

    def ChangeTargets(self, target_list):
        print("RegressionGraph: ChangeTargets()")
        self.Refresh()

    def ChangeCoef(self, coef_list):
        print("RegressionGraph: ChangeCoef(", coef_list, ")")
        self.ReDraw()

    def Refresh(self):
        print("RegressionGraph.Refresh() getting target list from parent")
        target_list = self.parent.GetSelectedTargets()
        print("RegressionGraph.Refresh() received: ", target_list)
        xforms.LoadExposures(target_list)
        self.coef_dict = xforms.RunRegressions()
        self.ReDraw()

    def ReDraw(self):
        selected_coef_name = self.parent.GetSelectedCoef()
        print("...ReDraw: selected_coef_name = ", selected_coef_name)
        coefficient = next((c for c in xforms.coefficients if c.name == selected_coef_name),None)
        print("...ReDraw: coefficient = ", coefficient)
        if (coefficient is None or coefficient.model is None or
            len(coefficient.model.display_points) == 0):
            print("RegressionGraph.ReDraw(): selected coefficient is None.")
            return
        # get rid of any existing plot
        if self.main_plot != None:
            self.main_plot.cla()
        else:
            self.main_plot = self.plot_figure.add_subplot(1,1,1)

        self.main_plot.set_ylabel(
            '('+coefficient.y_axis_pair[0]+'-'+coefficient.y_axis_pair[1]+')')
        self.main_plot.set_xlabel(
            '('+coefficient.x_axis_pair[0]+'-'+coefficient.x_axis_pair[1]+')')
        self.main_plot.set_title(label=coefficient.name)
        
        if coefficient.model is None or len(coefficient.model.display_points) == 0:
            return

        x_array = [p.x_val for p in coefficient.model.display_points]
        y_array = [p.y_val_adjusted for p in coefficient.model.display_points]
        color_array = ['r' if p.exclude else 'k' for p in coefficient.model.display_points]
        slope_negative = (coefficient.model.value < 0.0)
        slope_str = '{:+.5f}'.format(coefficient.model.value)
        slope_err_str = '{:.5f}'.format(coefficient.model.std_err_slope)
        r_sq_str = '{:.4f}'.format(coefficient.model.rsquared)

        x_low = min(x_array)
        x_high = max(x_array)
        line_y_low  = coefficient.model.disp_intercept + x_low*coefficient.model.disp_slope
        line_y_high = coefficient.model.disp_intercept + x_high*coefficient.model.disp_slope
        self.main_plot.plot([x_low,x_high],[line_y_low,line_y_high],color='red',
                            marker=None, linestyle='solid')
        self.main_plot.scatter(x_array, y_array, c=color_array, marker='o', s=2)
        self.main_plot.text(0.85 if slope_negative else 0.05, 0.9,
                            selected_coef_name+'= '+slope_str+'\n'+
                            'err = '+slope_err_str+'\n'+
                            'r^2 = '+r_sq_str, transform=self.main_plot.transAxes)

        self.plot_figure.canvas.draw()
        
class CoefficientSelector(gtk.Grid):
    def __init__(self, parent):
        super().__init__()
        self.parent = parent
        self.coef_buttons = []  # list of pairs: (coef.name, button)
        self.active_button = None

        group = None
        for coef in xforms.coefficients:
            button = gtk.RadioButton.new_with_label_from_widget(group, coef.name)
            button.connect("toggled", self.ChangeCoef_cb, coef.name)
            (x,y) = coef.grid_loc
            self.attach(button, x, y, 1, 1)
            self.coef_buttons.append((coef.name,button))
            if group is None:
                group = button

    def ChangeCoef_cb(self, button, coef):
        if button.get_active():
            self.active_button = coef
            self.parent.ChangeCoef(coef)

    def GetSelectedCoef(self):
        for (name,button) in self.coef_buttons:
            if button.get_active():
                return name
        return None

    def FinishSetup(self):
        for (name,button) in self.coef_buttons:
            if name == "Tv_bv":
                button.set_active(True)
                return
        
def main():
    opts,args = getopt.getopt(sys.argv[1:], 'd:', ['dir='])
    root_dir = None
    #print(opts, args)
    for opt,arg in opts:
        #print('option = ', opt, ',    arg = ', arg)
        if opt == '-d':
            root_dir = arg
            if not os.path.isdir(root_dir):
                root_dir = os.path.join("/home/IMAGES", root_dir)


    main = XFMainWin()
    main.FinishSetup()
    main.show_all()
    gtk.main()
    return

    if root_dir == None:
        print("usage: make_xforms.py -d /home/IMAGES/1-1-2023")
        sys.exit(-2)

    context.rootdir = root_dir
    astro_db_filename = os.path.join(root_dir, 'astro_db.json')
    if not os.access(astro_db_filename, os.R_OK):
        print("Cannot open ", astro_db_filename)
        sys.exit(-2)

    context.db_obj = astro_db.AstroDB(root_dir)
    context.db = context.db_obj.GetData()

    if 'inst_mags' not in context.db:
        print("Photometry not available in ", astro_db_filename)
        sys.exit(-2)

    target.BuildTargetLists()
    #xforms.LoadExposures(['ru-aql'])
    xforms.LoadExposures([x.name for x in context.target_list.values()])
    #xforms.LoadExposures(['ngc-7790','m67'])
    #xforms.RunOneRegression(xforms.Coefficients('Tb_bv',('B','b'),('B','V'),False))
    xforms.RunOneRegression(xforms.Coefficients('Tbr',('b','r'),('B','R'),True,(0,3)))
    #xforms.LoadExposures(['ngc-7790'])
    #xforms.RunOneRegression(xforms.Coefficients('Tb_bv',('B','b'),('B','V'),False))
    #xforms.LoadExposures(['m67'])
    #coef = xforms.Coefficients('Tb_bv',('B','b'),('B','V'),False)
    #model = xforms.RunOneRegression(coef)
    #xforms.AutoFilter(coef, model)
    #xforms.RunOneRegression(coef)
    #xforms.make_plot(coef)
    return

    c_dict = xforms.RunRegressions()
    for k in c_dict.keys():
        print(k, ": ", c_dict[k])
    cf = coefficients_json.CoefficientFile('/home/ASTRO/CURRENT_DATA/coefficients.json')
    cf.AddNewCoefficientSet(c_dict, root_dir)
    cf.Write()
    
    print("Available targets are:", context.target_list.keys())
    
if __name__ == "__main__":
    main()
