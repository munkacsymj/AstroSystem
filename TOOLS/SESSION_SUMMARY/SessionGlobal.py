import sys
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

homedir = '/home'
csv_summary_file = '/home'
log_summary_file = '/home'
shell_summary_file = '/home'
current_dark_file = ""
current_dark_data = []
current_image_name = None
global_summary = None
current_star = None
chart_window = None
image_region = None # FITSViewer for raw images
stack_region = None # FITSViewer for stacked images
stack_files = {} # dictionary indexed by single-letter color
thumbs = None
text_tabs = None
stacker = None
root = None
root_r = None
db_obj = None
db = None                       # db is the actual astro_db inside db_obj
image_xy_pick = None # use notifier("image_xy_pick")

all_targets = []
star_dictionary = {}

################################
# Notifiers in use:
#
# "current_star" (value_change)
# "stack_filesX" (value_change, content_change)
# "overall_summary" (value_change)
#
################################

class Notifier_element:
    def __init__(self):
        self.requestor = None
        self.variable = None
        self.callback = None
        self.condition = None
        self.debug = None
        self.data = None

class Notifier:
    def __init__(self):
        self.elements = []

    def register(self, requestor, variable, condition, callback, data=None, debug=None):
        ne = Notifier_element();
        ne.requestor = requestor
        ne.variable = variable
        ne.callback = callback
        ne.condition = condition
        ne.debug = debug
        ne.data = data

        self.elements.append(ne)

        if debug != None:
            print("Notifier.register(", debug, "): ", variable)
            print("   (number of Notifier.elements= ", len(self.elements))

    def trigger(self, trigger_source, variable, condition):
        print("Trigger ", variable, condition)
        print("   (number of Notifier.elements= ", len(self.elements))
        for e in self.elements:
            print(". . . checking ", e.variable, " & ", e.condition)
            if e.variable == variable and e.condition == condition:
                print(". . . match")
                if e.debug != None:
                    print("SessionGlobal.Notifier: triggering debug: ", e.debug)
                e.callback(e.variable, e.condition, e.data)
            else:
                print(". . . no match")

# Used for changes to current_star
notifier = Notifier()

def RecomputeAllBVRI():
    global db_obj
    import traceback
    print("RecomputeAllBVRI() invoked. Callback:")
    traceback.print_stack()
    if db_obj.GetData() is not None:
        text_tabs.comp_tab.ReLoadDB() # this triggers aavso_report
        text_tabs.bvri_tab.refresh_file_quiet()
        text_tabs.bvri_tab.SetEnsembleAndExcludeButton()
    print("RecomputeAllBVRI(): setting refresh_busy to False.")
    astro_db.refresh_busy = False

def ReloadReport():
    global db_obj, text_tabs
    print("ReloadReport() invoked.")
    if db_obj.GetData() is not None:
        for filter in ['B', 'V', 'R', 'I']:
            text_tabs.report_tab.ReloadSubmissions(filter)
            text_tabs.report_tab.Refresh_color(filter)
            text_tabs.report_tab.RefreshBottomLines()

    print("ReloadReport(): setting refresh_busy to False.")
    astro_db.refresh_busy = False
