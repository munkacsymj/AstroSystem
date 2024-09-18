class ModeRole:
    NONE = 0x00
    COMP = 0x01
    CHECK_CAND = 0x02
    ENS_CAND = 0x04
    CHECK_ACT = 0x08
    ENS_ACT = 0x10
    CHECKREF_CAND = 0x20
    CHECKREF_ACT = 0x40

    MODE_COMP = 1
    MODE_ENS = 2
    MODE_ALL = 3
    all_modes = [MODE_COMP, MODE_ENS]

    all_filters = "all"
    standard_filters = ['B', 'V', 'R', 'I']

    def __init__(self):
        self.value = {} # key is filter name, value is dictionary with mode as key
        for f in self.standard_filters:
            self.value[f] = {self.MODE_COMP : self.NONE, self.MODE_ENS : self.NONE }

    def Set(self, filter, mode, role):
        if filter == ModeRole.all_filters:
            for f in self.standard_filters:
                self.Set(f, mode, role)
        else:
            if mode != self.MODE_ALL:
                self.value[filter][mode] |= role
            else:
                for m in self.all_modes:
                    self.Set(filter, m, role)

    def IsComp(self, filter, mode):
        return bool(self.value[filter][mode] & self.COMP)
    def IsCheckCand(self, filter, mode):
        return bool(self.value[filter][mode] & self.CHECK_CAND)
    def IsEnsCand(self, filter, mode):
        return bool(self.value[filter][mode] & self.ENS_CAND)
    def IsCheckAct(self, filter, mode):
        return bool(self.value[filter][mode] & self.CHECK_ACT)
    def IsEnsAct(self, filter, mode):
        return bool(self.value[filter][mode] & self.ENS_ACT)
    def IsCheckRefCand(self, filter, mode):
        return bool(self.value[filter][mode] & self.CHECKREF_CAND)
    def IsCheckRefAct(self, filter, mode):
        return bool(self.value[filter][mode] & self.CHECKREF_ACT)
    
