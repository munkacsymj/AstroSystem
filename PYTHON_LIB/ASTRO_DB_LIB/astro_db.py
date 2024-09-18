import datetime
import time
import threading
import queue
import json
import os
import sys
import errno
import traceback
import gi
gi.require_version('Gtk', '3.0')
from gi.repository import GLib as glib

from .util import FindJUID
from . import astro_directive

coord_queue = queue.Queue()
worker_thread = None
global current_target, refresh_busy
db_homedir = None
current_target = None           # this is set by compTab

juid_types = [
    "session",
    "image",
    "set",
    "analysis",
    "inst_mags",
    "directive",
    "submission",
    "stacks"]

root_juids = {
    "session" : 1000000,
    "image" : 2000000,
    "set" : 5000000,
    "analysis" : 3000000,
    "inst_mags" : 4000000,
    "directive" : 7000000,
    "stacks" : 6000000,
    "submission" : 8000000
    }

top_level_names = {
    "session" : "session",
    "image" : "exposures",
    "stacks" : "stacks",
    "inst_mags" : "inst_mags",
    "analysis" : "analyses",
    "directive" : "directives",
    "submission" : "submissions",
    "set" : "sets"
    }

def SubtreeFindLargestJUID(root):
    if isinstance(root, dict):
        if 'juid' in root:
            return root['juid']
        elif 'JUID' in root:
            return root['JUID']
        return -1
    elif isinstance(root, list):
        if len(root) > 0:
            max_juid = max(SubtreeFindLargestJUID(x) for x in root)
        else:
            max_juid = -1
        return max_juid
    else:
        return -1

def JUIDToTopLevelName(juid):
    juid_base = 1000000*int(juid/1000000)
    juid_type = next((x for x in juid_types if juid_base == root_juids[x]),None)
    if juid_type is None:
        print("JUIDToTopLevelName(", juid, ") failed")
        traceback.print_stack()
        return None
    else:
        return top_level_names[juid_type]
        
        

class AstroDB:
    def __init__(self, directory):
        global db_homedir
        self.astro_db_filename = os.path.join(directory, 'astro_db.json')
        db_homedir = directory
        self.data = None
        self.Reload()

    def GetData(self):
        return self.data

    def Write(self):
        try:
            with open(self.astro_db_filename, 'w') as fp:
                json.dump(self.data, fp, indent=2)
                fp.flush()
                os.sync()
                fp.close()
        except IOError as x:
            print("Error writing astro_db.json: errno = ", x.errno, ', ',
                  x.strerror)
            if x.errno == errno.EACCES:
                print(self.astro_db_filename, " - no write permission")
            elif x.errno == errno.EISDIR:
                print(self.astro_db_filename, " - is a directory")
            
    def GetNextJUID(self, juid_type):
        this_juid = self.next_juid[juid_type]
        if this_juid < 0:
            this_juid = root_juids[juid_type]
        self.next_juid[juid_type] = this_juid+1
        return this_juid

    def GetDirectiveForTarget(self, target_name):
        targets = self.data["sets"]
        this_target = next((x for x in targets if x["stype"] == "TARGET" and
                            x["target"] == target_name), None)
        if this_target is None:
            return None

        input_list_juid = this_target["input"]
        directive_set = set()
        for j in input_list_juid:
            root_name = JUIDToTopLevelName(j)
            target_item = FindJUID(self.data, root_name, j)
            print("FindJUID(", j, ") returned ", target_item)
            if target_item == None:
                print("GetDirectiveForTarget() no match for JUID ", j)
            else:
                if not isinstance(target_item, list):
                    target_item = [target_item]
                print("target_item = ", target_item)
                for t in target_item:
                    if "directive" in t:
                        directive_set.add(t["directive"])
                    else:
                        print(j, " did not lead to directive??")
        if len(directive_set) > 1:
            print("Target ", target_name, " traces to multiple directives??")
        elif len(directive_set) == 0:
            print("Target ", target_name, " traces back to zero directives??")
        else:
            return FindJUID(self.data, "directives", directive_set.pop())

    def Reload(self):
        try:
            with open(self.astro_db_filename, 'r') as fp:
                self.data = json.load(fp)
        except IOError as x:
            if x.errno == errno.ENOENT:
                print(self.astro_db_filename, " - does not exist")
            elif x.errno == errno.EACCES:
                print(self.astro_db_filename, " - cannot be read")
            else:
                print(self.astro_db_filename, " - unknown error")
        except json.decoder.JSONDecodeError as x:
            print(self.astro_db_filename, " - JSON decode error")
            print(x.msg)
            print("Line number ", x.lineno)
            raise
        
        self.next_juid = {}
        for t in juid_types:
            if top_level_names[t] in self.data and len(self.data[top_level_names[t]]) > 0:
                self.next_juid[t] = 1+SubtreeFindLargestJUID(self.data[top_level_names[t]])
            else:
                self.next_juid[t] = -1
            #print(t, len(self.data[top_level_names[t]]))
            #print("Max JUID for ", t, " is ", self.next_juid[t])
        self.BuildImageDictionary()

    def UpdateTStamp(self, section):
        time_now = datetime.datetime.now()
        section['tstamp'] = int(datetime.datetime.timestamp(time_now))

    def BuildImageDictionary(self):
        self.image_dict = {}
        for source in [self.data['exposures'],
                       self.data['stacks']]:
            for image in source:
                if 'juid' not in image:
                    print(image)
                self.image_dict[image['filename']] = image['juid']
                       
    def FindExposureByJUID(self, juid):
        x = FindJUID(self.data, 'exposures', juid)
        if x is None:
            x = FindJUID(self.data, 'stacks', juid)
        return x

    def GetJUIDForImage(self, image_filename):
        return self.image_dict[image_filename]

    # valid command_string_items:
    #   "set_star_exclusions_check" [value is tuple: (filter, starlist)]
    #   "set_star_exclusions_ensemble" [value is tuple: (filter, starlist)]
    #   "set_use_ensemble" [value is boolean]
    #   "set_color_exclusions" [value is list of canonical filter names]
    #   "set_image_analy_exclusions" [ value is list of JUIDs ]
    #   "set_submission" [value is list of dicts in db['submissions']
    #   "shutdown" [value is ignored]
    
def SafeDirectiveChange(user_reload_function, directive_juid, command_string, value):
    global coord_queue
    coord_queue.put((user_reload_function, directive_juid, command_string, value))

def SafeSubmissionChange(user_reload_function, submission_dict_list):
    global coord_queue
    coord_queue.put((user_reload_function, None, "set_submission", submission_dict_list))

def ThreadWorker():
    global coord_queue, db_homedir, current_target
    directive_dict = None
    while True:
        print("ThreadWorker(): blocking wait starting for thread ",
              threading.get_ident(), flush=True)
        (user_reload_function,
         directive_juid,
         command_str,
         value) = coord_queue.get(block=True, timeout=None)
        queue_is_empty = False
        print("ThreadWorker: return from block with cmd: ", command_str, flush=True)
        count = 0
        do_tstamp_update = False
        exec_do_bvri = False
            
        while not queue_is_empty:
            print("ThreadWorker: reloading db_obj from file.", flush=True)
            db_obj = AstroDB(db_homedir)
            db = db_obj.GetData()

            print("Threadworker.queue_is_empty = ", queue_is_empty)
            while not queue_is_empty:
                print("ThreadWorker processing cmd: ", command_str, flush=True)
                if command_str in ['set_star_exclusions_check',
                                   'check_excl',
                                   'set_star_exclusions_ensemble',
                                   'set_use_ensemble',
                                   'set_color_exclusions',
                                   'set_image_analy_exclusions']:
                    directive_dict = FindJUID(db, 'directives', directive_juid)
                    if directive_dict is not None:
                        directive = astro_directive.Directive(db, directive_dict)
                        if command_str == 'set_star_exclusions_check':
                            directive.SetStarExclusions("check_excl", *value)
                            exec_do_bvri = True
                            do_tstamp_update = True
                        elif command_str == 'set_star_exclusions_ensemble':
                            directive.SetStarExclusions("ensemble_excl", *value)
                            exec_do_bvri = True
                            do_tstamp_update = True
                        elif command_str == 'set_use_ensemble':
                            directive.SetUseEnsemble(value)
                            exec_do_bvri = True
                            do_tstamp_update = True
                        elif command_str == 'set_color_exclusions':
                            directive.SetColorExclusions(value)
                            exec_do_bvri = True
                            do_tstamp_update = True
                        elif command_str == 'set_image_analy_exclusions':
                            directive.SetImageAnalyExclusions(value)
                            exec_do_bvri = True
                            do_tstamp_update = True
                elif command_str == 'set_submission':
                    db['submissions'] = value
                elif command_str == 'shutdown':
                    return
                else:
                    print("ThreadWorker: SafeDirectiveChange: invalid command_str = ",
                          command_str, flush=True)
                queue_is_empty = None
                count += 1
                try:
                    (user_reload_function,
                     directive_juid,
                     command_str,
                     value) = coord_queue.get(block=False, timeout=None)
                    queue_is_empty = False
                except queue.Empty:
                    queue_is_empty = True
                if queue_is_empty:
                    time.sleep(0.05) # 50 msec
                    try:
                        (user_reload_function,
                         directive_juid,
                         command_str,
                         value) = coord_queue.get(block=False, timeout=None)
                        queue_is_empty = False
                    except queue.Empty:
                        queue_is_empty = True


            print("ThreadWorker queue is (temporarily) empty.", flush=True)
            if directive_dict is None:
                print("ThreadWorker doing nothing because directive_dict is None.", flush=True)
                continue
            # Now safe to close out the write and let the external program run
            if do_tstamp_update:
                    db_obj.UpdateTStamp(directive_dict)
            db_obj.Write()
            print("ThreadWorker updated db with ", count, " changes.", flush=True)

            if exec_do_bvri:
                command = 'do_bvri '
                command += ' -t ' + current_target
                command += ' -d ' + db_homedir
                command += ' > /tmp/do_bvri.out 2>&1 '

                print("Running: ", command)
                retval = os.system(command)
                print("^^^Command completed. Return value is ", retval)
            
            print("ThreadWorker completed rerun of do_bvri().", flush=True)
            os.sync()
            global refresh_busy
            refresh_busy = True
            glib.idle_add(user_reload_function)
            while refresh_busy:
                time.sleep(0.1)
            print("ThreadWorker completed wait for refresh_busy.", flush=True)
                    
def Init():
    glib.threads_init()
    worker_thread = threading.Thread(target=ThreadWorker)
    worker_thread.daemon = True # ensures clean shutdown
    worker_thread.start()
        

print("ASTRO_DB_LIB version of astro_db.py is running.")
