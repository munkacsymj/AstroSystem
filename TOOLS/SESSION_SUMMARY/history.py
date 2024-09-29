#!/usr/bin/python3
import glob
import os
import sys
import logging
import threading
import time

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

files = None
session_history_all = []
history_by_target = {} # index is tgt name, value is list of TargetHistory
total_analysis_count = 0

class TargetHistory:
    def __init__(self, target, analy_db):
        self.target = target
        if 'ensembles' in analy_db:
            self.ensembles = analy_db['ensembles']
        else:
            self.ensembles = None

        if 'ensemble_fit' in analy_db:
            self.ensemble_fit = analy_db['ensemble_fit']
        else:
            self.ensemble_fit = None

        if 'check_fit' in analy_db:
            self.check_fit = analy_db['check_fit']
        else:
            self.check_fit = None

class SessionHistory:
    def __init__(self, db):
        global history_by_target
        global total_analysis_count
        self.targets = []
        self.tgt_history = {} # index by tgt name, value is TargetHistory

        if "analyses" not in db:
            return

        for a in db['analyses']:
            total_analysis_count += 1
            if 'target' not in a:
                pass
                #print("Analysis missing target: ", a)
            else:
                tgt = a['target']
                if tgt in self.tgt_history:
                    print("Error: found multiple analyses for ", tgt)
                else:
                    this_tgt = TargetHistory(tgt, a)
                    self.tgt_history[tgt] = this_tgt
                    self.targets.append(tgt)
                    if tgt not in history_by_target:
                        history_by_target[tgt] = []
                    history_by_target[tgt].append(this_tgt)
                    

history_thread = None
def BeginStartup(current_dir):
    global history_thread
    history_thread = threading.Thread(target=Initialize, args=(current_dir,))
    history_thread.start()

def Initialize(current_dir):
    global files
    global session_history_all

    files = glob.glob('/home/IMAGES/*/astro_db.json')
    files = [x for x in files if current_dir not in x]

    #print("Number of databases to read: ", len(files))

    for f in files:
        print("History processing file ", f)
        db = astro_db.AstroDB(os.path.dirname(f))
        this_session = SessionHistory(db.data)
        session_history_all.append(this_session)
        #print("finished fetching history from ", f)

def WaitForStartupComplete():
    global history_thread
    history_thread.join()

if __name__ == "__main__":
    ExecutePhase1("5-30-2023")
    ExecutePhase2()

    print("history_by_target contains data on ",
          len(history_by_target),
          " different targets.")
    print("session_history_all contains data from ",
          len(session_history_all),
          " different sessions.")
    print(total_analysis_count,
          " different analyses were found.")


    
    
