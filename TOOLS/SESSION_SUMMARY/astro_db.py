import json
import os
import errno

juid_types = [
    "session",
    "image",
    "set",
    "analysis",
    "inst_mags",
    "directive",
    "stacks"]

root_juids = {
    "session" : 1000000,
    "image" : 2000000,
    "set" : 5000000,
    "analysis" : 3000000,
    "inst_mags" : 4000000,
    "directive" : 7000000,
    "stacks" : 6000000
    }

top_level_names = {
    "session" : "session",
    "image" : "exposures",
    "stacks" : "stacks",
    "inst_mags" : "inst_mags",
    "analysis" : "analyses",
    "directive" : "directives",
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
        

class AstroDB:
    def __init__(self, directory):
        self.astro_db_filename = os.path.join(directory, 'astro_db.json')
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
        self.next_juid = {}
        for t in juid_types:
            self.next_juid[t] = SubtreeFindLargestJUID(self.data[top_level_names[t]])
            print("Max JUID for ", t, " is ", self.next_juid[t])

    def GetData(self):
        return self.data

    def Write(self):
        try:
            with open(self.astro_db_filename, 'w') as fp:
                json.dump(self.data, fp, indent=2)
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

print("Session_Summary version of astro_db.py is running.")
import traceback
traceback.print_stack()
