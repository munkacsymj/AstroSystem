import os
import math
import traceback
import statistics
import sys
import gi
from gi.repository import Gdk as gdk, Gtk as gtk, GLib
import cairo

sys.path.insert(1, '/home/mark/ASTRO/CURRENT')
from PYTHON_LIB.ASTRO_DB_LIB import astro_db, astro_directive
from PYTHON_LIB.IMAGE_LIB import star
from PYTHON_LIB.IMAGE_LIB import filter as filter_module

#import context

import comp_analy
import SessionGlobal

background_rgb = (0.9, 0.9, 1.0)
onesigma_rgb_ens = (0.52, 0.73, 0.95)
onesigma_rgb_check = (0.75, 0.75, 0.25)
historical_rgb = (0.0, 1.0, 0.0)
today_rgb = (1.0, 1.0, 0.0)
black_rgb = (0.0, 0.0, 0.0)

class EnsembleMember:
    def __init__(self, starname, star_disp):
        self.name = starname
        self.row_number = None
        self.star_disp = star_disp
        self.errs = {} # indexed by canonical filter name, value is (mag_err,uncty) tuple

        self.is_any_ens = star_disp.is_any_ens
        self.is_any_check = star_disp.is_any_check

        # Setup the errs dictionary for ensembles and checks
        if star_disp.is_any_ens or star_disp.is_any_check:
            for (filter,data) in star_disp.phot.items():
                mag_err = data.mag_err
                uncty = data.uncty
                self.errs[filter] = (mag_err, uncty)
                    
        # Setup the whiskers
        self.whiskers = {} # indexed by filter name, value is list of whiskers
        for (filter,(mag_err,uncty)) in self.errs.items():
            ball_color = 'G' if self.is_any_ens else 'Y'
            print("***whisker: ", self.name, filter, ball_color, mag_err, uncty)
            self.whiskers[filter] = [BallAndWhisker(ball_color, mag_err, uncty)]
        print("+++EnsembleMember: Star ", starname, " has whiskers for ",
              list(self.whiskers.keys()))

        # Grab history data
        if comp_analy.overall_summary.history is not None:
            for h in comp_analy.overall_summary.history:
                data_section = h.ensemble_fit if self.is_any_ens else h.check_fit
                for (filter, fit_data) in data_section.items():
                    if starname in fit_data['errs']:
                        fit_err = fit_data['errs'][starname]
                        stdev = fit_data['stdev'] if self.is_any_ens else fit_data['rms']
                        if filter not in self.whiskers:
                            self.whiskers[filter] = []
                        self.whiskers[filter].append(BallAndWhisker('Z',
                                                                    fit_err,
                                                                    stdev))
                
    # returns pair: (avg_err, rms_scatter)
    def ErrSummary(self, filter):
        if filter not in self.errs:
            return (None, None)
        (mag_err,uncty) = self.errs[filter]
        print("ErrSummary(",self.name,"): ", mag_err, " +/- ",uncty)
        return (mag_err, uncty)

# There's some ugliness in this class, associated with filter_set. The
# filter names provided in filter_set are NOT canonical filter names;
# they're the names the human is used to seeing. But screen labels and
# the self.filter_Labelx dictionaries use the non-canonical name. But
# the rest of comp_analy and even compTAB uses the canonical name,
# instead. Sorry if this causes confusion...
class SummaryBox(gtk.Grid):
    def __init__(self, filter_set):
        super().__init__()
        self.set_column_spacing(10)
        self.set_column_homogeneous(True)
        self.filter_set = filter_set

        self.eLabel = gtk.Label()
        self.eLabel.set_markup("<b>Ensemble</b>")
        self.attach(self.eLabel, 1, 0, 2, 1)
                    
        self.cLabel = gtk.Label()
        self.cLabel.set_markup("<b>Check</b>")
        self.attach(self.cLabel, 3, 0, 2, 1)

        self.t1Label = gtk.Label()
        self.t1Label.set_markup("<b><u>Today</u></b>")
        self.attach(self.t1Label, 1, 1, 1, 1)

        self.h1Label = gtk.Label()
        self.h1Label.set_markup("<b><u>Historical</u></b>")
        self.attach(self.h1Label, 2, 1, 1, 1)

        self.t2Label = gtk.Label()
        self.t2Label.set_markup("<b><u>Today</u></b>")
        self.attach(self.t2Label, 3, 1, 1, 1)

        self.h2Label = gtk.Label()
        self.h2Label.set_markup("<b><u>Historical</u></b>")
        self.attach(self.h2Label, 4, 1, 1, 1)
        
        self.filter_rownum = {} # index = filter, value = grid row number
        self.filter_Label1 = {} # index = filter name, value = gtk.Label
        self.filter_Label2 = {}
        self.filter_Label3 = {}
        self.filter_Label4 = {}

        nextrow = 2

        for f in filter_set:
            #print("Seting up summary for ", f)
            thisrow = nextrow
            self.filter_rownum[f] = nextrow
            nextrow += 1
            self.attach(gtk.Label(label=(f+':')),0, thisrow, 1, 1)
            
            self.filter_Label1[f] = gtk.Label()
            self.attach(self.filter_Label1[f], 1, thisrow, 1, 1)
            self.filter_Label2[f] = gtk.Label()
            self.attach(self.filter_Label2[f], 2, thisrow, 1, 1)
            self.filter_Label3[f] = gtk.Label()
            self.attach(self.filter_Label3[f], 3, thisrow, 1, 1)
            self.filter_Label4[f] = gtk.Label()
            self.attach(self.filter_Label4[f], 4, thisrow, 1, 1)

    def ReLoadLabel(self, filter, label, data):
        first_column_txt = ' '
        if filter in data and data[filter] is not None:
            fmt_txt = '<tt>{:6.3f}</tt>'
            first_column_txt = fmt_txt.format(data[filter])
        label.set_xalign(0.0)
        label.set_markup(first_column_txt)

    def ReLoad(self):
        summary = comp_analy.overall_summary
        for f in self.filter_set:
            filter = filter_module.to_canonical[f]
            self.ReLoadLabel(filter, self.filter_Label1[f], summary.ensemble_rms)
            self.ReLoadLabel(filter, self.filter_Label2[f], summary.ens_history)
            self.ReLoadLabel(filter, self.filter_Label3[f], summary.check_rms)
            self.ReLoadLabel(filter, self.filter_Label4[f], summary.check_history)

class StarData:
    # A new StarData is needed whenever the target star changes
    def __init__(self, target, initial_filter):
        print("+++Creating new StarData for ", target)
        self.target = target
        self.active_filter = initial_filter
        self.scale_min_err = None
        self.scale_max_err = None
        self.catalog = comp_analy.overall_summary.catalog
        self.RefreshData()

    # RefreshData is to be invoked whenever the analysis is
    # updated. For example, after an image is excluded from the analysis
    def RefreshData(self):
        print("+++StarData.RefreshData() starting. Filter is ", self.active_filter)
        # extract the set of ensemble stars from the analysis
        self.active_ensemble = {} # indexed by starname, value is EnsembleMember

        for (starname,x) in comp_analy.overall_summary.stars.items():
            if x.is_any_check or x.is_any_comp or x.is_any_ens:
                em = EnsembleMember(starname, x)
                self.active_ensemble[starname] = em

        print("CompTab: Using ", len(self.active_ensemble), " fitting stars.")

        # Sort everything. Sort first by (V) mag, then sort by (ens,check)
        sorted_em = list(self.active_ensemble.values())
        sorted_em.sort(key = lambda x : x.star_disp.sort_mag)
        sorted_em.sort(key = lambda x : x.is_any_ens, reverse=True)
            
        row_no = 1 # remember that row 0 is special, so we skip it here
        for em in sorted_em:
            em.row_number = row_no
            row_no += 1

        sys.stdout.flush()
        sys.stderr.flush()
        print("+++StarData RefreshData() with ", row_no, " lines.")

        self.for_display = self.active_ensemble # a dictionary,
                                                # indexed by starname
        self.UpdateForFilter()
        print("+++StarData.RefreshData() finished.")

    def SetActiveFilter(self, filter):
        print("+++SetActiveFilter(",filter,")")
        self.active_filter = filter
        self.UpdateForFilter()

    # "which_items" is either "ensemble" or "check"
    # Returns tuple: (one_sigma, min_err, max_err)
    def GetOneSigma(self, which_items):
        if which_items == "ensemble":
            starname_list = comp_analy.overall_summary.ens_stars_active[self.active_filter]
        else:
            starname_list = comp_analy.overall_summary.check_stars_active[self.active_filter]

        err_list = [x.ErrSummary(self.active_filter) for x in self.active_ensemble.values()
                    if x.name in starname_list]
        simple_err_list = []
        min_err = None
        max_err = None
        for (err,uncty) in err_list:
            print("+++GetOneSigma pair is ", (err,uncty))
            if err is None:
                continue
            min0 = err
            max0 = err
            simple_err_list.append(err)
            if uncty is not None:
                min0 = err-uncty
                max0 = err+uncty
            if min_err is None or min0 < min_err:
                min_err = min0
            if max_err is None or max0 > max_err:
                max_err = max0
        print("GetOneSigma(",which_items,") list:", simple_err_list)
        if len(simple_err_list) > 1:
            one_sigma = statistics.stdev(simple_err_list, 0.0)
        else:
            one_sigma = None
        return (one_sigma, min_err, max_err)
            

    def UpdateForFilter(self):
        print("+++StarData.UpdateForFilter() starting. Filter is ",
              self.active_filter)

        for (starname, data) in self.active_ensemble.items():
            for filter in set(data.errs.keys()):
                err = data.errs[filter][0]
                print(starname, data.row_number,
                      data.star_disp.cat_star.IsEnsemble(filter),
                      data.star_disp.cat_star.IsCheck(filter), filter, err)
        
        # Calculate aggregate summaries for this filter
        (sigma_ens,min_ens,max_ens) = self.GetOneSigma('ensemble')
        (sigma_check,min_check,max_check) = self.GetOneSigma('check')
        print('+++SigmaCheck[ens] = ', (sigma_ens,min_ens,max_ens))
        print('+++SigmaCheck[check] = ', (sigma_check,min_check,max_check))

        self.one_sigma_ens = sigma_ens
        self.one_sigma_check = sigma_check

        ################################
        # Minimum scale bounds
        ################################
        if min_ens is None:
            self.scale_min_err = min_check
        elif min_check is None:
            self.scale_min_err = min_ens
        else:
            self.scale_min_err = min(min_ens,min_check)
        ################################
        # Maximum scale bounds
        ################################
        if max_ens is None:
            self.scale_max_err = max_check
        elif max_check is None:
            self.scale_max_err = max_ens
        else:
            self.scale_max_err = max(max_ens,max_check)
            
        print("UpdateForFilter() set max/min to ",
              (self.scale_max_err, self.scale_min_err),
              " and one_sigma to ",
              self.one_sigma_ens, self.one_sigma_check)

    def RefreshStats(self, filter):
        for (starname,ens_member) in self.for_display.items():
            if filter not in ens_member.errs:
                continue        # skip stars with no data for this
            # filter
            for error in ens_member.errs[filter]:
                err = abs(error)
    
        
class CompTAB(gtk.VBox):
    def __init__(self, parent_notebook, tab_label):
        super().__init__(spacing=2)

        top_box = gtk.HBox(spacing=2)
        self.pack_start(top_box, fill=True, expand=False, padding=2)
        filter_list = ["B", "V", "Rc", "Ic"]
        self.summary_box = SummaryBox(filter_list)
        top_box.pack_start(self.summary_box, fill=True, expand=False, padding=2)

        topright_box = gtk.VBox(spacing=2)
        top_box.pack_start(topright_box, fill=False, expand=False, padding=2)
        #self.filter_selector = gtk.ComboBoxText()
        #for f in filter_list:
        #    self.filter_selector.append_text(f)
        #self.filter_selector.set_active(0) # typically, "B"
        #topright_box.pack_start(self.filter_selector, fill=False,
        #                        expand=False, padding=2)
        #self.filter_selector.connect("changed", self.filter_change)

        color_box = gtk.VBox(spacing=2)
        top_box.pack_start(color_box, fill=False, expand=False, padding=2)
        self.filter_b_B = gtk.RadioButton(group=None, label="B")
        self.filter_b_V = gtk.RadioButton(group=self.filter_b_B, label="V")
        self.filter_b_R = gtk.RadioButton(group=self.filter_b_B, label="R")
        self.filter_b_I = gtk.RadioButton(group=self.filter_b_B, label="I")
        self.filter_b_B.connect("toggled", self.filter_change, "B")
        self.filter_b_V.connect("toggled", self.filter_change, "V")
        self.filter_b_R.connect("toggled", self.filter_change, "R")
        self.filter_b_I.connect("toggled", self.filter_change, "I")
        color_box.pack_start(self.filter_b_B, fill=False, expand=False, padding=2)
        color_box.pack_start(self.filter_b_V, fill=False, expand=False, padding=2)
        color_box.pack_start(self.filter_b_R, fill=False, expand=False, padding=2)
        color_box.pack_start(self.filter_b_I, fill=False, expand=False, padding=2)

        bottomrighttop_box = gtk.HBox(spacing=2)
        topright_box.pack_start(bottomrighttop_box, fill=False, expand=False, padding=2)
        star_check_boxes = gtk.VBox(spacing=2)
        self.include_ensembles = gtk.CheckButton(label="Ensemble")
        self.include_ensembles.set_active(True)
        self.include_checks = gtk.CheckButton(label="Checks")
        self.include_checks.set_active(True)
        self.reset_to_catalog = gtk.Button(label="Reset to Catalog")
        self.reset_to_catalog.connect("clicked", self.Reset_cb, None)
        
        star_check_boxes.pack_start(self.include_ensembles, fill=False, expand=False, padding=2)
        star_check_boxes.pack_start(self.include_checks, fill=False, expand=False, padding=2)
        star_check_boxes.pack_start(self.reset_to_catalog, fill=False, expand=False, padding=2)
        bottomrighttop_box.pack_start(star_check_boxes, fill=True, expand=True, padding=2)

        # An important contraint is that the GraphGrid must ALWAYS be
        # the last object in self's container.
        self.graph = None
        self.NewGraphGrid()

        parent_notebook.append_page(self, gtk.Label(label=tab_label))

    def Reset_cb(self, widget, unused):
        astro_db.SafeDirectiveChange(SessionGlobal.RecomputeAllBVRI,
                                     comp_analy.overall_summary.directive_juid,
                                     "set_star_exclusions_ensemble",
                                     ('B',[]))
        astro_db.SafeDirectiveChange(SessionGlobal.RecomputeAllBVRI,
                                     comp_analy.overall_summary.directive_juid,
                                     "set_star_exclusions_ensemble",
                                     ('V',[]))
        astro_db.SafeDirectiveChange(SessionGlobal.RecomputeAllBVRI,
                                     comp_analy.overall_summary.directive_juid,
                                     "set_star_exclusions_ensemble",
                                     ('R',[]))
        astro_db.SafeDirectiveChange(SessionGlobal.RecomputeAllBVRI,
                                     comp_analy.overall_summary.directive_juid,
                                     "set_star_exclusions_ensemble",
                                     ('I',[]))
        astro_db.SafeDirectiveChange(SessionGlobal.RecomputeAllBVRI,
                                     comp_analy.overall_summary.directive_juid,
                                     "set_star_exclusions_check",
                                     ('B',[]))
        astro_db.SafeDirectiveChange(SessionGlobal.RecomputeAllBVRI,
                                     comp_analy.overall_summary.directive_juid,
                                     "set_star_exclusions_check",
                                     ('V',[]))
        astro_db.SafeDirectiveChange(SessionGlobal.RecomputeAllBVRI,
                                     comp_analy.overall_summary.directive_juid,
                                     "set_star_exclusions_check",
                                     ('R',[]))
        astro_db.SafeDirectiveChange(SessionGlobal.RecomputeAllBVRI,
                                     comp_analy.overall_summary.directive_juid,
                                     "set_star_exclusions_check",
                                     ('I',[]))

    # callback connected to the filter selector button up top
    def filter_change(self, w, filter):
        if w.get_active():
            self.star_data.SetActiveFilter(filter)
            #self.star_data.RefreshData()
            self.NewGraphGrid()

    def GetFilter(self):
        filter_info = [ (self.filter_b_B, "B"),
                        (self.filter_b_V, "V"),
                        (self.filter_b_R, "R"),
                        (self.filter_b_I, "I") ]
        filter = next( (f for (b,f) in filter_info if b.get_active()), "B")
        return filter_module.to_canonical[filter]

    def NewGraphGrid(self):
        if self.graph is not None:
            self.graph.destroy()
        self.graph = GraphGrid(self)
        self.pack_start(self.graph, fill=True, expand=True, padding=2)
        # By passing "self" as an arg, we give the graph the ability
        # to fetch all the data from the analysis that the host Tab
        # creates in RefreshData()
        self.graph.SetData(self)
        self.show_all()
        
    def SetTarget(self, target):
        global current_target
        self.target = target
        astro_db.current_target = target
        self.star_data = StarData(target, self.GetFilter())
        self.NewGraphGrid()
        self.summary_box.ReLoad()
        
    #def RefreshData(self, target):
    #    self.star_data.RefreshData()

    def ReRunBVRI(self):
        command = '/home/mark/ASTRO/ASTRO_DB/BIN/do_bvri.py '
        command += ' -t ' + self.target
        command += ' -d ' + SessionGlobal.homedir
        command += ' > /tmp/do_bvri.out 2>&1 '

        print("Running: ", command)
        retval = os.system(command)
        print("^^^Command completed. Return value is ", retval)

    def ReLoadDB(self):
        comp_analy.overall_summary.Reload()
        self.star_data.RefreshData()
        print("^^^Starting NewGraphGrid()")
        self.NewGraphGrid()
        print("^^^Starting summary_box.ReLoad()")
        self.summary_box.ReLoad()

class GraphRow:
    def __init__(self, graphgrid_obj, graph_grid, starname, ensemble_member):
        graphgrid_obj.grid_rows.append(self)
        
        first_column = "<tt>{starname:<14} {chartname:<3} {typename:<5}</tt>"
        fulltype = ensemble_member.star_disp.text_tag
        self.fulltype = fulltype
        if fulltype == "check":
            thistype = 'CHECK'
        elif fulltype == "ensemble":
            thistype = 'ENS'
        else:
            thistype = ' '
        self.thistype = thistype
        chartname = ensemble_member.star_disp.chartname
        if chartname is None:
            chartname = ' '
        first_column_txt = first_column.format(starname=starname,
                                               chartname=chartname,
                                               typename=thistype)
        self.star_label = gtk.Label(label=first_column_txt)
        self.star_label.set_xalign(0.0)
        self.star_label.set_markup(first_column_txt)
        self.starname = starname
        self.filter = graphgrid_obj.comp_tab.GetFilter()
        self.graph_grid = graphgrid_obj
        self.ensemble_member = ensemble_member
        self.row_number = ensemble_member.row_number
        self.enable_button = gtk.CheckButton(label='Include')
        self.enable_button.set_active(self.enable_button)
        if fulltype == 'check':
            if starname not in comp_analy.overall_summary.check_stars_active[self.filter]:
                self.enable_button.set_active(False)
        elif fulltype == 'ensemble':
            if starname not in comp_analy.overall_summary.ens_stars_active[self.filter]:
                self.enable_button.set_active(False)

        self.is_check = ensemble_member.star_disp.cat_star.IsCheck(self.filter)
        self.type_label = gtk.Label('Ens' if not self.is_check else 'Check')

        self.da = GraphArea(self, graphgrid_obj)
        self.da.set_size_request(graphgrid_obj.center_width, graphgrid_obj.row_height)
        print("Completed setup of GraphRow for ", starname)

        graph_grid.attach(self.star_label, 0, self.row_number, 1, 1)
        self.da.set_halign(gtk.Align.FILL)
        self.da.set_valign(gtk.Align.FILL)
        self.da.set_hexpand(True)
        self.da.set_vexpand(False)
        graph_grid.attach(self.da, 1, self.row_number, 1, 1)
        graph_grid.attach(self.enable_button, 2, self.row_number, 1, 1)
        # "thistype" is either CHECK or ENS
        self.enable_button.connect("toggled", graphgrid_obj.on_enable_toggle, thistype)
        self.da.connect("draw", self.on_draw_graph)

        ################################
        # Set background color depending
        # on whether the star was actually
        # used in the check/ensemble.
        ################################
        if fulltype == 'check':
            if starname not in comp_analy.overall_summary.check_stars_active[self.filter]:
                self.star_label.override_background_color(0, gdk.RGBA(0.8,0.8,0.8,1))
        if fulltype == 'ensemble':
            if starname not in comp_analy.overall_summary.ens_stars_active[self.filter]:
                self.star_label.override_background_color(0, gdk.RGBA(0.8,0.8,0.8,1))
            
    # "da" is the GraphArea
    def on_draw_graph(self, da, ctx: cairo.Context):
        #print("+++on_draw_graph() invoked.")
        da.ReDraw(ctx)

        return False
        
class BallAndWhisker:
    # Center and Halfwidth are both in user coordinates
    # Warning: halfwidth might be None
    def __init__(self, style, center, halfwidth):
        self.style = style
        self.center = center
        self.halfwidth = halfwidth # user coords
        self.vert_loc = None
        print("BallAndWhisker: center set to ", center)

    def SetVert(self, vert_loc):
        if self.style != 'Z':
            self.vert_loc = vert_loc
        else:
            self.vert_loc = vert_loc+15

    def ReDraw(self, drawing_area, ctx, parent_grid):
        # if "center" is None (maybe catalog is missing photometry in
        # this color?), do nothing
        if self.center is None:
            return
        middle = parent_grid.UserXToPixels(self.center)

        ################################
        # Draw the center circle
        ################################
        ctx.arc(middle, self.vert_loc, 6.0, 0, 2*math.pi)
        ctx.set_source_rgb(*self.StyleToRGB())
        ctx.fill()

        ################################
        # Draw horiz center bar
        ################################
        if self.halfwidth is not None:
            offset = parent_grid.UserDistToPixels(self.halfwidth)
            x_start = middle - offset
            x_end = middle + offset
        
            ctx.move_to(x_start, self.vert_loc)
            ctx.line_to(x_end, self.vert_loc)
            # left whisker
            ctx.move_to(x_start, self.vert_loc+5)
            ctx.line_to(x_start, self.vert_loc-5)
            # right whisker
            ctx.move_to(x_end, self.vert_loc+5)
            ctx.line_to(x_end, self.vert_loc-5)
            ctx.set_source_rgb(*black_rgb)
            ctx.set_line_width(2.0)
            ctx.stroke()
        
    def StyleToRGB(self):
        if 'G' in self.style:
            return (0, 1.0, 0)
        if 'Y' in self.style:
            return (1.0, 1.0, 0)
        if 'Z' in self.style:
            return (0.5, 0.5, 0.5)
        return (0.5, 0.5, 0.5)
        
class GraphGrid(gtk.ScrolledWindow):
    def __init__(self, comp_tab):
        super().__init__()
        self.scroller = self
        self.scroller.set_policy(gtk.PolicyType.NEVER,
                                 gtk.PolicyType.ALWAYS)
        
        self.grid = gtk.Grid(column_homogeneous=False,
                             row_homogeneous=False,
                             column_spacing=0,
                             row_spacing=0)
        self.scroller.add(self.grid)
        self.top_row_height = 15
        self.left_column_width = 100
        self.right_column_width = 80
        self.row_height = 40
        self.center_width = 600
        self.comp_tab = comp_tab
        self.grid_row_starname = []
        self.vert_line_usercoords = []
        self.vert_line_xcoords = []
        
        # The first grid row is special -- it contains headings and
        # stuff
        self.key_da = gtk.DrawingArea()
        self.key_da.set_size_request(self.center_width, 16) # used to be 20
        self.key_da.set_halign(gtk.Align.FILL)
        self.key_da.set_valign(gtk.Align.FILL)
        self.key_da.set_hexpand(True)
        self.key_da.set_vexpand(False)
        self.grid.attach(self.key_da, 1, 0, 1, 1)
        self.grid.connect("check-resize", self.ReScale, "check_resize")
        self.key_da.connect("size-allocate", self.ReScale, "size_allocate")
        self.ReScale(None, None, "__init__")
        self.key_da.connect("draw", self.on_draw_graphkey)
        self.grid_rows = []

    def ReLoad(self):
        self.queue_draw()

    # ReScale() fetches height and width of the "key" area in row 0 of
    # the grid. This gives us height and width of the drawing area(s)
    # in the center column of the grid.
    #
    # The following attributes are set at the end:
    #    self.cell_width -- pixels
    #    self.cell_height -- pixels
    #    self.zerox  -- x pixel coordinate of the zero line
    #    self.xscale -- mags/pixel
    #    self.vert_line_xcoords -- list of x coords for vert lines
    #
    
    def ReScale(self, widget, ctx, reason):
        print("GraphGrid.ReScale() invoked. ", reason)
        alloc = self.key_da.get_allocation()
        width = alloc.width
        height = alloc.height
        if not hasattr(self.comp_tab, "star_data"):
            print("ReScale(): Tab doesn't yet have star_data.")
            return
        print("ReScale(): Tab has star_data. Continuing.")
        star_data = self.comp_tab.star_data
        if star_data.scale_min_err is None or star_data.scale_max_err is None:
            print("GraphGrid.ReScale(): aborting: missing scale_min/scale_max")
            return
        self.min_userx = star_data.scale_min_err - 0.05*abs(star_data.scale_min_err)
        self.max_userx = star_data.scale_max_err + 0.05*abs(star_data.scale_max_err)
        
        #max_error = 0.029 # needs real computation
        max_error = self.max_userx - self.min_userx
        scale_width = max_error/width
        #scale_width = (0.025 - (-0.05))/width
        # NOTE: self.zerox might be off-screen; self.zerox may be out-of-range
        self.zerox = (0.0 - self.min_userx)/scale_width
        self.xscale = scale_width

        # Choose spacing between gridlines
        spacing_palette = [0.005, 0.01, 0.02, 0.05]
    
        max_num_vert_lines = width/70.0
        print("width = ", width, ", max_num_vert_lines = ",
              max_num_vert_lines, "(max_error = ", max_error, ")")
        chosen_spacing = next((x for x in spacing_palette
                               if (max_error/x) <= max_num_vert_lines),
                              0.1)
        num_vert_lines = 1+int(max_error/chosen_spacing)
        if num_vert_lines % 2 == 0:
            num_vert_lines += 1
        print("num_vert_lines = ", num_vert_lines,
              ", chosen_spacing = ", chosen_spacing)

        max_intervals = int(max((0.0-self.min_userx)/chosen_spacing,
                                (self.max_userx/chosen_spacing)))
        print("max_intervals = ", max_intervals)
        self.vert_line_usercoords = []

        for i in range(max_intervals+1):
            trial = i*chosen_spacing
            if trial >= self.min_userx and trial <= self.max_userx:
                self.vert_line_usercoords.append(trial)
            if i != 0:
                trial = -trial
                if trial >= self.min_userx and trial <= self.max_userx:
                    self.vert_line_usercoords.append(trial)

        vert_line_xcoords = [self.UserXToPixels(x) for x in self.vert_line_usercoords]
        self.vert_line_xcoords = vert_line_xcoords
        self.cell_width = width
        first_row_da = self.grid.get_child_at(1,1)
        if first_row_da is not None:
            alloc = first_row_da.get_allocation()
            self.cell_height = alloc.height

    def UserXToPixels(self, user_x):
        return (user_x - self.min_userx)/self.xscale

    def UserDistToPixels(self, dist):
        try:
            return dist/self.xscale
        except Exception as ex:
            tb = traceback.TracebackException.from_exception(ex, capture_locals=True)
            print("".join(tb.format()))

    def SetData(self, parent_tab):
        self.parent = parent_tab

        # Now set up the graph rows in the grid
        if hasattr(parent_tab, "star_data"):
            # "value" is an EnsembleMember
            self.graph_rows = [GraphRow(self, self.grid, name, value) for (name,value) in
                               parent_tab.star_data.active_ensemble.items()]
        else:
            print("Warning: SetData() doing nothing because star_data not set up yet.")
    
    # Any of the individual star "enable" toggle buttons will trigger this...
    def on_enable_toggle(self, widget, thistype):
        filter = self.comp_tab.GetFilter()
        exclusions = [x.starname for x in self.grid_rows if
                      x.thistype == thistype and
                      not x.enable_button.get_active()]
        #db_obj = astro_db.AstroDB(SessionGlobal.homedir)
        #db = db_obj.GetData()
        #directive_juid = comp_analy.overall_summary.directive_juid
        #directive_dict = astro_db.FindJUID(db, 'directives', directive_juid)
        #type_of_exclusion = "ensemble_excl" if thistype == "ENS" else "check_excl"
        #directive = astro_directive.Directive(db, directive_dict)
        #directive.SetStarExclusions(type_of_exclusion, filter, exclusions)
        #db_obj.UpdateTStamp(directive_dict)
        #db_obj.Write()
        #SessionGlobal.RecomputeAllBVRI()
        #self.comp_tab.ReRunBVRI()
        cmd_string = 'set_star_exclusions_ensemble' if thistype == 'ENS' else 'set_star_exclusions_check'
        print("on_enable_toggle: ", filter, exclusions)
        astro_db.SafeDirectiveChange(SessionGlobal.RecomputeAllBVRI,
                                     comp_analy.overall_summary.directive_juid,
                                     cmd_string,
                                     (filter, exclusions))
        
    # This handles the draw callback on the top grid row (the key)
    def on_draw_graphkey(self, da, ctx):
        #print("on_draw_graphkey() invoked.")
        #print(self.vert_line_xcoords)
        #print(self.vert_line_usercoords)
        for (x, val) in zip(self.vert_line_xcoords,
                            self.vert_line_usercoords):
            ctx.set_font_face(None) # None forces default
            ctx.set_font_size(10.0)
            #ctx.set_font_options(
            ctx.move_to(x-15, 15.0)
            ctx.show_text("{:.3f}".format(val))
            #print("on_draw_graphkey() stroking for ", val)
            ctx.stroke()
        return False

# A GraphArea sits in *one cell* of the GraphGrid
class GraphArea(gtk.DrawingArea):
    def __init__(self, graph_row, parent_grid):
        super().__init__()
        self.graph_row = graph_row
        self.grid = parent_grid

    def ReDraw(self, ctx: cairo.Context):
        filter = self.graph_row.filter
        # The next two statements handle strange things during startup, when the Grid may
        # not have been "show_all"d yet.
        if not hasattr(self.grid.comp_tab, "star_data"):
            return
        if not hasattr(self.grid, "cell_width"):
            return
        star_data = self.grid.comp_tab.star_data

        ################################
        # Draw background box
        ################################
        ctx.rectangle(0, 0, self.grid.cell_width,
                      self.grid.cell_height)
        ctx.set_source_rgb(*background_rgb)
        ctx.fill()

        ################################
        # Draw one-sigma box
        ################################
        if self.graph_row.ensemble_member.star_disp.text_tag == 'check':
            color = onesigma_rgb_check
            if filter in comp_analy.overall_summary.check_rms:
                one_sigma = comp_analy.overall_summary.check_rms[filter]
            else:
                one_sigma = None
        else:
            color = onesigma_rgb_ens
            if filter in comp_analy.overall_summary.ensemble_rms:
                one_sigma = comp_analy.overall_summary.ensemble_rms[filter]
            else:
                one_sigma = None
                
        if one_sigma is not None:
            sigma_1_start = self.grid.UserXToPixels(-one_sigma)
            sigma_1_end = self.grid.UserXToPixels(one_sigma)
        
            ctx.rectangle(sigma_1_start, 0, (sigma_1_end-sigma_1_start),
                          self.grid.cell_height)
            ctx.set_source_rgb(*color)
            ctx.fill()

        ################################
        # Bottom separator line splits rows
        ################################
        ctx.move_to(0, self.grid.cell_height-1)
        ctx.line_to(self.grid.cell_width, self.grid.cell_height-1)
        ctx.set_source_rgb(*black_rgb)
        ctx.set_line_width(2.0)
        ctx.stroke()

        ################################
        # Vertical Lines
        ################################
        for x in self.grid.vert_line_xcoords:
            ctx.move_to(x, 0)
            ctx.line_to(x, self.grid.cell_height)
            ctx.set_source_rgb(*black_rgb)
            ctx.set_line_width(1.0)
            ctx.set_dash([2.0])
            ctx.stroke()
        
        ################################
        # Ball-and-Whiskers
        ################################
        if filter in self.graph_row.ensemble_member.whiskers:
            for w in self.graph_row.ensemble_member.whiskers[filter]:
                w.SetVert(10.0)
                w.ReDraw(self, ctx, self.grid)
        

        
