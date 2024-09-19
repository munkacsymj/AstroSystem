import json
import astro_db

class Directive:
    def __init__(self, astro_db, juid):
        self.juid = juid
        self.astro_db = astro_db
        # search for the directives
        self.directive = None
        for d in self.astro_db.GetData()['directives']:
            if d['juid'] == juid:
                self.directive = d
                break
        if self.directive == None:
            print("ERROR: Directive not found with juid == ", juid)
        if 'do_transform' not in self.directive:
            self.SetDefaults()

    def Write(self):
        self.astro_db.Write()

    def SetDefaults(self):
        self.directive['stack_excl'] = [] # images excluded from stacking
        self.directive['img_analy_excl'] = [] # images excluded from analysis
        self.directive['filter_excl'] = [] # filters excluded from analysis
        self.directive['ensemble_excl'] = [] # star/filter pairs being excluded
        self.directive['check_excl'] = [] # star/filter pairs being excluded
        self.directive['use_ensemble'] = 1
        self.directive['do_transform'] = 1
        self.directive['do_extinction'] = 1
        self.directive['magerr_select'] = 'rms'

    def GetEnsemble(self):
        return self.directive['use_ensemble']

    def GetTransform(self):
        return self.directive['do_transform']

    
