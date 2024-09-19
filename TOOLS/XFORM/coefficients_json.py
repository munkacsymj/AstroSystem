import datetime
import os
import json
import statistics
import pdb

class CoefficientFile:
    def __init__(self, filename):
        self.filename = filename
        if not os.path.exists(self.filename):
            print("Creating ", self.filename)
            self.CreateFileTemplate()
        self.Reload()

    def CreateFileTemplate(self):
        self.data = { "active" : { "sessions" : [] } ,
                      "sets" : []
                     }
        self.Write()

    def Reload(self):
        try:
            with open(self.filename, 'r') as fp:
                self.data = json.load(fp)
        except IOError as x:
            if x.errno == errno.ENOENT:
                print(self.filename, " - does not exist")
            elif x.errno == errno.EACCES:
                print(self.filename, " - cannot be read")
            else:
                print(self.filename, " - unknown error")
        except json.decoder.JSONDecodeError as x:
            print(self.filename, " - JSON decode error")
            print(x.msg)
            print("Line number ", x.lineno)
            raise

    def AddNewCoefficientSet(self, coefficients, session_dir, merge_into_current=True):
        #pdb.set_trace()
        time_now = datetime.datetime.now()
        date_dir = os.path.basename(session_dir)

        for one_entry in self.data['sets']:
            if one_entry['session'] == date_dir:
                self.data['sets'].remove(one_entry)
                break
            
        new_entry =  { "timestamp" : int(datetime.datetime.timestamp(time_now)),
                       "session" : date_dir,
                       "coefficients" : coefficients }
        self.data['sets'].append(new_entry)

        if merge_into_current:
            self.AddToMerge(date_dir)

    def AddToMerge(self, date_dir):
        #pdb.set_trace()
        date_list = self.data['active']['sessions']
        if date_dir not in date_list:
            date_list.append(date_dir)
        self.BuildMerge(date_list)

    def BuildMerge(self, datelist):
        #pdb.set_trace()
        selected_sets = [x for x in self.data['sets'] if x['session'] in datelist]
        coefficient_set = {x for s in selected_sets for x in s['coefficients'].keys() }
        print("Coefficient_set = ", coefficient_set)

        time_now = datetime.datetime.now()

        new_merge = { "sessions" : [x['session'] for x in selected_sets],
                      "timestamp" : int(datetime.datetime.timestamp(time_now)),
                      "coefficients" : {}
                      }

        self.data['active'] = new_merge

        for coefficient in coefficient_set:
            coef_values = [s['coefficients'][coefficient]['value']
                           for s in selected_sets
                           if coefficient in s['coefficients'].keys()]
            new_value = statistics.mean(coef_values)
            if len(coef_values) > 1:
                new_err = statistics.stdev(coef_values)
            else:
                new_err = next((s['coefficients'][coefficient]['err']
                                for s in selected_sets),None)

            new_merge['coefficients'][coefficient] = { "value" : new_value,
                                                       "err" : new_err }
        self.Write()

    def Write(self):
        try:
            with open(self.filename, 'w') as fp:
                json.dump(self.data, fp, indent=2)
                fp.flush()
                os.sync()
                fp.close()
        except IOError as x:
            print("Error writing ", self.filename, ": errno = ", x.errno, ', ',
                  x.strerror)
            if x.errno == errno.EACCES:
                print(self.filename, " - no write permission")
            elif x.errno == errno.EISDIR:
                print(self.filename, " - is a directory")
            
        
